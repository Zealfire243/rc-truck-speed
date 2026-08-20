# motor-ctrl — STM32G474 (bench) → STM32F405 (flight)

The hard real-time layer. Single-axle 3-phase FOC, current sensing, rotor observer,
CRSF input, and every safety interlock. See [../docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md).

> **One firmware image, two targets, and the vehicle runs two copies of it — one per axle.**
> Phases 1–2 build for the Nucleo-G474RE + X-NUCLEO-IHM08M1 bench rig. From Phase 2b the same
> source builds for the **STM32F405** on a Flipsky FSESC6.7 Pro, which is what ships in the truck.
> The G474 stays on the bench as the reference rig. See
> [../docs/BOM.md](../docs/BOM.md#the-three-stage-hardware-plan).
>
> **Not dual-channel any more.** Each controller drives one motor. What used to be a second FOC
> instance on TIM8 is now a second board running this same firmware, coordinated over a peer link
> and cross-wired fault lines.

## The port boundary — keep it clean from day one

This is the single most consequential thing about how you write these files. Phase 2b is a week
or a month depending entirely on whether this line held:

| Ports unchanged | Rewritten at Phase 2b |
|---|---|
| `foc.c` — Clarke/Park/SVM, pure math | `pin_map.h` — every pin moves |
| `observer.c` — the estimator | `inverter.c` — timer, deadtime, gate driver SPI init |
| `speed_loop.c` — control policy | `current_sense.c` — new shunt value and amplifier gain |
| `safety.c` — interlock policy | |

**If Phase 2b has you editing `foc.c`, stop and find out why.** The algorithms are
hardware-independent; a change there means something leaked across the boundary.

## What isn't in this repo

Three files are CubeMX-generated and board-specific, so they aren't checked in.
For Stage 1, generate them for STM32G474RETx and drop them here (Stage 2 needs the
STM32F405RGTx equivalents):

| File | Where it comes from |
|---|---|
| `STM32G474RETx_FLASH.ld` | CubeMX project generation |
| `startup_stm32g474xx.s` | CubeMX, or the CMSIS device pack |
| `system_stm32g4xx.c` | CubeMX, or the CMSIS device pack |

Add the latter two to `add_executable()` in `CMakeLists.txt` when you have them.

Using CubeMX for peripheral init is a reasonable choice here and not a
compromise — the ADC injected-conversion setup and the TIM1 complementary
+ deadtime + break configuration are exactly the things its GUI is good at, and
exactly the things that are miserable to get right from the reference manual.
Generate the init, then keep the control code in these files. This applies to both
targets; you will run CubeMX twice.

## Build

```bash
export CUBE_G4_PATH=$HOME/STM32Cube/Repository/STM32Cube_FW_G4_V1.5.0
cmake -B build && cmake --build build
```

`BRINGUP_APP` is ON by default, so a fresh clone builds the Phase 1 bench app.
Turn it off from Phase 2 onward:

```bash
cmake -B build -DBRINGUP_APP=OFF
```

> ⚠ **`AXLES_POPULATED=2` is obsolete.** It selected a second FOC instance on the same chip.
> Each controller now owns exactly one axle; which axle it *is* becomes a build-time or
> stored-config choice (`AXLE=FRONT|REAR`), because it decides peer-link identity and the
> torque-split sign — not how many FOC instances run.

## Tests

The transforms are pure math with no hardware dependency, so they're testable
on your laptop right now — before a single part is bought:

```bash
cc -Iinc -lm -o /tmp/test_foc test/test_foc.c src/foc.c && /tmp/test_foc
```

Covers Clarke amplitude and balance, Park landing pure-d and pure-q at the
right angles, round-trip accuracy across a full electrical revolution, SVPWM
duty range and common-mode cancellation, and PI anti-windup recovery. Run it
any time you touch `foc.c` — every bug it catches is one you'd otherwise be
hunting with a motor spinning.

## Reading order

Start with the headers — they carry the design reasoning, the same way the
crawler's do.

1. `inc/pin_map.h` — pins, drivetrain constants, and the speed cap ceiling.
   ⚠ **`POLE_PAIRS` is currently 2 and must become 1** — the chosen QuicRun 3650 is a 2-pole
   motor. Get this wrong and every RPM and angle the observer produces is out by 2×.
2. `inc/foc.h` — what FOC does and the pipeline (`src/foc.c` is fully written)
3. `inc/observer.h` — **the hard part.** Read before starting Phase 2.
4. `inc/current_sense.h` — why sample timing is Phase 1's whole criterion
5. `inc/safety.h` — every interlock (`src/safety.c` is fully written)
