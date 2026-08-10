# motor-ctrl — STM32G474

The hard real-time layer. Dual 3-phase FOC, current sensing, rotor observer,
CRSF input, and every safety interlock. See [../docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md).

## What isn't in this repo

Three files are CubeMX-generated and board-specific, so they aren't checked in.
Generate them once for STM32G474RETx and drop them here:

| File | Where it comes from |
|---|---|
| `STM32G474RETx_FLASH.ld` | CubeMX project generation |
| `startup_stm32g474xx.s` | CubeMX, or the CMSIS device pack |
| `system_stm32g4xx.c` | CubeMX, or the CMSIS device pack |

Add the latter two to `add_executable()` in `CMakeLists.txt` when you have them.

Using CubeMX for peripheral init is a reasonable choice here and not a
compromise — the ADC injected-conversion setup and the TIM1/TIM8 complementary
+ deadtime + break configuration are exactly the things its GUI is good at, and
exactly the things that are miserable to get right from the reference manual.
Generate the init, then keep the control code in these files.

## Build

```bash
export CUBE_G4_PATH=$HOME/STM32Cube/Repository/STM32Cube_FW_G4_V1.5.0
cmake -B build && cmake --build build
```

`BRINGUP_APP` is ON by default, so a fresh clone builds the Phase 1 bench app.
Turn it off and set `AXLES_POPULATED=2` from Phase 3 onward:

```bash
cmake -B build -DBRINGUP_APP=OFF -DAXLES_POPULATED=2
```

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

1. `inc/pin_map.h` — pins, drivetrain constants, and the speed cap ceiling
2. `inc/foc.h` — what FOC does and the pipeline (`src/foc.c` is fully written)
3. `inc/observer.h` — **the hard part.** Read before starting Phase 2.
4. `inc/current_sense.h` — why sample timing is Phase 1's whole criterion
5. `inc/safety.h` — every interlock (`src/safety.c` is fully written)
