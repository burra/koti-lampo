# `emu/` — MCS-48 emulator for validating the firmware reconstruction

[`mcs48.py`](mcs48.py) runs the real ROM dumps on an emulated Intel 8035 with
every port operation instrumented.

## Why

[`../disasm/koti_lampo.c`](../disasm/koti_lampo.c) is a *hand-written*
reconstruction — 26 claims tagged SURE, 25 LIKELY, 12 GUESS. The ESP32
replacement plan ([Route B](../docs/db25_replacement.md#replacement-options))
treats it as the specification, but nothing tested it. This executes the actual
firmware so claims can be checked instead of trusted.

Not MAME: MAME has an MCS-48 core but is driver-based (no "run this bare ROM"
mode, so it would need a custom machine driver built from source), and the point
here is instrumentation MAME does not readily expose — every port write, every
8243 expander operation *together with* the `P2` strobe in force, every
external-RAM access.

## Trusting it

An emulator that is subtly wrong is worse than none, because it manufactures
false confidence. So the decoder is **proven, not assumed**: `d48` already
recorded the raw bytes of every instruction it decoded, which pins down
instruction boundaries and lengths across both banks. The self-test walks all of
them and compares bytes, length, and mnemonic:

```bash
./emu/mcs48.py --selftest
# decoder self-test: 2314/2314 instructions agree with d48
```

**Scope of that guarantee:** it proves the *decode* layer only. The execution
core (arithmetic, flags, banking, stack, timer) is reviewed but not proven, so
treat behavioural results as strong evidence, not proof. Two of the bugs found
so far were in this harness rather than the firmware (see Notes).

## Usage

```bash
./emu/mcs48.py --selftest                      # prove the decoder
./emu/mcs48.py --trace 400000                  # trace from reset
./emu/mcs48.py --trace 400000 --input p7=0b    # hold a plant input low
./emu/mcs48.py --trace 400000 --t0 0           # warm-start path
```

`--input PORT=HEX` pins what a port reads, which is how you ask "what does the
firmware do when the plant says X?". `--t0/--t1` set the test pins.

## What it has confirmed

Boot from reset, with the interlock input held low (`--input p7=0b`), reaches
the firmware's idle loop in 5428 instructions.

| Reconstruction claim | Was | Result |
| --- | --- | --- |
| `T0` = cold-boot detect (reset @ X0000) | LIKELY | **Confirmed, and sharpened.** `T0`=1 runs a full external-RAM wipe (`0xFF`→`0x00`) before init; `T0`=0 skips it. The two boot paths differ by exactly that wipe. |
| `io_init()` @ X000d drives `P7 = 0x00` | — | **Confirmed.** `ORLD` P7/P6/P5 to `0x0F`, then `ANLD` P7/P6 to `0x00`. |
| `P2[7:4]` high nibble = expander strobe, constants `8F 9F AF CF DF EF` | SURE | **All six observed.** `8F/9F/AF` during `expander_kick()` @ X0029, `CF/DF/EF` later. |
| `P1.5` (`0x20`) = strobe around conversions | LIKELY | **Confirmed and extended** — see below. |
| Idles at X07d8 until the next interrupt tick | LIKELY | **Confirmed.** Execution reaches `X07d8: jmp X07d8` immediately after `en i`, exactly as reconstructed. |

### New finding: the measurement is event-counting on `T1`

At `X01F4` the firmware does `orl p1,#20h` (asserts the `P1.5` strobe), runs a
nested `djnz` delay of 65536 iterations, then executes **`strt cnt`** — which
starts the 8035's counter on the **`T1`** pin.

So a sensor reading is: *assert strobe → wait → count edges arriving on `T1`*.
The measurement is **pulse/event-count encoded, not pulse-width or analog**.
That matters directly for Route B: it is independent evidence for the
["no analog crosses the DB25"](../docs/db25_replacement.md) conclusion, and it
tells you what an ESP32 must actually reproduce — a strobe output plus an edge
counter with a matching gate window.

### New finding: `P6`+`P7` are one 8-bit port pair, not two separate signals

Driving the mains-tick interrupt (`--int-every N`) gets the ISR running, and it
immediately exercises two routines that the reconstruction does not name:

```
X068f:  mov a,#0ffh / outl p2,a   ; select
        mov a,r1 / orld p7,a      ; low nibble of R1 -> P7
        cpl a    / anld p7,a
        mov a,r1 / swap a
        orld p6,a                 ; high nibble of R1 -> P6
        cpl a    / anld p6,a

X0509:  mov a,#0afh / outl p2,a   ; select
        movd a,p6 / swap a / mov r7,a
        movd a,p7 / orl a,r7      ; P6:P7 recombined into one byte
```

So **`P7` carries the low nibble and `P6` the high nibble of a single 8-bit
value**, in both directions — written under the `0xFF` strobe and read back under
`0xAF`. The `orld` / `cpl` / `anld` pair is the standard idiom for forcing a port
to an *exact* nibble when the instruction set only offers set-bits and
clear-bits.

This corrects the signal map in `../docs/db25_replacement.md`, which lists them
as unrelated (`P7` = "relay output latch", `P6` = "expander output shadow"). They
are two halves of one port, so anything reading or driving one without the other
is only seeing half the word — directly relevant to Route B.

### Note on `P2`'s low nibble

Every one of the 35 expander accesses observed had `P2`'s low nibble = `0xF`;
only the high nibble varied. Read carefully: on the 8048 the `MOVD`/`ANLD`/`ORLD`
instructions drive `P20-P23` **in hardware** during the expander cycle, so this
does *not* show the 8243 command bus is unused. What it does show is that the
firmware never uses the low nibble for its own signalling — it writes 1s and
leaves it to the instruction — which supports reading the high nibble as the
chip-select strobe. Whether these chips are clocked conventionally still depends
on the physical `CS'`/`PROG` wiring, which emulation cannot settle.

## Limitations / next steps

- **The ISR stalls on a peripheral that never answers.** With `--int-every` the
  interrupt is serviced and the ISR starts polling the `P6`:`P7` port pair, but
  pinned static inputs never change, so it loops. Going further needs a *model*
  of whatever it is talking to (or a live capture) — this is the natural boundary
  of what emulation alone can settle, and the reason a passive capture on the
  real board remains worth doing.
- **`T1` edges are not modelled**, so `strt cnt` counts nothing. Feeding a
  synthetic pulse train would let the measurement scaling be recovered, and
  compared against the Pt100 curve and the manual's calibration points
  (`TSTAV` 60 °C, test resistors 52 °C / 70 °C).
- External RAM is modelled as a flat 256 bytes; the real board has two
  `MCM51L01P45` devices whose actual decode has not been verified here.

## Notes

Bugs found so far, all in this harness rather than the firmware — recorded
because each was initially mistaken for a firmware discovery:

1. **d48 parser over-capture.** `d48` renders its ASCII column with the high bit
   stripped, so bytes `b8 34` print an ASCII column of literally `84` — which a
   greedy hex regex ate as a third byte. MCS-48 has no 3-byte instructions,
   which was the tell.
2. **False hang detection.** `djnz rN,<self>` is the standard MCS-48 delay idiom
   and jumps to its own address, so "PC did not advance" is not a hang signal.
   This briefly looked like the pin-23 fault lock-up. Only an unconditional
   `jmp` to itself is a real hang — which is how the firmware's genuine fault
   path and idle loop are both written.
