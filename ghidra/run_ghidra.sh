#!/usr/bin/env bash
#
# run_ghidra.sh - headless Ghidra disassembly + decompilation of the two
# Kotilampo UV-EPROM banks, producing C output for comparison against the
# hand reconstruction in ../disasm/koti_lampo.c
#
# Ghidra ships the 8048 (MCS-48) processor natively, so no extension build is
# needed. The two 2k ROMs form the 8048's 4k program space:
#   bin/3EF2H.bin -> bank 0, base 0x0000
#   bin/A98EH.bin -> bank 1, base 0x0800
#
# Usage:
#   GHIDRA_INSTALL_DIR=/usr/share/ghidra ./ghidra/run_ghidra.sh
#
# If GHIDRA_INSTALL_DIR is unset, the script tries common install locations.

set -euo pipefail

# --- locate analyzeHeadless ------------------------------------------------
HEADLESS=""
if [[ -n "${GHIDRA_INSTALL_DIR:-}" && -x "$GHIDRA_INSTALL_DIR/support/analyzeHeadless" ]]; then
    HEADLESS="$GHIDRA_INSTALL_DIR/support/analyzeHeadless"
else
    for c in \
        /usr/share/ghidra/support/analyzeHeadless \
        /opt/ghidra/support/analyzeHeadless \
        "$(command -v analyzeHeadless 2>/dev/null || true)"; do
        if [[ -n "$c" && -x "$c" ]]; then HEADLESS="$c"; break; fi
    done
fi
if [[ -z "$HEADLESS" ]]; then
    echo "error: analyzeHeadless not found. Install Ghidra and/or set GHIDRA_INSTALL_DIR." >&2
    exit 1
fi
echo "Using: $HEADLESS"

# --- paths -----------------------------------------------------------------
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$REPO/bin"
OUT="$REPO/ghidra/out"
PROJ="$(mktemp -d)/ghidra_proj"
mkdir -p "$OUT" "$PROJ"

# 8048 language id as registered by Ghidra's built-in processor module.
LANG="8048:LE:16:default"

analyze_bank () {
    local file="$1" base="$2" name="$3"
    echo "=== $name ($file @ $base) ==="
    "$HEADLESS" "$PROJ" KotilampoGhidra \
        -import "$BIN/$file" \
        -processor "$LANG" \
        -loader BinaryLoader \
        -loader-baseAddr "$base" \
        -scriptPath "$REPO/ghidra" \
        -postScript DecompileToC.py "$OUT/$name.ghidra.c" \
        -overwrite \
        -deleteProject
}

analyze_bank 3EF2H.bin 0x0000 bank0_3EF2H
analyze_bank A98EH.bin 0x0800 bank1_A98EH

echo
echo "Done. Ghidra C output in: $OUT/"
echo "Compare against the hand reconstruction: $REPO/disasm/koti_lampo.c"
