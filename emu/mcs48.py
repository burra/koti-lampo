#!/usr/bin/env python3
"""
MCS-48 (Intel 8035/8048) emulator + disassembler for the Valmet Kotilämpö firmware.

Why this exists
---------------
`disasm/koti_lampo.c` is a *hand-written* reconstruction of what the firmware
does. Roughly half of its claims are tagged LIKELY or GUESS rather than SURE,
and until now nothing tested any of them — yet the ESP32 replacement plan
(Route B in `docs/db25_replacement.md`) treats that reconstruction as its
specification. This runs the real ROM instead, so claims can be checked against
observed behaviour rather than trusted.

It is deliberately not MAME. MAME has an MCS-48 core but is driver-based (no
"run this bare ROM" mode), and the whole point here is deep instrumentation:
every port write, every 8243 expander operation together with the P2 strobe that
selects the chip, every external-RAM access. That is easier to build than to
retrofit.

Trusting the emulator
---------------------
An emulator that is subtly wrong is worse than none, because it manufactures
false confidence. So the decoder is not trusted, it is *proven*: `d48` already
emitted the raw bytes of every instruction it decoded, which pins down the
instruction boundaries and lengths across both ROM banks. `--selftest` walks
those and fails loudly on any disagreement. Run it before believing any trace.

The execution core beyond decode (arithmetic, flags, banking, stack) is not
covered by that self-test and remains "reviewed, not proven".

Usage
-----
    ./emu/mcs48.py --selftest            # prove the decoder against d48
    ./emu/mcs48.py --trace 200           # trace N instructions from reset
    ./emu/mcs48.py --trace 200 --ports   # only port/expander/extRAM activity
"""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# Disassembler
# ---------------------------------------------------------------------------

# Instructions taking a second byte. Everything else is one byte.
_TWO_BYTE = {
    0x03, 0x13, 0x23, 0x43, 0x53, 0xD3,          # ADD/ADDC/MOV/ORL/ANL/XRL A,#data
    0x88, 0x89, 0x8A, 0x98, 0x99, 0x9A,          # ORL/ANL BUS,P1,P2 ,#data
    0xB0, 0xB1,                                  # MOV @Rn,#data
    0x12, 0x32, 0x52, 0x72, 0x92, 0xB2, 0xD2, 0xF2,   # JBb
    0x16, 0x26, 0x36, 0x46, 0x56, 0x86, 0x96, 0xB6,   # JTF/JNT0/JT0/JNT1/JT1/JNI/JNZ/JF0
    0x76, 0xC6, 0xE6, 0xF6,                      # JF1/JZ/JNC/JC
}
_TWO_BYTE |= {0x04 | (p << 5) for p in range(8)}   # JMP
_TWO_BYTE |= {0x14 | (p << 5) for p in range(8)}   # CALL
_TWO_BYTE |= {0xB8 + r for r in range(8)}          # MOV Rn,#data
_TWO_BYTE |= {0xE8 + r for r in range(8)}          # DJNZ Rn,addr

_SIMPLE = {
    0x00: "nop",    0x01: "idl",  0x02: "outl bus,a",  0x05: "en i",     0x07: "dec a",
    0x08: "ins a,bus", 0x09: "in a,p1",  0x0A: "in a,p2",
    0x0C: "movd a,p4", 0x0D: "movd a,p5", 0x0E: "movd a,p6", 0x0F: "movd a,p7",
    0x10: "inc @r0", 0x11: "inc @r1",    0x15: "dis i",     0x17: "inc a",
    0x20: "xch a,@r0", 0x21: "xch a,@r1", 0x25: "en tcnti", 0x27: "clr a",
    0x30: "xchd a,@r0", 0x31: "xchd a,@r1", 0x35: "dis tcnti", 0x37: "cpl a",
    0x39: "outl p1,a", 0x3A: "outl p2,a",
    0x3C: "movd p4,a", 0x3D: "movd p5,a", 0x3E: "movd p6,a", 0x3F: "movd p7,a",
    0x40: "orl a,@r0", 0x41: "orl a,@r1", 0x42: "mov a,t", 0x45: "strt cnt",
    0x47: "swap a",
    0x50: "anl a,@r0", 0x51: "anl a,@r1", 0x55: "strt t",  0x57: "da a",
    0x60: "add a,@r0", 0x61: "add a,@r1", 0x62: "mov t,a", 0x65: "stop tcnt",
    0x67: "rrc a",
    0x70: "addc a,@r0", 0x71: "addc a,@r1", 0x75: "ent0 clk", 0x77: "rr a",
    0x80: "movx a,@r0", 0x81: "movx a,@r1", 0x83: "ret",
    0x85: "clr f0",
    0x8C: "orld p4,a", 0x8D: "orld p5,a", 0x8E: "orld p6,a", 0x8F: "orld p7,a",
    0x90: "movx @r0,a", 0x91: "movx @r1,a", 0x93: "retr", 0x95: "cpl f0",
    0x97: "clr c",
    0x9C: "anld p4,a", 0x9D: "anld p5,a", 0x9E: "anld p6,a", 0x9F: "anld p7,a",
    0xA0: "mov @r0,a", 0xA1: "mov @r1,a", 0xA3: "movp a,@a", 0xA5: "clr f1",
    0xA7: "cpl c",
    0xB3: "jmpp @a", 0xB5: "cpl f1",
    0xC5: "sel rb0", 0xC7: "mov a,psw",
    0xD0: "xrl a,@r0", 0xD1: "xrl a,@r1", 0xD5: "sel rb1", 0xD7: "mov psw,a",
    0xE3: "movp3 a,@a", 0xE5: "sel mb0", 0xE7: "rl a",
    0xF0: "mov a,@r0", 0xF1: "mov a,@r1", 0xF5: "sel mb1", 0xF7: "rlc a",
}

_COND = {
    0x12: "jb0", 0x32: "jb1", 0x52: "jb2", 0x72: "jb3",
    0x92: "jb4", 0xB2: "jb5", 0xD2: "jb6", 0xF2: "jb7",
    0x16: "jtf", 0x26: "jnt0", 0x36: "jt0", 0x46: "jnt1", 0x56: "jt1",
    0x76: "jf1", 0x86: "jni", 0x96: "jnz", 0xB6: "jf0",
    0xC6: "jz", 0xE6: "jnc", 0xF6: "jc",
}

_IMM = {0x03: "add", 0x13: "addc", 0x23: "mov", 0x43: "orl", 0x53: "anl", 0xD3: "xrl"}
_PORT_IMM = {0x88: "orl bus", 0x89: "orl p1", 0x8A: "orl p2",
             0x98: "anl bus", 0x99: "anl p1", 0x9A: "anl p2"}

# Register-embedded groups: base opcode -> mnemonic template
_REG_GROUPS = [
    (0x18, "inc r{r}"), (0x28, "xch a,r{r}"), (0x48, "orl a,r{r}"),
    (0x58, "anl a,r{r}"), (0x68, "add a,r{r}"), (0x78, "addc a,r{r}"),
    (0xA8, "mov r{r},a"), (0xC8, "dec r{r}"), (0xD8, "xrl a,r{r}"),
    (0xF8, "mov a,r{r}"),
]


def insn_length(op):
    return 2 if op in _TWO_BYTE else 1


def disassemble(rom, addr):
    """Return (text, length). Text style approximates d48 for comparison."""
    op = rom[addr]
    b2 = rom[addr + 1] if addr + 1 < len(rom) else 0

    if op in _SIMPLE:
        return _SIMPLE[op], 1
    for base, tmpl in _REG_GROUPS:
        if base <= op < base + 8:
            return tmpl.format(r=op - base), 1
    if op in _IMM:
        return f"{_IMM[op]} a,#{b2:02x}h", 2
    if op in _PORT_IMM:
        return f"{_PORT_IMM[op]},#{b2:02x}h", 2
    if op in _COND:
        return f"{_COND[op]} X{b2:02x}", 2
    if (op & 0x1F) == 0x04:                      # JMP
        return f"jmp X{((op & 0xE0) << 3) | b2:03x}", 2
    if (op & 0x1F) == 0x14:                      # CALL
        return f"call X{((op & 0xE0) << 3) | b2:03x}", 2
    if 0xB8 <= op < 0xC0:
        return f"mov r{op - 0xB8},#{b2:02x}h", 2
    if 0xE8 <= op < 0xF0:
        return f"djnz r{op - 0xE8},X{b2:02x}", 2
    if op in (0xB0, 0xB1):
        return f"mov @r{op - 0xB0},#{b2:02x}h", 2
    return f"db {op:02x}h", 1


# ---------------------------------------------------------------------------
# Emulator core
# ---------------------------------------------------------------------------

class MCS48:
    """
    8035 core: 64 bytes internal RAM, 4K external program memory in two banks,
    external data memory via MOVX, ports P1/P2/BUS, and 8243 expander ports
    P4-P7 reached through MOVD/ANLD/ORLD.

    Port inputs default to 0xFF (pulled high, nothing driving) unless a hook is
    installed via `on_read`. Every port operation is appended to `self.trace`.
    """

    def __init__(self, rom):
        if len(rom) != 4096:
            raise ValueError(f"expected a 4K program image, got {len(rom)}")
        self.rom = rom
        self.ram = bytearray(64)          # internal RAM (regs, stack, scratch)
        self.xram = bytearray(256)        # external data memory via MOVX
        self.a = 0
        self.pc = 0
        self.psw = 0x08                   # bit3 reads as 1; low 3 bits = SP
        self.dbf = 0                      # program memory bank (SEL MB0/MB1)
        self.f1 = 0
        self.timer = 0
        self.tf = 0                       # timer overflow flag
        self.timer_run = False
        self.counter_run = False
        self.irq_enabled = False
        self.int_pin = 1          # INT is active LOW on the 8048
        self.int_pending = False
        self.int_count = 0
        self.int_every = 0        # assert INT every N instructions
        self.tcnti_enabled = False
        self.in_isr = False
        self.cycles = 0
        self.icount = 0
        # port output latches; quasi-bidirectional pins idle high
        self.p1 = 0xFF
        self.p2 = 0xFF
        self.bus = 0xFF
        self.expander = [0x0F] * 4        # P4..P7 output nibbles
        self.t0 = 1
        self.t1 = 1
        self.on_read = {}                 # name -> callable(cpu) -> value
        self.trace = []
        self.halted_at = None

    # -- flags / registers ------------------------------------------------
    @property
    def carry(self):
        return (self.psw >> 7) & 1

    @carry.setter
    def carry(self, v):
        self.psw = (self.psw & 0x7F) | ((v & 1) << 7)

    @property
    def ac(self):
        return (self.psw >> 6) & 1

    @ac.setter
    def ac(self, v):
        self.psw = (self.psw & 0xBF) | ((v & 1) << 6)

    @property
    def f0(self):
        return (self.psw >> 5) & 1

    @f0.setter
    def f0(self, v):
        self.psw = (self.psw & 0xDF) | ((v & 1) << 5)

    @property
    def bank(self):
        return (self.psw >> 4) & 1

    @property
    def sp(self):
        return self.psw & 7

    @sp.setter
    def sp(self, v):
        self.psw = (self.psw & 0xF8) | (v & 7)

    def _rbase(self):
        return 0x18 if self.bank else 0x00

    def reg(self, r):
        return self.ram[self._rbase() + r]

    def setreg(self, r, v):
        self.ram[self._rbase() + r] = v & 0xFF

    # -- port plumbing ----------------------------------------------------
    def _log(self, kind, name, value, extra=""):
        self.trace.append((self.icount, self.pc_insn, kind, name, value, extra))

    def _read_port(self, name):
        hook = self.on_read.get(name)
        val = hook(self) if hook else 0xFF
        self._log("rd", name, val)
        return val & 0xFF

    def _write_port(self, name, value):
        value &= 0xFF
        if name == "p1":
            self.p1 = value
        elif name == "p2":
            self.p2 = value
        elif name == "bus":
            self.bus = value
        self._log("wr", name, value)

    def _expander(self, opname, idx, nibble=None):
        """
        8243 access. P2's low nibble carries the address/command the expander
        latched, and (on this board) the high nibble is the chip-select strobe
        the firmware sets up beforehand -- so log P2 alongside, since that
        pairing is exactly what identifies which physical P8243 is addressed.
        """
        pname = f"p{4 + idx}"
        if opname == "movd_r":
            hook = self.on_read.get(pname)
            val = (hook(self) if hook else 0x0F) & 0x0F
            self._log("exp_rd", pname, val, f"p2={self.p2:02x}")
            return val
        cur = self.expander[idx]
        if opname == "movd_w":
            new = nibble
        elif opname == "orld":
            new = cur | nibble
        elif opname == "anld":
            new = cur & nibble
        else:
            raise AssertionError(opname)
        self.expander[idx] = new & 0x0F
        self._log(f"exp_{opname}", pname, new & 0x0F,
                  f"p2={self.p2:02x} nib={nibble:x}")
        return None

    # -- stack ------------------------------------------------------------
    def _push(self):
        base = 0x08 + self.sp * 2
        self.ram[base] = self.pc & 0xFF
        self.ram[base + 1] = ((self.pc >> 8) & 0x0F) | (self.psw & 0xF0)
        self.sp = (self.sp + 1) & 7

    def _pop(self, restore_psw):
        self.sp = (self.sp - 1) & 7
        base = 0x08 + self.sp * 2
        hi = self.ram[base + 1]
        self.pc = ((hi & 0x0F) << 8) | self.ram[base]
        if restore_psw:
            self.psw = (self.psw & 0x0F) | (hi & 0xF0)

    # -- arithmetic -------------------------------------------------------
    def _add(self, val, with_carry=False):
        cin = self.carry if with_carry else 0
        total = self.a + val + cin
        self.ac = 1 if ((self.a & 0x0F) + (val & 0x0F) + cin) > 0x0F else 0
        self.carry = 1 if total > 0xFF else 0
        self.a = total & 0xFF

    # -- execution --------------------------------------------------------
    def step(self):
        """Execute one instruction. Returns the PC it was fetched from."""
        self._service_timer()
        self._service_interrupt()
        self.pc_insn = self.pc
        op = self.rom[self.pc]
        self.pc = (self.pc + 1) & 0xFFF
        n = 2 if op in _TWO_BYTE else 1
        b2 = 0
        if n == 2:
            b2 = self.rom[self.pc]
            self.pc = (self.pc + 1) & 0xFFF
        self.icount += 1
        self.cycles += n
        self._exec(op, b2)
        return self.pc_insn

    def _page_target(self, b2):
        """Conditional jumps / DJNZ stay in the page of the following byte."""
        return (self.pc & 0xF00) | b2

    def _exec(self, op, b2):
        a = self.a

        # --- register-embedded groups ---
        for base, kind in ((0x18, "inc"), (0x28, "xch"), (0x48, "orl"),
                           (0x58, "anl"), (0x68, "add"), (0x78, "addc"),
                           (0xA8, "movto"), (0xC8, "dec"), (0xD8, "xrl"),
                           (0xF8, "movfrom")):
            if base <= op < base + 8:
                r = op - base
                if kind == "inc":
                    self.setreg(r, self.reg(r) + 1)
                elif kind == "dec":
                    self.setreg(r, self.reg(r) - 1)
                elif kind == "xch":
                    v = self.reg(r); self.setreg(r, self.a); self.a = v
                elif kind == "orl":
                    self.a |= self.reg(r)
                elif kind == "anl":
                    self.a &= self.reg(r)
                elif kind == "xrl":
                    self.a ^= self.reg(r)
                elif kind == "add":
                    self._add(self.reg(r))
                elif kind == "addc":
                    self._add(self.reg(r), True)
                elif kind == "movto":
                    self.setreg(r, self.a)
                elif kind == "movfrom":
                    self.a = self.reg(r)
                return

        if 0xB8 <= op < 0xC0:
            self.setreg(op - 0xB8, b2); return
        if 0xE8 <= op < 0xF0:
            r = op - 0xE8
            self.setreg(r, self.reg(r) - 1)
            if self.reg(r) != 0:
                self.pc = self._page_target(b2)
            return

        # --- @Ri indirect (internal RAM) ---
        if op in (0x10, 0x11):
            i = self.reg(op - 0x10) & 0x3F
            self.ram[i] = (self.ram[i] + 1) & 0xFF; return
        if op in (0x20, 0x21):
            i = self.reg(op - 0x20) & 0x3F
            v = self.ram[i]; self.ram[i] = self.a; self.a = v; return
        if op in (0x30, 0x31):
            i = self.reg(op - 0x30) & 0x3F
            self.ram[i] = (self.ram[i] & 0xF0) | (self.a & 0x0F)
            self.a = (self.a & 0xF0) | (self.ram[i] & 0x0F); return
        if op in (0x40, 0x41):
            self.a |= self.ram[self.reg(op - 0x40) & 0x3F]; return
        if op in (0x50, 0x51):
            self.a &= self.ram[self.reg(op - 0x50) & 0x3F]; return
        if op in (0x60, 0x61):
            self._add(self.ram[self.reg(op - 0x60) & 0x3F]); return
        if op in (0x70, 0x71):
            self._add(self.ram[self.reg(op - 0x70) & 0x3F], True); return
        if op in (0xA0, 0xA1):
            self.ram[self.reg(op - 0xA0) & 0x3F] = self.a; return
        if op in (0xB0, 0xB1):
            self.ram[self.reg(op - 0xB0) & 0x3F] = b2; return
        if op in (0xD0, 0xD1):
            self.a ^= self.ram[self.reg(op - 0xD0) & 0x3F]; return
        if op in (0xF0, 0xF1):
            self.a = self.ram[self.reg(op - 0xF0) & 0x3F]; return

        # --- external data memory ---
        if op in (0x80, 0x81):
            addr = self.reg(op - 0x80)
            self.a = self.xram[addr]
            self._log("movx_rd", f"x{addr:02x}", self.a); return
        if op in (0x90, 0x91):
            addr = self.reg(op - 0x90)
            self.xram[addr] = self.a
            self._log("movx_wr", f"x{addr:02x}", self.a); return

        # --- immediate ALU ---
        if op == 0x03: self._add(b2); return
        if op == 0x13: self._add(b2, True); return
        if op == 0x23: self.a = b2; return
        if op == 0x43: self.a |= b2; return
        if op == 0x53: self.a &= b2; return
        if op == 0xD3: self.a ^= b2; return

        # --- ports ---
        if op == 0x02: self._write_port("bus", self.a); return
        if op == 0x08: self.a = self._read_port("bus"); return
        if op == 0x09: self.a = self._read_port("p1"); return
        if op == 0x0A: self.a = self._read_port("p2"); return
        if op == 0x39: self._write_port("p1", self.a); return
        if op == 0x3A: self._write_port("p2", self.a); return
        if op == 0x88: self._write_port("bus", self.bus | b2); return
        if op == 0x89: self._write_port("p1", self.p1 | b2); return
        if op == 0x8A: self._write_port("p2", self.p2 | b2); return
        if op == 0x98: self._write_port("bus", self.bus & b2); return
        if op == 0x99: self._write_port("p1", self.p1 & b2); return
        if op == 0x9A: self._write_port("p2", self.p2 & b2); return

        # --- 8243 expander ---
        if 0x0C <= op <= 0x0F:
            self.a = self._expander("movd_r", op - 0x0C); return
        if 0x3C <= op <= 0x3F:
            self._expander("movd_w", op - 0x3C, self.a & 0x0F); return
        if 0x8C <= op <= 0x8F:
            self._expander("orld", op - 0x8C, self.a & 0x0F); return
        if 0x9C <= op <= 0x9F:
            self._expander("anld", op - 0x9C, self.a & 0x0F); return

        # --- accumulator ops ---
        if op == 0x07: self.a = (self.a - 1) & 0xFF; return
        if op == 0x17: self.a = (self.a + 1) & 0xFF; return
        if op == 0x27: self.a = 0; return
        if op == 0x37: self.a ^= 0xFF; return
        if op == 0x47: self.a = ((self.a << 4) | (self.a >> 4)) & 0xFF; return
        if op == 0x57:                                   # DA A
            if (self.a & 0x0F) > 9 or self.ac:
                self.a += 6
                if self.a > 0xFF:
                    self.carry = 1
                self.a &= 0xFF
            if ((self.a >> 4) & 0x0F) > 9 or self.carry:
                self.a = (self.a + 0x60) & 0xFF
                self.carry = 1
            return
        if op == 0x67:                                   # RRC A
            c = self.carry; self.carry = self.a & 1
            self.a = ((self.a >> 1) | (c << 7)) & 0xFF; return
        if op == 0x77: self.a = ((self.a >> 1) | ((self.a & 1) << 7)) & 0xFF; return
        if op == 0xE7: self.a = ((self.a << 1) | (self.a >> 7)) & 0xFF; return
        if op == 0xF7:                                   # RLC A
            c = self.carry; self.carry = (self.a >> 7) & 1
            self.a = ((self.a << 1) | c) & 0xFF; return

        # --- program-memory reads ---
        if op == 0xA3:
            self.a = self.rom[(self.pc & 0xF00) | self.a]; return
        if op == 0xE3:
            self.a = self.rom[(self.dbf << 11) | 0x300 | self.a]; return
        if op == 0xB3:
            self.pc = (self.pc & 0xF00) | self.a; return

        # --- flags / control ---
        if op == 0x97: self.carry = 0; return
        if op == 0xA7: self.carry ^= 1; return
        if op == 0x85: self.f0 = 0; return
        if op == 0x95: self.f0 ^= 1; return
        if op == 0xA5: self.f1 = 0; return
        if op == 0xB5: self.f1 ^= 1; return
        if op == 0xC5: self.psw &= 0xEF; return
        if op == 0xD5: self.psw |= 0x10; return
        if op == 0xE5: self.dbf = 0; return
        if op == 0xF5: self.dbf = 1; return
        if op == 0xC7: self.a = self.psw | 0x08; return
        if op == 0xD7: self.psw = self.a | 0x08; return
        if op == 0x00: return
        if op == 0x01:
            # undefined on the NMOS 8035; d48 names it IDL (a CMOS 80C48 op).
            # Reaching it means we are executing data, not code.
            self.halted_at = self.pc_insn
            raise NotImplementedError(
                f"executed undefined opcode 01 (idl) at {self.pc_insn:03x} "
                "- almost certainly data, not code")
        if op == 0x05: self.irq_enabled = True; return
        if op == 0x15: self.irq_enabled = False; return
        if op == 0x25: self.tcnti_enabled = True; return
        if op == 0x35: self.tcnti_enabled = False; return
        if op == 0x42: self.a = self.timer; return
        if op == 0x62: self.timer = self.a; return
        if op == 0x45: self.counter_run = True; self.timer_run = False; return
        if op == 0x55: self.timer_run = True; self.counter_run = False; return
        if op == 0x65: self.timer_run = self.counter_run = False; return
        if op == 0x75: return                            # ENT0 CLK

        # --- control transfer ---
        if (op & 0x1F) == 0x04:
            self.pc = (self.dbf << 11) | ((op & 0xE0) << 3) | b2; return
        if (op & 0x1F) == 0x14:
            self._push()
            self.pc = (self.dbf << 11) | ((op & 0xE0) << 3) | b2; return
        if op == 0x83: self._pop(False); return
        if op == 0x93:
            self._pop(True); self.in_isr = False; return

        # --- conditional jumps ---
        if op in _COND:
            m = _COND[op]
            taken = False
            if m.startswith("jb"):
                taken = bool(self.a & (1 << int(m[2])))
            elif m == "jtf":
                taken = bool(self.tf); self.tf = 0
            elif m == "jnt0": taken = not self.t0
            elif m == "jt0":  taken = bool(self.t0)
            elif m == "jnt1": taken = not self.t1
            elif m == "jt1":  taken = bool(self.t1)
            elif m == "jnz":  taken = self.a != 0
            elif m == "jz":   taken = self.a == 0
            elif m == "jc":   taken = bool(self.carry)
            elif m == "jnc":  taken = not self.carry
            elif m == "jf0":  taken = bool(self.f0)
            elif m == "jf1":  taken = bool(self.f1)
            elif m == "jni":  taken = (self.int_pin == 0) or self.int_pending
            if taken:
                self.pc = self._page_target(b2)
            return

        raise NotImplementedError(
            f"undefined opcode {op:02x} at {self.pc_insn:03x}")

    def _service_interrupt(self):
        """
        External interrupt: vector 0x003, PC+PSW pushed, further interrupts
        inhibited until RETR. On this board INT is the mains zero-cross tick,
        so `--int-every N` stands in for that periodic wake-up.
        """
        if self.int_every and self.icount and self.icount % self.int_every == 0:
            self.int_pending = True
        if not (self.int_pending and self.irq_enabled and not self.in_isr):
            return
        self.int_pending = False
        self.in_isr = True
        self.int_count += 1
        self._log("INT", "vec3", self.int_count, f"from {self.pc:03x}")
        self._push()
        self.pc = 0x003

    def _service_timer(self):
        if self.timer_run and self.cycles >= 32:
            self.cycles -= 32
            self.timer = (self.timer + 1) & 0xFF
            if self.timer == 0:
                self.tf = 1

    # -- helpers ----------------------------------------------------------
    def run(self, n, detect_hang=True):
        """
        Run up to n instructions, stopping early only on a *deliberate* hang.

        Care is needed here: `djnz rN,<self>` is the standard MCS-48 delay idiom
        and jumps to its own address, so "PC did not advance" is NOT a hang
        signal. Only an unconditional JMP to itself is (that is how this
        firmware implements its fault lock-up).
        """
        for _ in range(n):
            pc = self.step()
            if detect_hang and self.pc == pc and (self.rom[pc] & 0x1F) == 0x04:
                # A jmp-to-self is only *terminal* if nothing can break it. The
                # firmware's idle loop is exactly this shape and is meant to be
                # broken by the mains-tick interrupt, so keep running when an
                # interrupt is still able to arrive.
                if self.int_every and self.irq_enabled:
                    self.halted_at = pc          # note it, but keep going
                    continue
                self.halted_at = pc
                return


def load_rom():
    lo = (ROOT / "bin" / "3EF2H.bin").read_bytes()   # bank 0
    hi = (ROOT / "bin" / "A98EH.bin").read_bytes()   # bank 1
    return bytes(lo + hi)


# ---------------------------------------------------------------------------
# Decoder self-test against the committed d48 disassemblies
# ---------------------------------------------------------------------------

# NB: the byte field must stop at the tab that precedes d48's ASCII column.
# d48 renders that column with the high bit stripped (0xb8 -> '8'), so a
# 2-byte instruction can be followed by something that looks like more hex.
_D48_LINE = re.compile(r"^(?P<text>.*?);\s*(?P<addr>[0-9a-f]{4})\s*-\s*"
                       r"(?P<bytes>[0-9a-f]{2}(?: [0-9a-f]{2})*)(?:\t|\s*$)")


def parse_d48(path):
    """Yield (addr, [bytes], mnemonic) for each decoded instruction."""
    for raw in Path(path).read_text(errors="replace").splitlines():
        m = _D48_LINE.match(raw)
        if not m:
            continue
        text = m.group("text")
        if ":" in text:
            text = text.split(":", 1)[1]
        text = text.strip()
        if not text:
            continue
        mnem = text.split()[0].lower()
        addr = int(m.group("addr"), 16)
        data = [int(b, 16) for b in m.group("bytes").split()]
        yield addr, data, mnem


def selftest():
    rom = load_rom()
    banks = [("3EF2H.d48", 0x000), ("A98EH.d48", 0x800)]
    total = fails = 0
    problems = []

    for fname, base in banks:
        path = ROOT / "disasm" / fname
        for addr, data, mnem in parse_d48(path):
            # d48 disassembles each 2K bank independently from offset 0
            phys = base + addr
            if phys >= len(rom):
                continue
            total += 1
            # 1. the bytes d48 recorded must match the ROM image
            actual = list(rom[phys:phys + len(data)])
            if actual != data:
                fails += 1
                problems.append(f"{fname} {addr:04x}: ROM bytes "
                                f"{['%02x' % b for b in actual]} != d48 {['%02x' % b for b in data]}")
                continue
            # 2. our decoded length must match d48's byte count
            text, length = disassemble(rom, phys)
            if length != len(data):
                fails += 1
                problems.append(f"{fname} {addr:04x}: length {length} != "
                                f"d48 {len(data)}  ({text} vs {mnem})")
                continue
            # 3. our mnemonic must match d48's
            ours = text.split()[0].lower()
            if ours != mnem:
                fails += 1
                problems.append(f"{fname} {addr:04x}: mnemonic '{ours}' != "
                                f"d48 '{mnem}'  (bytes {['%02x' % b for b in data]})")

    print(f"decoder self-test: {total - fails}/{total} instructions agree with d48")
    if problems:
        print(f"\n{len(problems)} disagreement(s):")
        for p in problems[:40]:
            print("  " + p)
        if len(problems) > 40:
            print(f"  ... and {len(problems) - 40} more")
        return 1
    print("PASS - decode layer matches the independent d48 disassembly "
          "on every instruction in both banks.")
    return 0


# ---------------------------------------------------------------------------
# Boot trace
# ---------------------------------------------------------------------------

def apply_inputs(cpu, specs, t0, t1):
    """
    Install constant-value read hooks, e.g. p7=0b holds P7 bit 2 low.

    Real inputs are driven by the backend across the opto barrier; pinning them
    is how we ask "what does the firmware do when the plant says X?".
    """
    for spec in specs or []:
        if "=" not in spec:
            raise SystemExit(f"--input expects NAME=HEX, got {spec!r}")
        name, val = spec.split("=", 1)
        name = name.strip().lower()
        if name not in ("p1", "p2", "bus", "p4", "p5", "p6", "p7"):
            raise SystemExit(f"unknown port {name!r}")
        v = int(val, 16)
        cpu.on_read[name] = (lambda v: (lambda _cpu: v))(v)
    if t0 is not None:
        cpu.t0 = t0
    if t1 is not None:
        cpu.t1 = t1


def trace(n, ports_only, inputs=None, t0=None, t1=None, int_every=0):
    rom = load_rom()
    cpu = MCS48(rom)
    apply_inputs(cpu, inputs, t0, t1)
    cpu.int_every = int_every
    cpu.run(n)

    if not ports_only:
        where = (f"halted (self-loop) at {cpu.halted_at:03x}"
                 if cpu.halted_at is not None else "no hang detected")
        print(f"ran {cpu.icount} instructions; {where}; "
              f"{cpu.int_count} interrupt(s) serviced")
        print(f"A={cpu.a:02x} PSW={cpu.psw:02x} DBF={cpu.dbf} "
              f"P1={cpu.p1:02x} P2={cpu.p2:02x} "
              f"P4-P7={[f'{v:x}' for v in cpu.expander]}")
        print()

    print(f"{'#':>6}  {'pc':>4}  {'op':<9} {'port':<6} {'val':>4}  detail")
    print("-" * 62)
    for icount, pc, kind, name, val, extra in cpu.trace:
        print(f"{icount:6}  {pc:03x}   {kind:<9} {name:<6} "
              f"{val:>4x}  {extra}")
    print(f"\n{len(cpu.trace)} port/expander/extRAM operations")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--selftest", action="store_true",
                    help="prove the decoder against the committed d48 output")
    ap.add_argument("--trace", type=int, metavar="N",
                    help="run N instructions from reset and dump I/O activity")
    ap.add_argument("--ports", action="store_true",
                    help="with --trace: show only the I/O table")
    ap.add_argument("--input", action="append", metavar="PORT=HEX",
                    help="hold a port's read value, e.g. --input p7=0b "
                         "(clears bit 2, the fault/interlock line). Repeatable.")
    ap.add_argument("--t0", type=int, choices=(0, 1),
                    help="state of the T0 test pin (cold-boot detect)")
    ap.add_argument("--t1", type=int, choices=(0, 1),
                    help="state of the T1 test pin")
    ap.add_argument("--int-every", type=int, metavar="N", default=0,
                    help="assert the external interrupt every N instructions, "
                         "standing in for the mains zero-cross tick")
    args = ap.parse_args()

    if args.selftest:
        return selftest()
    if args.trace:
        return trace(args.trace, args.ports, args.input, args.t0,
                     args.t1, args.int_every)
    ap.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
