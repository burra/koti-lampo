# DB25 connector & modern-replacement notes

The Valmet Kotilämpö is split into two physically separate halves joined by a
single multi-conductor cable terminated in a **DB25 D-sub** ("printer"-style)
connector. This is **not** a printer port — it is the proprietary I/O harness
that carries power, sensor and actuator signals between the two halves.

| Half | Where | Contents |
| --- | --- | --- |
| **Head unit** ("the computer") | `pic/front*`, `pic/top_pcb.jpg`, `pic/bottom_pcb.jpg` | Front panel (display LEDs + settings switches: `LÄMPÖTILAN SÄÄTÖ`, `KELLON ASETUS`, `YLÄRAJAT`, the day/time `LÄMPÖTILAN ALENNUSAJAT` setback matrix) and the CPU board (8035, two EPROMs, glue logic, opto/relay drivers, trimmers, resonator). The **DB25 is on the left edge of `bottom_pcb.jpg`.** |
| **Backend cabinet** (inside the heating unit) | `pic/backend_overview.jpg`, `pic/X10_X11_backend_*.jpg` | Power contactors `PAK1`/`PAK2`, mains transformer, terminal rails, sensor cables labelled `TS`/`TT`/`TH`, actuator terminals `VA1`/`VA2`/`AE`/`LE`, and two plug-in analog cards on edge connectors: `X10 = AVM2`, `X11 = AVO2`. |

So the DB25 carries: transformer power in, the relay/expander drive lines plus
their strobes, the interrupt/timing line, and the analog sensor/setpoint signals
to and from the X10/X11 cards.

## Firmware-side signal map (what the 8035 drives across the cable)

Derived from the reconstruction in [`../disasm/koti_lampo.c`](../disasm/koti_lampo.c)
and the Ghidra cross-check. Confidence tags match that file.

| 8035 signal | Role | Firmware reference | Confidence |
| --- | --- | --- | --- |
| `P2[7:4]` high nibble | Page / chip-select strobe to the 8243-style nibble expander. Constants `0x8F 0x9F 0xAF 0xCF 0xDF 0xEF`. | `expander_kick()` @ X0029 | LIKELY |
| `P4..P7` (via 8243 expander) | Nibble I/O bus (`movd`/`anld`/`orld`). | port-init @ X000d | LIKELY |
| `P7` latch | Relay output: heat call / valve / pump bits. Shadowed in `XRAM[0x15]`. | regulation pass @ X0224 | LIKELY |
| `P5` | Relay/channel **group select** + per-channel writes. | scan/select @ ~X01xx | LIKELY |
| `P6` | Expander output shadow (`XRAM[0xF8]`). | cold boot wipe | LIKELY |
| `P1.5` (`0x20`) | Strobe / enable output (set/cleared around conversions). | @ X028x | LIKELY |
| `P1.4` (`0x10`) | Input poll — ready / zero-cross / ADC-busy. | @ X028x wait loop | GUESS |
| `T0` | Read once at boot — cold-boot detect / config jumper. | reset @ X0000 | LIKELY |
| `INT` (external) | Interrupt line; ISR saves ACC + P2. Likely mains zero-cross / tick. | EXTIRQ | SURE (space), GUESS (source) |
| `BUS`/P0 | Multiplexed data to expander/latches. | throughout | GUESS |

Analog measurement & setpoints almost certainly route to the backend **X10
(AVM2)** and **X11 (AVO2)** cards rather than to an on-board ADC; the head unit
appears to deal in digital strobes + a busy/ready handshake.

## DB25 pinout — TO BE MEASURED

Cannot be read from the photos. Fill this in with a multimeter: probe each DB25
pin on `bottom_pcb`, trace to a CPU port pin / driver transistor / transformer
tap / GND, then match to the firmware signal above.

| DB25 pin | Traced to (head-unit net) | Backend function | Firmware signal | Notes |
| --- | --- | --- | --- | --- |
| 1 |  |  |  |  |
| 2 |  |  |  |  |
| 3 |  |  |  |  |
| 4 |  |  |  |  |
| 5 |  |  |  |  |
| 6 |  |  |  |  |
| 7 |  |  |  |  |
| 8 |  |  |  |  |
| 9 |  |  |  |  |
| 10 |  |  |  |  |
| 11 |  |  |  |  |
| 12 |  |  |  |  |
| 13 |  |  |  |  |
| 14 |  |  |  |  |
| 15 |  |  |  |  |
| 16 |  |  |  |  |
| 17 |  |  |  |  |
| 18 |  |  |  |  |
| 19 |  |  |  |  |
| 20 |  |  |  |  |
| 21 |  |  |  |  |
| 22 |  |  |  |  |
| 23 |  |  |  |  |
| 24 |  |  |  |  |
| 25 |  |  |  |  |

### Backend terminal map — TO BE MEASURED

| Label | Type (guess) | Function | Notes |
| --- | --- | --- | --- |
| `TS` | NTC/PT sensor? | Temperature sensor | Identify resistance curve |
| `TT` | NTC/PT sensor? | Temperature sensor |  |
| `TH` | NTC/PT sensor? | Temperature sensor |  |
| `VA1` | Actuator | Motor valve / pump |  |
| `VA2` | Actuator | Motor valve / pump |  |
| `AE` | ? |  |  |
| `LE` | ? |  |  |
| `X10` | Edge card | AVM2 (analog measure?) | Reverse the card's I/O |
| `X11` | Edge card | AVO2 (analog out?) |  |

### How to take the measurements

1. Power **off**, mains disconnected. Identify DB25 pin 1 (usually marked on the
   shell or silkscreen).
2. Continuity from each DB25 pin to: every 8035 port pin, each relay-driver
   transistor collector/base, transformer secondary, and GND/0V plane.
3. Note resistance of `TS`/`TT`/`TH` at known temperatures to identify the sensor
   type (NTC 10k? PT100/PT1000? Valmet-specific?).
4. With mains on **and** appropriate caution, scope the `INT` candidate pin to
   confirm 50 Hz zero-cross, and the `P1.4` pin for the busy/ready handshake.

## Replacement options

### Route A — CPU-socket drop-in (hard)
Replace the 8035 in its socket with an ESP32/RP2040 adapter that bit-bangs the
MCS-48 bus to the *original* CPU board. Keeps DB25 + backend untouched, but
requires faithful emulation of 8035 port/bus timing. High effort, little payoff.

### Route B — new head unit over the DB25 (recommended)
Don't emulate the CPU. Because the control logic is now reconstructed in
[`../disasm/koti_lampo.c`](../disasm/koti_lampo.c), reimplement it on an ESP32 and
drive the *same* contactors / valves / sensors directly through the DB25. Needs
the pinout table above completed first, then:

- ESP32 GPIO → relay drivers for `VA1`/`VA2` (and the `P7` heat/valve/pump bits).
- ESP32 ADC (or an external ADC) → `TS`/`TT`/`TH` after identifying the sensor
  curve and adding the right divider/reference.
- Decide whether to keep or replace the X10/X11 analog cards — if kept, the ESP32
  must speak their interface; if replaced, do measurement/output on the ESP32.
- Reproduce the setback schedule + setpoint logic from the disassembly.

Route B is the path the project README already aims at ("replace with a different
cpu and motherboard" / "drop-in replacement in the cpu socket with e.g. an
ESP32"). The only blocker is the physical pinout, which needs the meter.

## Photo annotations & measurement worksheet (Route B prerequisite)

What is visible in each photo and the exact probing to do. Record results in the
DB25 and backend tables above. **Mains disconnected for all continuity steps.**

### `pic/bottom_pcb.jpg` — CPU board, solder/component side
Visible: the **DB25 D-sub on the left edge**, the two EPROMs (centre, ceramic
windowed), the 40-pin 8035, several DIP logic ICs, a bank of relays/optos on the
right, two trimmer pots (top right), and the resonator/crystal. The DB25 is the
only off-board connector on this side — every cable to the backend goes through
it.

Probe list:
1. **Find DB25 pin 1.** Look for a `1` on the silkscreen or the shell key; D-sub
   pin numbering runs along the two rows (13 + 12).
2. **GND/0V pins:** beep from each DB25 pin to the board ground plane / the 8035
   `Vss` pin (pin 20). Mark every pin that is continuous with GND — expect
   several (signal returns).
3. **Power pins:** beep each DB25 pin to the transformer secondary pads and to
   the board `Vcc` rail. These are the supply feed from the backend transformer.
4. **CPU port pins:** with one probe on each 8035 port pin, find which DB25 pin it
   reaches (directly or through a driver). Map in this priority order, because the
   firmware tells us what they are:
   - 8035 `P7` latch outputs → relay drives (heat call / valve / pump).
   - 8035 `P5` outputs → relay/channel group select.
   - 8035 `P2[7:4]` → expander page strobes (`0x8F..0xEF`).
   - 8035 `P1.5` → strobe/enable out; `P1.4` → busy/ready in.
   - 8035 `INT` (pin 6) and `T0` (pin 1) → interrupt / boot-config lines.
5. **Driver transistors:** where a DB25 pin does *not* go straight to a CPU pin,
   trace it to a relay-driver transistor/opto and note which CPU port bit drives
   that driver's base — that bit is the logical signal for the pin.

### `pic/top_pcb.jpg` / `pic/front_open.jpg` — front panel board
Visible: display LEDs and the user controls (`LÄMPÖTILAN SÄÄTÖ`, `KELLON ASETUS`,
`YLÄRAJAT`, the day/time `LÄMPÖTILAN ALENNUSAJAT` setback switch matrix). This
board is local to the head unit and connects to the CPU board by an internal
ribbon, **not** through the DB25 — so for Route B it can be ignored (the ESP32
gets its own UI). Note it here only to confirm none of these switch lines appear
on the DB25.

### `pic/backend_overview.jpg` — power cabinet overview
Visible: contactors `PAK1`/`PAK2`, the mains transformer (right), terminal rails,
and the cabling fan-out. Identify:
- Which terminal block the head-unit DB25 cable lands on (the backend end of the
  umbilical).
- Transformer secondary voltage(s) feeding the head unit (measure AC at the
  secondary with mains on, **caution**).
- Which contactor coils (`PAK1`/`PAK2`) are switched by the relay-drive pins
  found on the CPU board.

### `pic/X10_X11_backend_1.jpg` / `..._2.jpg` — analog plug-in cards
Visible: two edge-connector cards, **`X10 = AVM2`** and **`X11 = AVO2`**, plus the
sensor cables hand-labelled **`TS` / `TT` / `TH`** and actuator terminals
**`VA1` / `VA2` / `AE` / `LE`**. Do:
1. Measure `TS`/`TT`/`TH` resistance at a known temperature (ice water + warm
   water) to identify the sensor curve — NTC 10k, PT100/PT1000, or
   Valmet-specific. Record two (temp, ohms) points per sensor.
2. On the X10/X11 edge connectors, find power, ground, the analog signal pin(s),
   and any digital strobe that ties back to a DB25 pin. Decide keep-vs-replace:
   - **Keep:** ESP32 must reproduce whatever strobe/clock the cards expect.
   - **Replace:** do the ADC (sensors) and analog/relay output on the ESP32 and
     remove the cards.
3. Trace `VA1`/`VA2`/`AE`/`LE` to the contactors/valves they switch and note the
   coil voltage/current so the ESP32 relay board is rated correctly.

### Minimal "first light" subset for Route B
If a full 25-pin trace is too much up front, the smallest set to bring up an
ESP32 prototype is: **power feed**, **GND**, the **relay-drive pins** (so it can
switch heat/valve/pump), and the **TS/TT/TH sensor lines** (so it can read
temperature). Strobes, the X10/X11 interface and the INT/zero-cross line can be
characterised in a second pass once basic on/off control is proven.
