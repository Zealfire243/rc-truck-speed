# rc-truck-speed

A scratch-built RC speed truck: an inherited **Nikko 14620B** body (with its very good spoiler) over
a stretched, scratch-built chassis with **two brushless motors, each parallel to the axle it
drives**, each commutated by its own controller running **custom FOC firmware you wrote** —
STM32F405 on VESC-class hardware, one per axle. An ESP32 rides along as a supervisor — GPS,
telemetry, logging, and an advisory ground-speed estimate that can only ever *reduce* torque,
never raise it. Printed ABS/ASA chassis first, cut-and-bend
aluminum later.

**Target: past 70 mph, GPS-verified.** Not on the first outing, and not on printed parts. It also
has to survive **3–5 minute RC-course runs**, which is a different and in places harder thermal
problem than the speed run — see [BOM.md](docs/BOM.md#two-vehicles-one-truck).

> **One target, bench and truck alike: Flipsky FSESC6.7 Pro boards** — an STM32F405 bolted to a
> 70 A inverter, one per motor. **You write the FOC; you never design a PCB.** An earlier plan
> spent Phases 1–2 on a cheap Nucleo-G474RE bench rig so the expensive mistakes landed somewhere
> harmless; that rig was dropped, and what replaces it — stock VESC firmware as the known-good
> reference, the gate driver's own overcurrent protection, a starved bench supply, and a sealed
> spare board — is in
> [BOM.md](docs/BOM.md#decision-no-stage-1-rig-straight-to-the-fsesc). Read
> [Phase 1a](docs/BUILD_PHASES.md#phase-1a--the-board-before-a-motor-exists) before powering
> anything.

**Start here: [docs/BUILD_PHASES.md](docs/BUILD_PHASES.md).** Phase-by-phase, each one ending in a
specific checkable result, from "one motor spinning under your own current loop" through a printed
mule to an aluminum chassis at speed.

> This is a **separate vehicle** from [rc-truck-crawler](../rc-truck-crawler), not its Phase 6 MK2 —
> that stays an open someday-build in the crawler repo. What carries over here is the *approach*:
> two-tier control split, JSON-lines telemetry, host-side tuning, and phase gates you don't skip.

## Repo layout

| Path | What |
|---|---|
| [`motor-ctrl/`](motor-ctrl/) | Motor controller firmware (C) — the hard real-time layer. Single-axle 3-phase FOC, current sensing, rotor observer, RC input, and every safety interlock. Owns the motors outright. **Builds for the STM32F405 on bench and flight hardware alike; the vehicle runs two copies, one per axle.** |
| [`supervisor/`](supervisor/) | ESP32 (ESP-IDF, C) — GPS, telemetry logging, WiFi streaming, host link. **Not in the control path.** You must be able to yank it at speed and have nothing happen. |
| [`host-tools/`](host-tools/) | Laptop-side Python — the gearing solver, the acceleration simulator, live telemetry plotting, and interactive gain tuning over the serial link. |
| [`hardware/`](hardware/) | CAD (`cad/`) and print-ready (`print/`) files. Every structural part is designed as a flat blank plus bend lines from day one — see [MECHANICAL.md](docs/MECHANICAL.md). |
| [`docs/`](docs/) | The phase plan, architecture, wiring, protocol, safety, mechanical design rules, and a phase-gated BOM. |

## Quick start

Motors, pack, ESP32-S3 and both Flipsky FSESC6.7 Pro boards are in hand; the drivetrain
is still being sourced. The solver is still the first thing to run, though — it is what every
gearing and tire decision still open is measured against:

```bash
cd host-tools && uv run gearing.py --sweep    # top speed vs tire and ratio
cd host-tools && uv run accel.py --sweep      # acceleration, and what limits it
```

That prints the tire × final-drive × pack table the whole build is designed around. See
[Phase 0](docs/BUILD_PHASES.md#phase-0--numbers-before-parts).

```bash
# Motor controller — needs arm-none-eabi-gcc and STM32Cube.
# ⚠ CMakeLists.txt and pin_map.h still target the G474 from the dropped bench rig.
# Retargeting them to the F405 is the first task of Phase 1 — see motor-ctrl/README.md.
cd motor-ctrl && cmake -B build && cmake --build build

# Supervisor — needs ESP-IDF v5.5+
cd supervisor && idf.py set-target esp32 && idf.py build flash monitor

# Live telemetry plot / interactive tuning
cd host-tools && uv run telemetry_plot.py
cd host-tools && uv run tune.py
```

## The numbers this is designed around

**Two tire configurations, one drivetrain** — 2,850 kV on 4S, 2.5:1 final drive, and you change
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
    at the wheels              150 W
    electrical                 200 W
    pack current              13.9 A   (6.9 A per motor, steady state)
```

Read that twice, because it reframes the whole project: **power is not the constraint.** Holding
70 mph against drag on a body this small takes about as much power as a bright desk lamp. What
actually stops you is RPM, tires letting go, and **directional stability** — with the body measured,
the 50/50 layout goes yaw-divergent around **50 mph**, and the fix is ~60% front weight bias, not the
spoiler. Design accordingly — see
[MECHANICAL.md](docs/MECHANICAL.md#-directional-stability--the-layout-as-drawn-is-divergent-at-50-mph)
and [SAFETY.md](docs/SAFETY.md).

## Body dimensions (measured, the donor)

| | |
|---|---|
| Bumper to bumper | 273 mm (10¾ in) |
| Wheel well centers, front to back | 165 mm (6½ in) |
| Wheel arch opening | **55 mm** — sets max tire OD |
| Width, skinniest (cab/bed sides) | 105 mm |
| Width, widest (fender flares) | **124.3 mm** |
| Height, body bottom to cab top | 80 mm |
| Spoiler | 54 × 160 mm crescent, pressure point **20 mm behind the rear axle** |

The chassis under it is deliberately **not** 165 mm. It stretches to ~190 mm wheelbase and
**~135 mm track**, with the body sitting over it. The wheelbase stretch helps yaw stability, though
less than an earlier draft of this claimed — it is a weak linear lever, worth ~5% between 180 and
190 mm, and kept at 190 because it is free here rather than because it buys much. The track went to
135 mm to buy
half-shaft length for the Phase 8 ride-height actuators; it costs **5.35 mm per side** of hub
standing proud of the fender flares, and it raises the rollover CG ceiling from 57 to 61 mm as a
bonus. The reasoning is in [MECHANICAL.md](docs/MECHANICAL.md#geometry).
