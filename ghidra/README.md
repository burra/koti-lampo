# Ghidra reverse-engineering of the Kotilampo ROMs

Ghidra ships the **8048 (MCS-48)** processor natively (added in
[NSA/ghidra#638](https://github.com/NationalSecurityAgency/ghidra/pull/638),
later improved by [#4825](https://github.com/NationalSecurityAgency/ghidra/pull/4825)
which added `SEL MB0/MB1` bank handling). So **no third-party extension or
Gradle build is required** — the built-in `8048` language handles these dumps.

The two 2k ROMs form the 8048's 4k program space:

| ROM | Bank | Base address |
| --- | --- | --- |
| `bin/3EF2H.bin` | bank 0 | `0x0000` |
| `bin/A98EH.bin` | bank 1 | `0x0800` |

## Run it

```bash
# Install Ghidra (Arch/Manjaro)
sudo pacman -S ghidra

# Headless import + auto-analysis + decompile-to-C of both banks
GHIDRA_INSTALL_DIR=/usr/share/ghidra ./ghidra/run_ghidra.sh
```

Committed Ghidra C output lives in `ghidra/out/`:

- `bank0_3EF2H.ghidra.c` — bank 0 alone (35 functions)
- `combined_4k.ghidra.c` — both banks loaded into one 4k space (70 functions)

The **combined** file is the useful one: bank 1 has no reset vector, so analysed
in isolation the auto-analyzer finds no functions. Loading both banks together
lets analysis start at the reset vector and follow the `SEL MB1` cross-bank
calls into bank 1, recovering every function with real `0x8xx` addresses.

Compare against the hand reconstruction in
[`../disasm/koti_lampo.c`](../disasm/koti_lampo.c).

## Files

- `run_ghidra.sh` — locates `analyzeHeadless`, imports bank 0 and a combined
  4k image with the `8048:LE:16:default` language, runs analysis, and invokes
  the decompiler post-script.
- `DecompileToC.java` — Ghidra post-script that decompiles every discovered
  function and writes the C to the given output file. (Java, not Python:
  Ghidra 12 dropped Jython and only runs Python under PyGhidra.)

## Notes

- The decompiler quality for MCS-48 is limited; expect the Ghidra C to be more
  mechanical than the annotated `disasm/koti_lampo.c`. The value is the
  **cross-check**: where the two disagree, one of them is wrong.
- Ghidra keeps both banks in one 4k address space, so cross-bank `call`/`jmp`
  targets resolve to real `0x8xx` addresses (unlike the per-bank `d48` output).
- If `8048:LE:16:default` is not the exact registered language id in your Ghidra
  version, list the available ids with:
  `$GHIDRA_INSTALL_DIR/support/sleigh -? 2>/dev/null` or check the GUI's
  "Select Language" dialog and adjust `LANG` in `run_ghidra.sh`.
