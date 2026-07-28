# DB25 passive capture checklist

A repeatable procedure for recording what actually crosses the CPU board's I/O
pins during normal operation, without touching or influencing it. This is what
settles the questions [`emu/`](../emu/) (the firmware emulator) hit its limit
on: the AVM2 card's measurement encoding, the `P6`/`P7` handshake, and real
timing for the GUESS-tagged signals in
[`db25_replacement.md`](db25_replacement.md).

**This is a listen-only capture.** Nothing is driven onto the board; you are
only clipping probe leads to already-present signals. That's what makes it
safe to do on a live, running unit.

## Why these signals, specifically

The emulator ([`../emu/README.md`](../emu/README.md)) ran the real firmware
and got stuck exactly where a passive capture picks up:

- The ISR asserts a strobe on `P2` and then polls `P6`/`P7`, but a model with
  no backend to answer it just loops. A live capture shows what actually
  answers.
- The firmware measures each sensor by asserting the `P1.5` strobe, delaying,
  then counting edges on `T1` (`strt cnt`) — confirmed by emulation, but the
  count→temperature **scaling** can only come from a real measurement, since
  nothing simulates the backend AVM2 card.

So the capture target is deliberately narrow: the `P2` strobe (to know which
operation is in flight), the `P6`/`P7` byte pair, the `P1.5`/`T1` measurement
handshake, and `INT`/`T0` to anchor everything to the mains tick and boot mode.

## Where to probe — CPU-board side only

Everything below is **on the head-unit side of the opto isolation barrier**
(the CPU board's own pins), not on the DB25 connector or inside the backend
cabinet:

- Probing here is 5V TTL logic — low voltage, low risk.
- **You do not need to open the backend cabinet or touch anything mains-side.**
  The opto barrier exists so the interesting logic is already safely available
  on your side of it.

| Signal | Where (per `docs/db25_replacement.md` / `docs/bottom_pcb_chip_map.html`) | Why it matters |
| --- | --- | --- |
| `P2` (all 8 bits, or at least the high nibble) | CPU (`P8035L`) port 2 pins | Decodes which operation is in flight — expander strobes `8F 9F AF CF DF EF` are all on this port |
| `P6`, `P7` | CPU port pins, or the `P8243` expander output side | One 8-bit port pair (confirmed by emulation — `P7` = low nibble, `P6` = high), the primary payload for whatever the ISR is exchanging |
| `P1.5` | CPU port 1, bit 5 (`0x20`) | Measurement strobe — asserted before each sensor read |
| `T1` | CPU pin 39 (test/counter input) | The edge-counted measurement signal — burst length after each `P1.5` strobe *is* the sensor reading |
| `INT` | CPU external interrupt pin | Mains zero-cross tick; use as a trigger/timing anchor |
| `T0` | CPU pin 14 | Boot-mode select (cold vs. warm start) — only needs a one-off check at power-on, not continuous capture |

If you have spare channels, also tap `P5` (candidate relay/channel group
select) and one or two DB25 pins from the table in `db25_replacement.md` that
are still marked "no firmware match found yet" (pins 9/10) — extra context at
no extra risk since they're already-identified low-voltage points.

## Equipment

**Best option: a USB logic analyzer** (an 8+ channel Saleae-compatible/DSLogic/
FX2-based unit — commonly ~$10-30, several already used elsewhere in this kind
of retro-computing work). Needs:

- **≥1 MS/s sample rate.** This is 1980s TTL logic; the fastest thing here
  (the `T1` counting burst) is still kHz-range at most, so this is generous
  headroom, not a tight requirement.
- **8+ channels**, so the whole signal group above fits in one capture instead
  of several separate sessions that then need re-aligning against each other.

A cheap USB oscilloscope works as a **first look** at one or two signals (e.g.
confirming `T1` pulse width/rate once you know where to look), but can't
correlate the whole handshake at once — don't use it as the primary tool.

**Do not use a bare microcontroller (Arduino/ESP32) GPIO capture as the
primary tool.** Sampling jitter from a general-purpose loop blurs exactly the
fast edges this capture needs (`T1` counting, `P1.5` strobe timing). Fine for
a rough sanity check, not for the real data.

## Capture procedure

1. **Connect probes + shared ground** to the pins in the table above, board
   powered off.
2. **Power up normally** — mains connected, backend cabinet attached as usual.
   This is passive: you are only listening, so there is no reason to run the
   system in any special or reduced configuration.
3. **Trigger the capture** on a `P2` transition to `0xAF` (the read-back
   strobe identified by emulation), or on `INT` rising if your tool makes that
   easier to set up — either gives you a known, repeatable starting point per
   capture window.
4. **Capture length:** a few hundred milliseconds per window, repeated across
   several mains-tick intervals. That should span multiple `P1.5`/`T1`
   measurement bursts across the sensor-scan sequence the factory test box
   already documents (see `db25_replacement.md`'s "Confirmed signal subset"
   section) — long enough to see more than one sensor's reading, short enough
   to keep the trace file a manageable size.
5. **Export** as whatever your tool's native format is, plus a CSV or VCD if
   it can produce one (VCD in particular is easy to script against).
6. **Note alongside the capture:** which physical sensor(s) were at roughly
   what temperature at capture time, if known — even a rough number gives a
   real point to check the recovered count→temperature scaling against,
   alongside the manual's calibration points (`TSTAV` 60°C default, 52°C/70°C
   test resistors).

## After the capture

Hand the exported trace back for analysis — decoding follows the same
discipline as the emulator: no assumptions, only what the capture actually
shows, checked against the emulator's predicted `P2` strobe sequence before
trusting any interpretation of it. The concrete goals are:

- Confirm or correct the `P6`/`P7` = one 8-bit port finding against real data.
- Recover the `T1` count → resistance → temperature scaling.
- Fill in the DB25 pins still marked unresolved in `db25_replacement.md`
  (measurement strobe pin-out, the 4th actuator drive, the second energy
  pulse, `LL`, and the pin 11 direction recheck already flagged there).

This is the one remaining step that neither the disassembly nor the emulator
can supply on their own — everything else has been derived from static
analysis or execution, but the actual measurement scaling only exists on the
live board.
