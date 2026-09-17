# motor-ctrl — STM32F405

The hard real-time layer. Single-axle 3-phase FOC, current sensing, rotor observer,
CRSF input, and every safety interlock. See [../docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md).

> **One target, and the vehicle runs two copies of this firmware — one per axle.** Everything
> builds for the **STM32F405** on a Flipsky FSESC6.7 Pro, on the bench and in the truck alike.
> An earlier plan had Phases 1–2 building for a Nucleo-G474RE + X-NUCLEO-IHM08M1 bench rig, with
> a HAL port at Phase 2b; that rig was dropped and there is no port. See
> [../docs/BOM.md](../docs/BOM.md#decision-no-stage-1-rig-straight-to-the-fsesc).
>
> **Not dual-channel any more.** Each controller drives one motor. What used to be a second FOC
> instance on TIM8 is now a second board running this same firmware, coordinated over a peer link
> and cross-wired fault lines.

## The port boundary — keep it clean from day one

**The porting argument for this boundary is gone; the debugging argument is not.** When the
motor does something inexplicable, the first question is always "is this my math or my
hardware?", and that question is only answerable cheaply if the two are separable:

| Ports unchanged | Rewritten at Phase 2b |
|---|---|
| `foc.c` — Clarke/Park/SVM, pure math | `pin_map.h` — every pin moves |
| `observer.c` — the estimator | `inverter.c` — timer, deadtime, gate driver SPI init |
| `speed_loop.c` — control policy | `current_sense.c` — new shunt value and amplifier gain |
| `safety.c` — interlock policy | |

**If Phase 2b has you editing `foc.c`, stop and find out why.** The algorithms are
hardware-independent; a change there means something leaked across the boundary.

## What isn't in this repo

> ⚠ **The build is not retargeted yet.** `CMakeLists.txt` still asks for `CUBE_G4_PATH` and
> `pin_map.h` is still 353 lines of G474 assignments, both inherited from the dropped bench
> rig. **Retargeting them to the F405 is the first firmware task of Phase 1**, alongside
> `inverter.c` and `current_sense.c`. Everything below describes the F405 target you are
> moving to, not what a fresh clone builds today.

Three files are CubeMX-generated and board-specific, so they aren't checked in.
Generate them for **STM32F405RGTx** and drop them here:

| File | Where it comes from |
|---|---|
| `STM32F405RGTx_FLASH.ld` | CubeMX project generation |
| `startup_stm32f405xx.s` | CubeMX, or the CMSIS device pack |
| `system_stm32f4xx.c` | CubeMX, or the CMSIS device pack |

Add the latter two to `add_executable()` in `CMakeLists.txt` when you have them.

Using CubeMX for peripheral init is a reasonable choice here and not a
compromise — the ADC injected-conversion setup and the TIM1 complementary
+ deadtime + break configuration are exactly the things its GUI is good at, and
exactly the things that are miserable to get right from the reference manual.
Generate the init, then keep the control code in these files. There is one target now,
so you run CubeMX once.

## Build

```bash
# once retargeted; a fresh clone today still wants CUBE_G4_PATH
export CUBE_F4_PATH=$HOME/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.0
cmake -B build && cmake --build build
```

`BRINGUP_APP` is ON by default, so a fresh clone builds the Phase 1 bench app.
Turn it off from Phase 2 onward:

```bash
cmake -B build -DBRINGUP_APP=OFF
```

> **`AXLES_POPULATED` is gone**, replaced by `AXLE_SELF`. Nothing counts axles any more — the
> answer is always one. What varies is *which* one, because it decides peer-link identity and the
> torque-split sign:
>
> ```bash
> cmake -B build -DBRINGUP_APP=OFF -DAXLE_SELF=AXLE_REAR
> ```
>
> `AXLE_COUNT` stays 2: the arrays still hold two axles' worth of **state** (own, measured + peer,
> received over the link), which the desync interlock and
> [PROTOCOL.md §2](../docs/PROTOCOL.md) both need. **Two axles of data, one axle of drive.**

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

1. `inc/pin_map.h` — pins, drivetrain constants, `AXLE_SELF`, and the speed cap ceiling.
   `POLE_PAIRS` is **1** for the 2-pole QuicRun 3650. Count the magnets and re-verify if you
   change motor — get it wrong and every RPM and angle the observer produces is out by 2×.
2. `inc/foc.h` — what FOC does and the pipeline (`src/foc.c` is fully written)
3. `inc/observer.h` — **the hard part.** Read before starting Phase 2.
4. `inc/current_sense.h` — why sample timing is Phase 1's whole criterion
5. `inc/safety.h` — every interlock (`src/safety.c` is fully written)
