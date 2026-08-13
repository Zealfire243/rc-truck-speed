# rc-truck-speed

A scratch-built RC speed truck: an inherited **Nikko 14620B** body (with its very good spoiler) over
a stretched, scratch-built chassis with **two brushless motors, one per axle**, both commutated by a
single **STM32G4 running custom dual-channel FOC**. An ESP32 rides along as a supervisor — GPS,
telemetry, logging — with no authority over the motors. Printed ABS/ASA chassis first, cut-and-bend
aluminum later.

**Target: past 70 mph, GPS-verified.** Not on the first outing, and not on printed parts.

**Start here: [docs/BUILD_PHASES.md](docs/BUILD_PHASES.md).** Phase-by-phase, each one ending in a
specific checkable result, from "one motor spinning under your own current loop" through a printed
mule to an aluminum chassis at speed.

> This is a **separate vehicle** from [rc-truck-crawler](../rc-truck-crawler), not its Phase 6 MK2 —
> that stays an open someday-build in the crawler repo. What carries over here is the *approach*:
> two-tier control split, JSON-lines telemetry, host-side tuning, and phase gates you don't skip.

## Repo layout

| Path | What |
|---|---|
| [`motor-ctrl/`](motor-ctrl/) | STM32G4 (C) — the hard real-time layer. Dual 3-phase FOC, current sensing, rotor observer, RC input, and every safety interlock. Owns the motors outright. |
| [`supervisor/`](supervisor/) | ESP32 (ESP-IDF, C) — GPS, telemetry logging, WiFi streaming, host link. **Not in the control path.** You must be able to yank it at speed and have nothing happen. |
| [`host-tools/`](host-tools/) | Laptop-side Python — the gearing solver, live telemetry plotting, and interactive gain tuning over the serial link. |
| [`hardware/`](hardware/) | CAD (`cad/`) and print-ready (`print/`) files. Every structural part is designed as a flat blank plus bend lines from day one — see [MECHANICAL.md](docs/MECHANICAL.md). |
| [`docs/`](docs/) | The phase plan, architecture, wiring, protocol, safety, mechanical design rules, and a phase-gated BOM. |

## Quick start

Nothing has been bought yet, so the first thing to run is the solver, not a compiler:

```bash
cd host-tools && uv run gearing.py --sweep
```

That prints the tire × final-drive × pack table the whole build is designed around. See
[Phase 0](docs/BUILD_PHASES.md#phase-0--numbers-before-parts).

```bash
# Motor controller — needs arm-none-eabi-gcc and STM32CubeG4 at $CUBE_G4_PATH
cd motor-ctrl && cmake -B build && cmake --build build

# Supervisor — needs ESP-IDF v5.5+
cd supervisor && idf.py set-target esp32 && idf.py build flash monitor

# Live telemetry plot / interactive tuning
cd host-tools && uv run telemetry_plot.py
cd host-tools && uv run tune.py
```

## The numbers this is designed around

**Two tire configurations, one drivetrain** — 2,441 kV on 4S, 3.5:1 final drive, and you change
top speed by changing wheels:

| Profile | Tire | Top speed | Wheel RPM |
|---|---|---|---|
| `street` | 55 mm, tucks in the stock arches | 55 mph | **8,537** |
| `speed` | 70 mm, wheels proud | 70 mph | **8,537** |

Wheel RPM is identical in both, because the motor's RPM ceiling divided by the final drive sets it
outright — tire diameter only changes how far the truck travels per revolution. So the tire never
spins faster in the fast configuration than in the slow one. Run `gearing.py --swap`.

```
  POWER  (confirm, don't design to)
    at the wheels              119 W
    electrical                 175 W
    pack current              12.2 A   (6.1 A per motor, steady state)
```

Read that twice, because it reframes the whole project: **power is not the constraint.** Holding
70 mph against drag on a body this small takes about as much power as a bright desk lamp. What
actually stops you is RPM, tires letting go, and yaw stability on a short wheelbase.
Design accordingly — see [SAFETY.md](docs/SAFETY.md) and [MECHANICAL.md](docs/MECHANICAL.md).

## Body dimensions (measured, the donor)

| | |
|---|---|
| Bumper to bumper | 273 mm (10¾ in) |
| Wheel well centers, front to back | 165 mm (6½ in) |
| Wheel arch opening | **55 mm** — sets max tire OD |
| Width | 105 mm |

The chassis under it is deliberately **not** 165 mm. It stretches to ~190 mm wheelbase and ~125 mm
track, with the body sitting over it and the wheels slightly proud of the wells — yaw stability
scales hard with wheelbase, and that stretch is the cheapest stability in the entire project. The
reasoning is in [MECHANICAL.md](docs/MECHANICAL.md#geometry).
