# Valmet Kotilämpö

A preservation and backup effort to keep a several-decades-old Valmet Kotilämpö
solar-heating HVAC controller running, and eventually build a modern replacement.


## Current status — what works today

Confirmed working:

* ✅ **Firmware dumped from both units.** The B2716-6 UV-EPROMs were read out of
  both control units, and both units contain **byte-for-byte identical**
  firmware (same SHA-256 — see [`bin/checksum.txt`](bin/checksum.txt)). The two
  2K dumps are committed as [`bin/3EF2H.bin`](bin/3EF2H.bin) and
  [`bin/A98EH.bin`](bin/A98EH.bin).
* ✅ **Runs on a modern 28C16.** The original PCBs have been confirmed to run
  correctly with the firmware written into the easier-to-source, still-available
  **28C16** EEPROM — a working drop-in replacement for the now-scarce B2716-6.
  This means a failed EPROM no longer blocks keeping the system running.

Still open (see [Hypothesis](#hypothesis) below): the DB25 pinout, a redrawn
schematic/PCB, and an ESP32-based CPU replacement.


## Hypothesis

* ~~Make backups of the 2st B2716-6 EEPROM's from the spare pcb and write into 28C16 or 27C16 proms~~
  *  ~~Read out prom from the original and compare if they match in checksum, Yes they do~~
  *  Disassemble the code from the proms ? 
* Make pinout diagram of the DB25 connector on the back to make it possible to replace with a total different cpu and motherboard 
* Draw Schema/PCB in order to be able to make a new PCB if the old ones can't be repaired any more 
* Create drop in replacement in the cpu socket with e.g and ESP32
  

## UV-Eprom
UV-Eproms start to be rare and expensive as they where released in the beginning of 1980 and the intel series I couldn't find where to buy 

* [Relatively-Universal-ROM-Programmer](https://github.com/AndersBNielsen/Relatively-Universal-ROM-Programmer)
* [Software](https://github.com/henols/firestarter_app)
* [Firmware](https://github.com/henols/firestarter)
* [The Collector’s guide to Vintage Intel Microchips](https://deramp.com/downloads/mfe_archive/050-Component%20Specifications/Intel/VintageIntelMicrochipsRev4.pdf) 

```bash
# install firestarter
pipx install firestarter
# program the programmer / arduino uno, this may fail first time when arduino is totally new (Should debug and make case if I get time)
firestarter -v fw --install
# avrdude -c arduino -P /dev/ttyUSB0  -b 115200 -p atmega328p -D -U flash:w:firestarter_uno.hex:i
```


### Read from UV-Eprom B2716-6
 
```bash
# On manjaro you may have to run to set rights for the usb port when you plug it in
# sudo chown $USER /dev/ttyUSB0
# Read out the software from the chips
firestarter read M2716 3EF2H.bin
firestarter verify M2716 3EF2H.bin

firestarter read M2716 A98EH.bin
firestarter verify M2716 A98EH.bin 

# Verify the committed dumps against the recorded SHA-256 hashes (no hardware needed)
cd bin && sha256sum -c checksum.txt && cd ..
```

### Find a compatible EEprom replacement
As this chipset is not found easily on the market one have to find a replacement that the read function is the same 

```bash
# Find the chipset or the one that is the same specification
firestarter search 2716
# This will give you and pinout to compare the chipset you are thinking is possible replacement 
firestarter info M2716
```

### Clear EEprom and verify it's empty
```bash
# Some EEprom support to be erased but I haven't found any 
# firestarter erase AM27C512
# Create a empty file with 16k bits of data 
dd if=/dev/zero bs=1 count=2048 | tr '\0' '\377' > empty.bin 
firestarter write CAT28C16A empty.bin
firestarter verify CAT28C16A empty.bin
```
### Write UV-Eprom that is bigger 

Happened to have lying around from the past M2732AF6 so tried to program these also.   

```bash
# Make binary bigger so software don't complain
dd if=/dev/zero bs=1 count=2048 | tr '\0' '\377' >> 3EF2H_32k.bin
# Tune up voltage to max with turning potentiometer RV1 to 21V in my case 17.36v for max
firestarter vpp #VPP: 17.36v, Internal VCC: 4.90v
firestarter vpe #VPE: 22.10v, Internal VCC: 4.92v   
firestarter -v write  --vpe-as-vpp M2732A 3EF2H_32k.bin
```

### Write to EEprom CAT28C16A

```bash
# On manjaro you may have to run to set rights for the usb port when you plug it in
# sudo chown $USER /dev/ttyUSB0
firestarter write CAT28C16A 3EF2H.bin
firestarter verify CAT28C16A 3EF2H.bin

firestarter write CAT28C16A A98EH.bin
firestarter verify CAT28C16A A98EH.bin
```

## Disassemble with d52

The CPU is an Intel **P8035L** (MCS-48 family, the ROM-less 8048), so the ROM
dumps contain MCS-48 machine code. [d52](https://aur.archlinux.org/packages/d52) ships
`d48`, a dedicated 8048/8041 disassembler.

```bash
# On Manjaro / Arch (AUR)
yay -S d52

# Disassemble each 2k bank. -d adds address + raw bytes in the comment field.
d48 -d -b 3EF2H.bin   # -> 3EF2H.d48
d48 -d -b A98EH.bin   # -> A98EH.d48
```

Bank order (the two 2k ROMs make up the 8048's 4k program space, selected with
`SEL MB0` / `SEL MB1`):

| ROM | Bank | Range | Contents |
| --- | --- | --- | --- |
| `3EF2H.bin` | bank 0 | `0x000-0x7FF` | reset/interrupt vectors, port I/O, keypad/display scan, regulation logic |
| `A98EH.bin` | bank 1 | `0x800-0xFFF` | multi-byte BCD math library + control state machine |

`3EF2H` is bank 0 because it holds the reset vector at `0x000` and the
external-interrupt vector (`jmp` at `0x003`). The disassemblies live in
[`disasm/`](disasm/), alongside a hand-written C reconstruction
([`disasm/koti_lampo.c`](disasm/koti_lampo.c)) that annotates the logic with the
original ROM addresses.

> Note: `d48` disassembles each 2k bank independently, so cross-bank `call`/`jmp`
> targets in bank 1 show as `X0xxx` when they really live at `0x8xx`. For a
> labelled 4k address space, load it into [ghidra](https://github.com/NationalSecurityAgency/ghidra)
> (see below).

## Disassemble with Ghidra

Ghidra is free and ships the **8048 (MCS-48)** processor natively, so no
third-party extension or Gradle build is required. Unlike `d48`, it keeps both
banks in one 4k address space, so cross-bank `call`/`jmp` targets resolve to
real `0x8xx` addresses.

```bash
# Install Ghidra (Arch/Manjaro)
sudo pacman -S ghidra

# Headless import + auto-analysis + decompile-to-C of both banks
GHIDRA_INSTALL_DIR=/usr/share/ghidra ./ghidra/run_ghidra.sh
```

The script loads bank 0 alone and a combined 4k image (`3EF2H.bin` + `A98EH.bin`).
The combined run is the useful one: bank 1 has no reset vector, so analysed in
isolation the auto-analyzer finds no functions; loading both banks together lets
analysis start at the reset vector and follow the `SEL MB1` cross-bank calls,
recovering every function with real `0x8xx` addresses.

Committed Ghidra C output and full workflow notes live in
[`ghidra/`](ghidra/) (see [`ghidra/README.md`](ghidra/README.md)). The value is
the **cross-check** against the hand reconstruction
([`disasm/koti_lampo.c`](disasm/koti_lampo.c)): where the two disagree, one of
them is wrong. Comparing the two is what surfaced the internal- vs
external-RAM (`mov @r` vs `movx @r`) modelling fix in the reconstruction.

## Datasheets

Components below are taken from the `Display blockschema.odg` board block diagram.

| Description | IC |
| --- | --- |
| EEPROM 16 Kbit (2K×8), UV-erasable | [B2716-6](https://www.alldatasheet.com/datasheet-pdf/pdf/22809/STMICROELECTRONICS/M2716.html) |
| CPU, MCS-48 family (ROM-less 8048) | [P8035L](https://en.wikipedia.org/wiki/Intel_MCS-48) |
| 8-bit I/O expander (DIP 24) | [P8243](https://www.alldatasheet.com/datasheet-pdf/pdf/164277/INTEL/P8243.html) |
| Dual 4-input NAND gate (CMOS 4012) | [MC14012B](https://www.onsemi.com/pdf/datasheet/mc14012b-d.pdf) |
| Dual type-D flip-flop (CMOS 4013) | [MC14013B](https://www.onsemi.com/pdf/datasheet/mc14013b-d.pdf) |
| 12-bit binary counter (CMOS 4040) | [MC14040B](https://www.onsemi.com/pdf/datasheet/mc14040b-d.pdf) |
| Dual 4-bit latch / databus driver (CMOS 4508) | [MC14508B](https://www.onsemi.com/pdf/datasheet/mc14508b-d.pdf) |
| Optoisolator, transistor output | [4N26](https://www.onsemi.com/pdf/datasheet/4n26-d.pdf) |
| EEPROM (non-volatile settings store) | VA2101 |






