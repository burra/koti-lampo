# Reimplementation spec — new controller (ESP32 / Python)

Consolidated control-logic specification for building a **functionally
equivalent** replacement controller (the "Route B" path in
[`db25_replacement.md`](db25_replacement.md)). The goal is a new brain that
reads the same sensors and drives the same actuators per the same rules — **not**
a bit-exact emulation of the original Intel 8035.

Sources, in order of authority for the control behaviour:

1. [`toiminta_ja_kaytto_en.md`](toiminta_ja_kaytto_en.md) — function description,
   thresholds, fault logic, service rules (the manual).
2. [`flowcharts_en.md`](flowcharts_en.md) — the 8-block program and the formulas.
3. `IO list.ods` + [`db25_replacement.md`](db25_replacement.md) — the I/O contract.
4. [`../disasm/koti_lampo.c`](../disasm/koti_lampo.c) — corroboration of structure
   and constants (cross-check, tie-breaker).

> Confidence tags below: **[DOC]** stated in the manual/flowcharts;
> **[FW]** confirmed in the firmware reconstruction; **[TODO]** must still be
> measured or decided before coding.

---

## 1. I/O contract

Tag names from `IO list.ods`; firmware/manual names in parentheses.

### Inputs

| Tag | Firmware | Type | Meaning |
| --- | --- | --- | --- |
| `TE_101` (TH)  | room | PT100 | Indoor/room temperature (in the control centre) |
| `TE_102` (TAK) | collector | PT100 | Solar-collector temperature |
| `TE_201` (THM) | heating supply | PT100 | Heating supply-water temp |
| `TE_202` (TAM) | heating return | PT100 | Heating return = solar-exchanger supply |
| `TE_203` (TAP) | exchanger return | PT100 | Solar-exchanger return-water temp |
| `TE_204` (TS)  | tank | PT100 | Storage-tank mid temperature |
| `ST_101` (VM)  | heating flow | pulse in | Heating-water flow-meter pulses |
| `ST_102` (LVM) | DHW flow | pulse in | DHW flow — **not in use** [DOC] |

All six sensors are **PT100** (~100 Ω @ 0 °C, ~138.5 Ω @ 100 °C). [DOC]
The original measures them one at a time on a single time-to-count channel with
an internal reference resistor `REF`; a new build just uses a PT100 front end
(precision current source / ratiometric ADC) per channel. [FW]

### Outputs

| Tag | Firmware | Type | Load | Notes |
| --- | --- | --- | --- | --- |
| `CV_101` | VA1 | DO | solenoid 24 VDC | Kotilämpö return; **normally OPEN de-energised** [DOC] |
| `CV_102` | VA2 | DO | solenoid 24 VDC | Kotilämpö bypass; **normally CLOSED de-energised** [DOC] |
| `CV_301` | VH  | DO(s) | 3-way mixing valve 230 VAC | ~**20 min** time constant; has manual override [DOC] |
| `CV_401` | PAK | DO | solar fan 230 VAC | single-phase, 3-speed via transformer (PAK I/II) [DOC] |
| `CV_501` | PM1 | DO | damper motor 230 VAC | dampers at top of solar machine [DOC] |
| `CV_502` | PM2 | DO | damper motor 230 VAC | |
| `CV_601` | VP  | DO | circulation pump 230 VAC | |
| `CV_201` | (LL) | DO | relay, extra heat | aux-heat enable |
| `ST_201` | AE  | pulse out | solar-energy kWh pulses | LED blinks per kWh |
| `ST_202` | LE  | pulse out | consumed-energy kWh pulses | LED blinks per kWh |

**I/O budget:** 6 PT100 in, 2 pulse in (1 unused), 8 DO, 2 pulse out.

> **[TODO] DB25 pinout** — which physical pin carries each signal is still
> unmeasured. This is the one hard blocker; see the measurement worksheet in
> [`db25_replacement.md`](db25_replacement.md). Everything else below can be
> coded and bench-tested before the pinout is known.

---

## 2. Operator setpoints / knobs

| Symbol | Meaning | Notes |
| --- | --- | --- |
| base setpoint | selected room temperature | front panel |
| setback (`alennus`) | night reduction of the base, with day/time schedule | `LÄMPÖTILAN ALENNUSAJAT` matrix |
| `PITO` | **effective** room setpoint = base − setback (when active) | derived, not a knob |
| `ATS` | tank upper-limit knob | feeds `TSTAV` |
| `AP` | return-water (TAM) upper-limit knob | limits heat draw |
| aux-heat option | 1 / 2 / 3 | see §6 |
| `TSTAV` | **calculated** tank target (not displayed) | from ATS + curve + option + weekly min |

`TSTAV` defaults to **60 °C** at commissioning/reset (testable with 52/70 °C
resistors). [DOC]

---

## 3. Constants and thresholds (single table)

| Name | Value | Where used | Source |
| --- | --- | --- | --- |
| Cycle time | **1 min** | main loop | [DOC/FW] |
| Snow-melt cycle | **30 min** | block 8 | [DOC] |
| PT100 conversion | `Tx = (tx − tref)/tref × 258.98` | temp read | [DOC] flowcharts |
| Energy quantum | **×11.51 Wh** per pulse-unit → kWh | metering | [DOC] flowcharts |
| Room fault band | `PITO − TH > 1 °C` | fault detect | [DOC] |
| Tank-cold band | `TSTAV − TS > 5 °C` (too cold) | fault / aux-heat | [DOC] |
| Freeze guard | `TAP < 5 °C` | water fault | [DOC] |
| Exchanger cooling | `TAM − TAP > 1 °C` sustained **> 16 min** | air fault | [DOC] |
| Pulse timeout | no heating pulse in **64 min** | water fault | [DOC] |
| Tank-full lockout (aux) | tank **> 95 °C** → no aux-heat | aux-heat | [DOC] |
| Tank over-temp LED | tank **> 90 °C** → yellow "tank full" | display | [DOC] |
| VH time constant | ~**20 min** | mixing valve model | [DOC] |
| Aux window, option 2 | **22:00–06:00** (max 8 h) | aux-heat | [DOC] |
| Aux window, option 3 | **23:00–07:00** | aux-heat | [DOC] |
| Aux full-time outdoor | weekly min **≤ −25 °C** ⇒ full 8 h | aux-heat curve | [DOC] |
| Aux zero-time outdoor | weekly min **≥ +15 °C** ⇒ 0 h (linear between) | aux-heat curve | [DOC] |
| Correction reserve | **2 h** left at end of aux window | aux-heat | [DOC] |
| Default TSTAV | **60 °C** | commissioning | [DOC] |

> **[TODO]** A few hysteresis / anti-cycle constants exist in the firmware as
> counters (`XRAM[0x27]`, `XRAM[0xF9]`, the `0x16` action countdown) but their
> exact reload values are not yet transcribed from the `.d48`. For a new build,
> pick sensible anti-short-cycle guards (e.g. min on/off times) rather than
> reproducing the originals. [FW]

---

## 4. Control-curve graph (`TSTAV` vs outdoor)

The manual's "TALON SÄÄTÖKÄYRÄSTÖ" (fig. 3) is a family of straight lines mapping
**outdoor temperature → calculated tank target `TSTAV`**, selected by the ATS
knob (curve labels 70/60/50/40 through the pivot). Sample calculated targets at
the −15 °C design point: ~**57.5 / 50 / 42.5 / 35 °C**. [DOC]

Implementation: store each curve as a 2-point line (slope + offset) keyed by the
ATS setting; clamp to the tank max. Reproduce the four printed lines; interpolate
for intermediate ATS.

---

## 5. Main loop — 8 blocks (once per minute)

From [`flowcharts_en.md`](flowcharts_en.md), corroborated by the block structure
in [`koti_lampo.c`](../disasm/koti_lampo.c):

1. **Measure temperatures** — read all PT100s (apply §3 conversion).
2. **Read setpoints** — base, ATS, AP, setback state → compute `PITO`, `TSTAV`.
3. **Room control** — drive VA1/VA2/VH to hold `PITO` (see §5.1).
4. **Metering** — count VM (and LVM if fitted) pulses; accumulate kWh (×11.51 Wh),
   emit AE/LE pulses.
5. **Solar program** — run collector→tank exchanger loop (PAK, PM1/PM2, VP).
6. **Aux-heat program** — decide/time supplementary heat (see §6).
7. **Fault detection** — water- and air-circulation tests (see §7).
8. **Snow melt** — independent 30-min cycle.

### 5.1 Room loop actuator semantics

- **VA1** (return) is **open when de-energised**, **VA2** (bypass) **closed when
  de-energised** — so loss of control fails to a defined "heat through Kotilämpö"
  state. With no flow through the Kotilämpö unit, `THM = TAM` within 0.2 °C once
  settled (a useful self-test). [DOC]
- **VH** is a modulating 3-way valve with a ~20 min time constant; model it as a
  slow integrator, not an on/off output. [DOC]
- Manual fallback (also the firmware's fault escape): force VA1/VA2 to manual and
  open VH sufficiently to keep heat flowing. [DOC/FW]

---

## 6. Aux-heat (supplementary heat) program

- **Never** switch aux-heat on if tank **> 95 °C**. [DOC]
- **Option 1** — base option per the control curve.
- **Option 2** — target set once/day at 22:00 from the curve; heat window
  **22:00–06:00**, capped at 8 h. Heating time scales **linearly** with the
  week's minimum outdoor temp: full 8 h at ≤ −25 °C → 0 h at ≥ +15 °C. When the
  computed time < 8 h, trim it **from the start (evening)** so control always
  ends on a full hour; leave a **2 h** correction reserve at the end. Apply a
  correction to the next night if the target was missed/overshot, accounting for
  tank-temp change between calc and switch-on. [DOC]
- **Option 3** — as option 2 but window **23:00–07:00**. [DOC]
- Outside the aux window on options 2/3 the automation gives no aux control;
  "day electricity" is handled by a separate tank-top thermostat (also guarantees
  DHW temperature outside the heating season). [DOC]

---

## 7. Fault detection

Two alarms: **water-circulation** and **air-circulation** (red LEDs).

**Water-circulation fault** if any of:
1. `TAP < 5 °C` (freeze risk) → take tank-top water into circulation, stop PAK.
2. No heating pulse (VM) within 64 min.
3. `PITO − TH > 1 °C` **and** tank too cold (`TSTAV − TS > 5 °C`).

**Air-circulation fault** if any of:
1. `TAM − TAP > 1 °C` sustained > 16 min → stop PAK.
2. `PITO − TH > 1 °C` **and** tank NOT too cold (≤ 5 °C below target).

If no tank sensor is fitted, air-fault case 2 also covers water-fault case 3. The
fault program also limits tank-top water use; the limit can later drop room temp
and light the air-fault LED. [DOC] The user/service action flowcharts for each
fault are transcribed in [`toiminta_ja_kaytto_en.md`](toiminta_ja_kaytto_en.md).

---

## 8. Persistence

The original stores user settings (clock, setback schedule, setpoints, energy
totals) in the `VA2101` EEPROM. A new build replaces this with the MCU's own
flash/NVS; just persist the same set and zero the energy totals on reset. [FW]

Display selector items to reproduce (front-panel counter): clock; 1 room temp;
2 collector temp; 3 water temp riser to battery; 4 water temp after battery;
5 water temp after solar battery; 6 tank temp; 7 consumed kWh; 8 solar kWh;
9 consumed DHW m³; 0 circulated heating water m³. [DOC]

---

## 9. What is NOT needed from the original

- The 8035 port/expander protocol (P8243 nibble bus, P2 strobes, MC14508B latch),
  the BCD math library, and the time-to-count PT100 read — all replaced by native
  MCU I/O and floating-point. They are documented in
  [`koti_lampo.c`](../disasm/koti_lampo.c) only as evidence that the block
  structure and constants above were read correctly.
- Cycle-accurate timing. The control is a 1-minute loop; only the 230 VAC
  switching (VH/PAK) needs proper drivers, not tight real-time from the host.

---

## 10. Open items before coding

1. **[TODO]** Measure the DB25 pinout (blocker) — worksheet in `db25_replacement.md`.
2. **[TODO]** Digitise the four control-curve lines into slope/offset per ATS.
3. **[TODO]** Choose anti-short-cycle guard times (originals not fully transcribed).
4. **[TODO]** Confirm PAK 3-speed selection logic (transformer taps vs conditions).
5. **[TODO]** Decide whether to keep the backend X10/X11 (AVM2/AVO2) analog cards
   or replace their function on the new board.
