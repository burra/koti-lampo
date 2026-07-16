# DB25 connector & modern-replacement notes

The Valmet Kotilämpö is split into two physically separate halves joined by a
single multi-conductor cable terminated in a **DB25 D-sub** ("printer"-style)
connector, labelled **X1** on the PCB silkscreen. This is **not** a printer
port — it is the proprietary I/O harness that carries power, sensor and
actuator signals between the two halves.

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
| `P2[7:4]` high nibble | Page / chip-select strobe to the **P8243** nibble expander. Constants `0x8F 0x9F 0xAF 0xCF 0xDF 0xEF`. | `expander_kick()` @ X0029 | SURE (P8243 confirmed by block schema) |
| `P4..P7` (via P8243 expander) | Nibble I/O bus (`movd`/`anld`/`orld`). | port-init @ X000d | SURE (expander confirmed) |
| `P7` latch | Relay output: heat call / valve / pump bits. Shadowed in `XRAM[0x15]`. Latched by the **MC14508B** dual 4-bit latch ("databus driver"). | regulation pass @ X0224 | LIKELY (latch part confirmed) |
| `P5` | Relay/channel **group select** + per-channel writes. | scan/select @ ~X01xx | LIKELY |
| `P6` | Expander output shadow (`XRAM[0xF8]`). | cold boot wipe | LIKELY |
| `P1.5` (`0x20`) | Strobe / enable output (set/cleared around conversions). | @ X028x | LIKELY |
| `P1.4` (`0x10`) | Input poll — ready / zero-cross / ADC-busy. | @ X028x wait loop | GUESS |
| `T0` | Read once at boot — cold-boot detect / config jumper. | reset @ X0000 | LIKELY |
| `INT` (external) | Interrupt line; ISR saves ACC + P2. Mains zero-cross / tick, divided by the **MC14040B** counter and gated through the **MC14013B** flip-flops. | EXTIRQ | SURE (space), LIKELY (source, timing chain confirmed) |
| `BUS`/P0 | Multiplexed data to expander/latches. | throughout | GUESS |

Analog measurement & setpoints almost certainly route to the backend **X10
(AVM2)** and **X11 (AVO2)** cards rather than to an on-board ADC; the head unit
appears to deal in digital strobes + a busy/ready handshake.

### Confirmation from the board block diagram (`Display blockschema.odg`)

The hardware block schema corroborates the firmware-derived map (see the
datasheet table in the root [`README.md`](../README.md)):

- **P8243** — confirms the "8243-style nibble expander" inference on P4–P7.
- **MC14508B** dual 4-bit latch / databus driver — the `P7` relay output latch.
- **MC14040B** 12-bit counter + **MC14013B** D flip-flops — the timing divider and
  the `INT`/zero-cross handshake.
- **MC14012B** dual 4-input NAND — glue logic.
- **4N26 optoisolators (transistor output)** — the lines crossing the DB25 to the
  mains-side backend are **galvanically isolated**. For Route B this means an ESP32
  can drive/read these lines at logic level *through optos*, and any replacement
  must preserve that isolation barrier between the controller and mains side.
- **VA2101 EEPROM** — non-volatile store on the head-unit board for user settings
  (clock, the `LÄMPÖTILAN ALENNUSAJAT` setback schedule, setpoints). It is local to
  the controller, **not** on the DB25, so Route B replaces it with the ESP32's own
  flash/NVS — the only requirement is to persist the same settings.

## Canonical I/O list (`IO list.ods`)

The repo-root spreadsheet [`IO list.ods`](../IO%20list.ods) is a clean,
tag-based I/O inventory that maps one-to-one onto the firmware/manual signal
names. This is the authoritative signal count for a Route B (ESP32) rebuild.

| Tag | Type | Info | = firmware/manual name |
| --- | --- | --- | --- |
| `TE_101` | PT100 (in) | inside temperature | `TH` |
| `TE_102` | PT100 (in) | sun-panel temperature | `TAK` |
| `TE_201` | PT100 (in) | water temp before battery | `THM` (heating supply) |
| `TE_202` | PT100 (in) | water temp after battery | `TAM` (heating return) |
| `TE_203` | PT100 (in) | water temp after sun-panel battery | `TAP` |
| `TE_204` | PT100 (in) | tank water temperature | `TS` |
| `ST_101` | FDI (pulse in) | water circulation | `VM` (heating flow) |
| `ST_102` | FDI (pulse in) | hot-water circulation | `LVM` (DHW flow) — **not in use** |
| `CV_101` | DO | magnet valve 24 VDC | `VA1` |
| `CV_102` | DO | magnet valve 24 VDC | `VA2` |
| `CV_201` | DO | relay, extra heat | aux-heat (`LL`) |
| `CV_301` | DO | 3-way valve 230 VAC | `VH` |
| `CV_401` | DO | sun-panel circulation fan | `PAK` |
| `CV_501` | DO | ventilation valve motor 230 VAC | `PM1` |
| `CV_502` | DO | ventilation valve motor 230 VAC | `PM2` |
| `CV_601` | DO | circulation pump 230 VAC | `VP` |
| `ST_201` | FDO (pulse out) | pulse train, sun energy | `AE` |
| `ST_202` | FDO (pulse out) | pulse train, consumed energy | `LE` |

**I/O budget for the ESP32:** 6× PT100 inputs, 2× flow-pulse inputs (1 unused),
8× digital outputs (2× 24 VDC solenoid, 6× 230 VAC via contactor/relay), 2×
pulse outputs.

Front-panel display selector (counter/memory items, recalled with the front
button): clock; 1 inside temp; 2 sun-panel temp; 3 water temp riser to battery;
4 water temp after battery; 5 water temp after sun-panel battery; 6 tank temp;
7 consumed energy kWh; 8 solar energy kWh; 9 consumed hot water m³; 0 circulated
heating water m³.

## DB25 pinout — TO BE MEASURED

Cannot be read from the photos. Fill this in with a multimeter: probe each DB25
pin on `bottom_pcb`, trace to a CPU port pin / driver transistor / transformer
tap / GND, then match to the firmware signal above.

**Known signal inventory (from `Kotilämpö_vuokaaviot.pdf`, KUVA 11).** That
schematic draws the DB25 explicitly; its legend decodes every line on the cable.
The DB25 must therefore carry some subset of these — use it as the candidate list
when tracing. See [`flowcharts_en.md`](flowcharts_en.md) for full definitions.

Sensors (inputs): `TH` room, `TAK` solar collector, `THM` heating supply, `TAM`
solar-exchanger supply, `TAP` return, `TS` storage tank, `VM` heating flow,
`LVM` DHW flow.

Actuators (outputs): `VA1`/`VA2` solenoid valves, `VH` 3-way mixing valve,
`VP` circulation pump, `PM1`/`PM2` damper motors, `PAK I`/`PAK II` solar-fan
contactor (2 stages), `VPK` pump contactor; plus `AE`/`LE` energy-pulse outputs to
the remote display.

Power: mains feed is `1~ 220 V, 10 A` via a `T 6.3 A` slow fuse + transformer on
the backend side (KUVA 6 / KUVA 11). The umbilical is Valmet-supplied cable in a
Ø50 conduit, **7 m max**.

**Confirmed signal subset (from the service test box, `Kotilämpö_toiminta_ja_käyttö.pdf`
appendix).** The factory **test box connects between the control unit and the
power-supply unit** on the existing cable — i.e. it physically taps this exact
interface. Its LEDs/buttons are therefore an *authoritative* list of what crosses
the link (stronger than the KUVA 11 candidate list, which includes backend-only
nets). See [`toiminta_ja_kaytto_en.md`](toiminta_ja_kaytto_en.md).

- **Sensor measurements (control unit ← power-supply AVM card):** `REF`
  (automation's own reference resistor), `TH`, `TAK`, `THM`, `TAM`, `TAP`, `TS`.
  Measured **sequentially** — REF first, then each sensor at a "low" and "high"
  end, the step encoded on LEDs `A1..A4`; a failing sensor is retried up to 3×,
  else the old reading is kept. This confirms the firmware map's
  *digital-strobe + busy/ready handshake* picture (`P1.5` strobe, `P1.4` ready):
  there is no parallel ADC, just one multiplexed resistance-measuring channel.
- **Sensors are resistive** — test resistors substitute for them directly
  (glossary: Pt100). `REF` is an on-board reference resistor for ratiometric
  measurement. Useful calibration points: `TSTAV` defaults to **60 °C**, testable
  with **52 °C / 70 °C** test resistors.
- **Actuator drives (control unit → power-supply):** `VA` (= `VA1`), `VH`,
  `PAK I`, `PAK II` — each LED lights when that actuator is being driven. (VA2 is
  the de-energised complement of VA1, so it need not be a separate line.)
- **Status / metering outputs:** `LL` (aux-heat on), `LE` / `AE` (blink once per
  energy kWh).
- **Flow-pulse inputs (→ control unit):** `VM`, `LVM` — the box can inject pulses
  to verify the control unit counts them.

Net effect for the pinout trace: the DB25 must carry power + GND, **7 resistive
sensor lines on one multiplexed measuring channel** (TH, TAK, THM, TAM, TAP, TS,
plus the REF reference), the strobe/ready handshake pair, the actuator drives
(VA1, VH, PAK I, PAK II), the LL/LE/AE status lines, and the VM/LVM pulse inputs.

| DB25 pin | P8243 (chip, pin, name) | Opto | Notes |
| --- | --- | --- | --- |
| 1 | — | — | VCC |
| 2 | — | — | GND |
| 3 | — | — | VCC |
| 4 | — (via `CD4093BFX`, TBD) | 4N26 #12 + #15 | Confirmed; anti-parallel LED pair with pin 17 — see note |
| 5 | — (further trace TBD) | 4N26 #13 | Confirmed; LED anode (pin 1), downstream P8243/CPU pin not yet traced |
| 6 | — (further trace TBD) | 4N26 #14 | Confirmed; LED anode (pin 1), downstream P8243/CPU pin not yet traced |
| 7 | — | — | NC (not connected) |
| 8 | — | — | GND |
| 9 | `#2` pin 22 (`P52`) | 4N26 #11 | No firmware match found yet |
| 10 | `#2` pin 23 (`P51`) | 4N26 #10 | No firmware match found yet |
| 11 | `#2` pin 24 (`VCC`) | 4N26 #9 | Confirmed |
| 12 | `#2` pin 21 (`P53`) | 4N26 #8 | Corrects earlier "4N26 #2 / 005F" misattribution |
| 13 | — | — | GND |
| 14 | — | — | VCC (corrects earlier GND misattribution) |
| 15 | — | — | GND |
| 16 | — | — | VCC (DB25 connector pin — distinct from `P8243 #1` package pin 16 used for DB25 pin 22's opto chain, below) |
| 17 | — (further trace TBD) | 4N26 #12 + #15 | Confirmed; 4N26 #12 pin 1 (LED anode) and 4N26 #15 pin 2 (LED cathode) — completes the anti-parallel LED pair with pin 4, see note |
| 18 | — | — | GND |
| 19 | `#2` pin 19 (`P61`) | 4N26 #7 | Candidate firmware match, see multi-chip caveat |
| 20 | `#2` pin 18 (`P62`) | 4N26 #6 | Candidate firmware match, see multi-chip caveat |
| 21 | `#2` pin 17 (`P63`) | 4N26 #5 | Same physical package as 9/10/11/12/19/20, see multi-chip caveat |
| 22 | `#1` pin 16 (`P73`) | 4N26 #1 | Pulsed actuator/status output, see opto driver chain |
| 23 | `#1` pin 15 (`P72`) | 4N26 #2 | Fault/interlock input, see pin 23 note |
| 24 | `#1` pin 14 (`P71`) | 4N26 #3 | Candidate firmware match, see multi-chip caveat |
| 25 | `#1` pin 1 (`P50`) | 4N26 #4 | Likely companion write to pin 22's pulse, see pin 25 note |

**All 25 of 25 DB25 pins now identified.** Power/ground (VCC: 1, 3, 14, 16;
GND: 2, 8, 13, 15, 18 — 9 pins), pin 7 (NC), all 4 `P8243 #1` package pins
(22, 23, 24, 25), all 7 `P8243 #2` package pins (9, 10, 11, 12, 19, 20, 21),
and pins 4/17 (the anti-parallel `4N26 #12`/`#15` pair) plus 5/6 (each
opto-isolated on their own dedicated 4N26). What's left is not pin
identity but **downstream tracing**: the CPU/`P8243` pin each of `4N26`
#12's, #13's, #14's, and #15's isolated collector ultimately feeds (#12 is
already known to reach a `CD4093BFX` gate; the other three are still open).

**Every traced signal pin (all except power/ground) runs through at least
one dedicated 4N26 optoisolator** — 15 confirmed so far (pin 4 touches
two: #12 and #15), numbered in the order found (see the `Opto` column
above). Given the board carries 7 `P8243` packages (see multi-chip caveat
below), the real opto count across the whole connector is likely in the
dozens — 15 is a lower bound, not a total.

**Pins 4 and 17 — an anti-parallel LED pair.** Between them, these two
DB25 pins wire up both `4N26 #12` and `4N26 #15` in inverse parallel:

```
DB25 pin 4  --- 4N26 #12 pin 2 (cathode)   4N26 #15 pin 1 (anode) --- DB25 pin 17
DB25 pin 17 --- 4N26 #12 pin 1 (anode)     4N26 #15 pin 2 (cathode) -- DB25 pin 4
```

i.e. two LEDs, wired back-to-back, sharing the same two-wire pair — current
flowing 4→17 lights #12, current flowing 17→4 lights #15. That's a
polarity-independent (or AC-sensing) input scheme: the receiving circuit
doesn't care which direction current flows through the pair, since one of
the two LEDs will conduct either way. This resolves what initially looked
like an unexplained shared node on pin 4 alone.

Each opto's isolated collector still needs tracing separately. `4N26 #12`'s
is the only one known so far: its collector feeds a `CD4093BFX` (quad
2-input NAND, already on the parts list as glue logic) rather than going
straight to a `P8243`. Since pin 4's LED side is what's on the DB25 (not
the collector side, unlike pin 22's chain), the *backend* drives this opto
and the signal direction is inbound to the head unit — the `CD4093BFX` is
plausibly there to clean up/debounce the opto's output edge (its gates are
Schmitt-trigger inputs) before the signal reaches CPU logic. `4N26 #15`'s
collector destination is still open.

**Pin 11.** DB25 pin 11 traces through 4N26 #9 to `P8243 #2` pin 24 — the
package's own `VCC` pin per the datasheet pinout (see the `README.md`
datasheets table), rather than a `P4-P7` signal pin. Confirmed.

### Opto driver chain (confirmed pattern for pin 22, first 4N26)

Tracing DB25 pin 22 found the full isolated signal path through the first
4N26 on the board:

```
P8243 pin 16 --[resistor]--> 4N26 #1 pin 2 (LED cathode)
                              4N26 #1 pin 1 (LED anode)  = VCC
                              4N26 #1 pin 6 (output side) = GND
                              4N26 #1 pin 5 (collector)  = DB25 pin 22
```

The CPU (via the `P8243` expander) sinks current through the LED to switch
the opto; the isolated collector then drives DB25 pin 22 across the mains
barrier. This is the same chain shape expected for every actuator/status
line — useful as a template when tracing the remaining pins: find the
resistor feeding an opto's LED cathode, work backward to the `P8243`/`P7`
port bit for the firmware signal, and forward from the collector to the
DB25 pin.

**Correction — pin 12 and the real 4N26 count.** DB25 pin 12 was earlier
misattributed to "4N26 #2 (marked 005F)". It's actually **4N26 #8**, LED
anode side, continuing on to `P8243 #2` pin 21 (`P53`) — the same physical
`P8243` package that also serves DB25 pins 10, 11, 19, 20, and 21 (exact
package pins for 10/11/19/20 still TBD). Given the board turned out to
carry 7 `P8243` packages rather than 2 (see multi-chip caveat below), the
4N26 count is likely similarly larger than the "two" first assumed — treat
any `4N26 #N` label in this doc as an id from continuity tracing, not a
claim about total quantity on the board.

**Firmware match:** on the [P8243](../README.md#datasheets) 24-pin DIP, pin 16
is **`P73`** — bit 3 (MSB) of Port 7 (`P70..P73` = pins 13-16). Bit `0x08` of
P7 is set, briefly held, then cleared (a pulse) in
`regulation_pass()` @ `X0224` (`disasm/koti_lampo.c:326-328`):

```c
u8 relays = X(0x15) | 0x08; wr_p7(relays);
bank1_busy_delay();                          /* call X0cff */
relays &= 0xFB; X(0x15) = relays; wr_p7(relays);
```

### Pin 23 — likely a hard fault/interlock input (P73's neighbour, opposite direction)

DB25 pin 23 traces to `P8243 #1` pin 15, i.e. **`P72`** (bit `0x04` of the same
`P7` port group as pin 22/`P73` above), but this bit is *read*, not written —
`movd_p7()`, not `wr_p7()`. Two independent call sites treat it identically,
as a hard fault:

```c
// read_timing_channel() @ X01eb — analog measurement poll
if (movd_p7() & 0x04) { P2 &= 0x7F; for (;;) {} }  /* fault hang */

// ext_interrupt() @ X04ff — button/panel scan
if (p7 & 0x04) { for (;;) { P2 &= 0x7F; } }  /* fault hang */
```

Both deliberately lock the CPU in an infinite loop if the bit is set —
consistent with a hard-wired safety interlock, not routine status. It also
fits `io_init()`'s startup sequence (`or_p7(0xFF)` then `and_p7(0x00)`,
i.e. P7 starts actively driven **low** by the CPU): the 8243's ports are
quasi-bidirectional (weak pull, overridable by a stronger external drive),
so the likely picture is the CPU normally holds this line low and some
backend condition (high-limit thermostat, overtemperature cutout, or a
power-supply fault line) can force it high, overriding the CPU's weak pull —
detected on the next poll and treated as unrecoverable. This would be
independent of the software fault logic already documented in the manuals
(`TAP<5°C`, `TAM-TAP>1°C`, etc.) — a second, hardware-level safety layer.

So DB25 pin 22 carries a **pulsed**, not level-held, actuator/status signal —
consistent with the flow-pulse outputs `AE`/`LE` (energy metering, "blink
once per kWh") rather than a level-held valve/relay line like `VA1`/`VH`.
Next step: identify which XRAM[0x15] bits the *other* relay writes
(`relay_clear_and_idle()` clears `0xFC` = bits 2-7) use, and match each to
its own P7 pin (13/14/15/17-20 for P6, 21-23+1 for P5) and DB25 pin, to
build out the rest of the actuator map.

### Pin 25 — likely companion write to pin 22's pulse

DB25 pin 25 traces to `P8243 #1` pin 1, i.e. **`P50`** (bit 0 of Port 5).
`regulation_pass()` sets this bit (`or_p5(0x01)`) on the very next line
after the pin-22 pulse, under the same `P2 = 0xEF` strobe:

```c
u8 relays = X(0x15) | 0x08; wr_p7(relays);
bank1_busy_delay();
relays &= 0xFB; X(0x15) = relays; wr_p7(relays);
P2 = 0xEF; or_p5(0x01);   /* <- DB25 pin 25 candidate */
```

Same function, same strobe, immediately adjacent — a reasonable case that
this is a companion status/latch bit for the same actuator event as pin 22
(e.g. "pulse done" or "select next stage"), and likely the **same physical
chip** as pin 22/23.

### Multi-chip caveat — the board has (at least) 7 P8243 packages, not 2

Confirmed by inspection: **4 `P8243` on the bottom (CPU) board, 3 more on the
top (front-panel) board.** `disasm/koti_lampo.c` predates this and models
port access with one generic set of functions (`movd_p4..p7`, `wr_p4..p7`,
`or_p5/6/7`, `and_p5/6/7`) with no per-chip distinction — so any single
code-level match like "`P7` bit `0x02`" could in principle belong to *any*
of the 7 packages, not just the two seen on the DB25 so far.

**Important finding: the `P8243` serving DB25 pins 22-25 has its pin 6
(`CS'`, chip select) not connected to anything** — no decode/select logic.
On the 8243, `CS'` is a dedicated pin, separate from the shared 4-bit
`PROG`-clocked command bus (`BUS` = `P20-P23`); with multiple 8243s on one
shared bus, only the chip whose `CS'` is asserted may drive it back, so
sharing a bus normally *requires* per-chip select logic (a plausible
candidate already on the parts list: the **`MC14012B`** dual 4-input NAND,
documented as board "glue logic"). A `CS'` wired to nothing implies the
opposite: **this chip doesn't need arbitration because it isn't sharing its
bus with anything** — i.e. this design most likely uses several independent
`BUS`/`PROG` groups (plausibly one per board, or even one per chip-cluster)
rather than one 7-way-decoded shared bus. That's a reasonable read of why
there are 7 separate packages in the first place: simpler to give each
functional group (front-panel keypad/display vs. CPU-board relay/sensor
I/O) its own dedicated expansion bus than to build 7-way select decode.

This is good news for the pins 22/23/25 findings: that chip isn't
contending with anything, so the `P2` strobe values already matched against
the code for it should be trustworthy as they stand.

It reframes, rather than resolves, pins 21 and 24:

- **Pin 21** (on a different physical package than 22-25, confirmed by
  continuity) — next check is whether *its* `CS'` is also unconnected (own
  dedicated bus, so the earlier "same `P2=0xAF` context" observation in
  `ext_interrupt()` is coincidental naming, not evidence of sharing) or
  wired to real decode logic (meaning it *does* share a bus with others,
  and the decode line is what actually needs identifying — not `P2` alone).
- **Pin 24** (`P8243 #1` pin 14, `P71`) — candidate match
  `if (p7 & 0x02) { btn_inc(0x02, 0x18); ... }` still reads as a
  front-panel button, architecturally odd for a DB25/backend pin. Treat as
  unresolved regardless of the CS finding.
- **Pins 19/20** (`P8243 #2` pins 19/18, `P61`/`P62`) have the same shape of
  issue: candidate matches `if (p6 & 0x02) { btn_clock_adjust(); ... }` and
  `if (p6 & 0x04) { btn_mode(); ... }` are both front-panel buttons in
  `ext_interrupt()` — again architecturally odd for DB25/backend pins.
  Treat as unresolved for the same reason as pin 24. Pins 9/10 (`P52`/`P51`)
  have no candidate code match at all yet.

**To resolve conclusively:** for each `P8243`, trace pin 6 (`CS'`) *and*
the `BUS`/`PROG` pins (8-11, 7) back toward the CPU. Two chips only share a
logical port space if both their `BUS` and `PROG` land on the same CPU
pins — `CS'` wiring (or the lack of it) is what tells you whether that
sharing needs arbitration.

**`P8243 #2`'s `BUS` trace — 3 of 4 confirmed, 1 anomalous.** Cross-checked
against the Intel MCS-48 40-pin DIP pinout (pins 12-19 = `D0-D7`, the
8035's data bus; pin 20 = `GND`/`Vss`):

| `P8243` pin | `P8243` signal | CPU (8035) pin | CPU signal |
| --- | --- | --- | --- |
| 8 | `P23` | 17 | `D5` |
| 9 | `P22` | 18 | `D6` |
| 10 | `P21` | 19 | `D7` |
| 11 | `P20` | 20 | `GND`/`Vss` (anomalous) |

The first three land exactly where expected — consecutive `BUS` bits on
consecutive CPU pins. The fourth breaks the pattern: `P20` landing on
`GND` rather than `D4` (which would complete the sequence at CPU pin 16)
doesn't fit a normal 4-bit bus line. Re-confirmed by continuity, so
recorded as measured rather than assumed to be a mis-read — but flagged as
needing further explanation (possible candidates: a partially-populated
bus where only 3 of the 4 `P2x` lines are actually used, or a
mis-identified physical pin under the probe).

**Major open question: `CS'` and `PROG` are confirmed floating (no trace
found at all, either side of the board) on *both* `P8243 #1` and
`P8243 #2`.** This is a bigger anomaly than it might look: `PROG` is not
optional on the 8243 protocol — the datasheet defines every read/write/
OR/AND operation as "set up the command on `P2` + data on `BUS`, then
pulse `PROG` to latch it." A chip with `PROG` truly floating cannot
execute a transaction via the documented protocol at all, which sits
awkwardly next to the firmware's heavy, working use of `movd_p4..p7`,
`wr_p4..p7`, `or_p5/6/7`, and `and_p5/6/7` throughout
`disasm/koti_lampo.c`.

Possible explanations, none yet confirmed:
- **Probing miss.** A via carrying the signal to the opposite copper layer
  is easy to miss on a double-sided board — worth re-checking both sides
  before ruling this out.
- **Wrong reference pin.** If package-pin counting was thrown off (e.g. by
  the earlier `P8243`-vs-`2716`-EPROM mix-up risk already noted for pin
  11), pins 6/7 might not be the ones actually being probed.
- **Non-standard usage.** Less likely, but possible: this design might not
  drive `PROG` per-chip in the conventional sense, e.g. a shared/free-
  running clock wired elsewhere that wasn't recognised as `PROG` during
  tracing.

Until resolved, treat the confirmed `P8243`/DB25 pin mappings above as
solid (they're independent continuity facts), but treat *how* these chips
are actually clocked/selected as an open question — worth a dedicated
re-check pass with the board flipped both ways before trusting either
conclusion.

### Backend terminal map — TO BE MEASURED

Sensor types are no longer a question: `IO list.ods` confirms **all six
temperature sensors are PT100** (`TE_101..TE_204`). What still needs the meter
is the *pinout* — which DB25 pin each lands on — not the sensor technology.

| Label | Type | Function | Notes |
| --- | --- | --- | --- |
| `TS` | **PT100** | Tank water temperature (`TE_204`) | ~100 Ω @ 0 °C, ~138.5 Ω @ 100 °C |
| `TT` | **PT100** | Temperature sensor (one of `TE_101..TE_203`) | PT100 per I/O list |
| `TH` | **PT100** | Inside/room temperature (`TE_101`) | PT100 per I/O list |
| `VA1` | Actuator | Solenoid valve 24 VDC (`CV_101`) |  |
| `VA2` | Actuator | Solenoid valve 24 VDC (`CV_102`) |  |
| `AE` | Pulse out | Solar-energy pulse train (`ST_201`) |  |
| `LE` | Pulse out | Consumed-energy pulse train (`ST_202`) |  |
| `X10` | Edge card | AVM2 (analog measure?) | Reverse the card's I/O |
| `X11` | Edge card | AVO2 (analog out?) |  |

### How to take the measurements

1. Power **off**, mains disconnected. The connector is **X1** on the PCB
   silkscreen. **VCC: pins 1, 3, 14, 16; GND: pins 2, 8, 13, 15** (confirmed).
2. Continuity from each remaining DB25 pin to: every 8035 port pin, each
   relay-driver transistor collector/base, transformer secondary, and GND/0V
   plane.
3. Sensors are **PT100** (confirmed by `IO list.ods`); no need to identify the
   curve. Just verify ~100 Ω near 0 °C to confirm wiring/probe before tracing
   each sensor line to its DB25 pin.
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

### Physical chip naming convention (no factory silkscreen refdes on this board)

This board has no per-component reference designators printed on the
silkscreen, so chips are referred to by **`<part>-<board><n>`**, e.g.
`P8243-B1`, `4N26-B12`:
- `<part>` — the part number (`P8243`, `4N26`, `CD4093BFX`, `MC14013BC`, …).
- `<board>` — `B` for the bottom/CPU board, `T` for the top/front-panel board.
- `<n>` — a locally-assigned index, numbered by physical position (e.g. the
  15 `4N26` optoisolators on the bottom board are `4N26-B1`..`4N26-B15`,
  left-to-right along the row visible in the interactive chip map below).

**Correction:** the row of black components running along the CPU's
right side (previously mis-described here as a "relay bank") is not
relays — it's a row of **front-panel pushbuttons**, mounted on this
board but operated through the case from the top plate (the weekday /
setback-time buttons etc. in the `LÄMPÖTILAN ALENNUSAJAT` matrix and
similar). They wire into `P8243 #3`/`#4`, which aren't traced yet.

**`P8243 #3`'s neighbours:** the two round "SEUFER"-branded components
next to it (top-right corner of the board) are **rotary switches**
(numbered/detented position switches), not trimmer potentiometers —
consistent with `P8243` being digital I/O only (no ADC). Likely wired
into `#3` alongside the button row; not traced yet.

(A static labeled reference photo used to live here; superseded by the
interactive chip map below, which covers every IC and stays easier to
keep in sync.)

### `docs/bottom_pcb_chip_map.html` — interactive chip map

Every IC on the board's component side, marked as toggleable SVG layers
over `pic/bottom_pcb_component_side.jpg` so any category can be isolated
while tracing: optos, all 4 `P8243`s (`#1`-`#4`, only `#1`/`#2` traced so
far), glue logic (`CD4093BFX` C1-C3, `MC14013BC` D1-D5), CPU, EPROMs,
`MC14040BA` counters, `SN74LS138N`/`139N` decoders, misc gates
(`MC14012BAL`, `CD4025BCJ`, `CD4023BF`, `ECD4011BE` x2), analog
(`HEF4066AE` x2, `LM239J`), keypad/display drivers (`MC14518BAL` x6,
`MC14508BCL` x2), memory/IO (`P8212`, `MCM51L01P45` x2 — confirmed at the
board). Passive/mechanical parts (resonator, trimmer pots, rotary
switches, relays, discretes) are intentionally not marked.

Open the file directly in a browser — it loads the photo via a relative
path, so keep it next to `pic/`. If `bottom_pcb_component_side.jpg` is
ever re-cropped, the marker coordinates in the `<script>` block need
regenerating to match (they're pixel coordinates against that exact
image).

### `pic/bottom_pcb_both_sides_overlay.jpg` — both copper layers combined

Both sides of the bottom PCB combined into one image: **green = component
side, magenta = solder side**; where a trace exists on both layers at the
same spot it reads white/bright. Built by manually matching ~9 physical
reference points (mounting holes, the 25×30mm rectangular window, one
mounting screw) between the two full-res photos, then fitting a
polynomial warp of the solder-side photo into the component-side photo's
pixel space. Each layer is then run through adaptive local contrast
(CLAHE, to even out lighting so faint/shadowed traces aren't lost),
thresholded to a trace-only mask (level stretch + sigmoidal contrast +
a slight morphological close to reconnect broken segments), and
composited as opaque colored lines onto a pale cream background matching
the board's actual translucent fiberglass color — closer to how the bare
board looks in hand than a raw photo blend, and traces read clearly and
continuously instead of fighting substrate texture or fading out.

**Alignment is approximate, not pixel-precise** — good near the reference
points (roughly the board's center), drifting up to ~10-15px near the
edges, a genuine limit of aligning two independently hand-shot phone
photos on a handful of manually-picked points. Treat it as a rough visual
aid for spotting which general area a trace lands in on the other layer —
verify anything load-bearing with a multimeter, the same as everything
else in this doc.

Automated feature-matching alignment (SIFT/RANSAC/spline) isn't viable
here — the board's dense repeated chip/pad patterns defeat descriptor
matching. Getting this pixel-precise needs a fixed-camera rig (board
flipped in place, camera untouched) rather than two independent handheld
photos — worth doing if these photos are ever retaken.

Not pixel-aligned with `bottom_pcb_component_side.jpg` or the interactive
chip map — it's a separate crop/warp, built straight from the original
full-resolution photos.

### `pic/bottom_pcb.jpg` — CPU board, solder/component side
Visible: the **DB25 D-sub on the left edge**, the two EPROMs (centre, ceramic
windowed), the 40-pin 8035, several DIP logic ICs, a row of front-panel
pushbuttons (operated from the top plate — weekday/setback buttons etc.,
wired to `P8243 #3`/`#4`, not the same as relays) on the right, two trimmer
pots (top right), and the resonator/crystal. The DB25 is the only off-board
connector on this side — every cable to the backend goes through it.

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
1. Sensors are **PT100** (per `IO list.ods`). Optionally spot-check resistance
   in ice water (~100 Ω @ 0 °C) to confirm the probe/wiring before wiring up the
   ESP32 front end (PT100 needs a precision current source / divider + amp, not
   the simple divider an NTC would use).
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
