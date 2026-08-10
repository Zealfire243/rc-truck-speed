# Architecture

Two processors, one rule: **the thing that can kill the motors is not the thing running WiFi.**

```
        CRSF receiver
              │  (UART, 2-4 ms)
              ▼
     ┌─────────────────────┐        UART, JSON-lines      ┌──────────────────┐
     │     STM32G4         │◄───────────────────────────► │      ESP32       │
     │  hard real-time     │                              │   supervisor     │
     │                     │                              │                  │
     │  • dual 3-phase FOC │                              │  • GPS, 10 Hz    │
     │  • current sensing  │                              │  • logging       │
     │  • rotor observer   │                              │  • WiFi stream   │
     │  • RC input         │                              │  • host link     │
     │  • speed cap        │                              │                  │
     │  • ALL interlocks   │                              │  NO motor        │
     │                     │                              │  authority       │
     └──────┬───────┬──────┘                              └──────────────────┘
            │       │
        TIM1│       │TIM8
            ▼       ▼
       ┌────────┐ ┌────────┐
       │ front  │ │  rear  │      two 3-phase inverters
       │inverter│ │inverter│
       └───┬────┘ └───┬────┘
           ▼          ▼
        front       rear
        motor       motor
```

## Why one G4 instead of two

The obvious dual-motor layout is one controller per axle. It's worse here.

An STM32G474 has **two advanced-control timers** (TIM1 and TIM8), each with three complementary
PWM pairs and its own break input, plus **five ADCs**. That is exactly a dual 3-phase inverter
controller, and ST supports the configuration directly. Both channels then run off **one control
ISR on one clock**.

That collapses the biggest risk in a dual-motor build. With two independent controllers, "the axles
disagree" is a distributed-systems problem — two boards with two clocks guessing at each other over
a link, each having to decide whether the other is alive. With one controller it's a local
consistency check on two variables in the same tick, and the response (cut both) is a single
register write to two break inputs.

**Cost of the bet:** one chip is a single point of failure, and the control ISR now has to complete
two full FOC pipelines within one PWM period. Phase 3 exists to measure that deadline rather than
assume it.

## Why the receiver goes to the G4

The crawler's best structural property is that killing the Raspberry Pi mid-run doesn't affect
driving. That property comes from the coordinator having no authority, and it only holds if the
control input doesn't route through it.

So the CRSF receiver wires **directly to the G4**. The ESP32 cannot command torque, cannot raise
the speed cap, and cannot clear a fault latch. It observes, logs, and talks to your laptop.

This is tested explicitly in [Phase 5](BUILD_PHASES.md#phase-5--supervisor-tier--gps): pull the
ESP32's power at 20 mph and keep driving. If that test ever fails, something has crept into the
control path and needs to come out.

The corollary: **anything safety-relevant lives in the G4.** Speed cap, overcurrent, overtemp,
RC-loss failsafe, axle desync, estop latch. The ESP32 can *ask* for things; it is never the reason
something is safe.

## Control stack, top to bottom

| Layer | Rate | Lives in | Notes |
|---|---|---|---|
| Human | ~5 Hz | you | 150–250 ms reaction; dominates every other latency in this table |
| CRSF link | 250–500 Hz | receiver → G4 | 2–4 ms. The slowest thing in the machine that isn't you. |
| Vehicle policy | 200–500 Hz | `speed_loop.c` | Traction control, launch, torque split, speed cap |
| Velocity loop | 1–2 kHz | `speed_loop.c` | Per-axle RPM control |
| Current loop (FOC) | 16–20 kHz | `foc.c` | d/q PI, one PWM period of latency |
| Commutation | 16–20 kHz PWM | TIM1/TIM8 + inverter | SVPWM, deadtime enforced in hardware |
| Overcurrent trip | asynchronous | comparator → BKIN | **Hardware.** Kills PWM with no software involved. |

The bottom row matters more than it looks. Software overcurrent protection has to survive the
software being wrong. Tying a comparator to the timer break input means a fault kills the gates
whether or not the ISR is still running. See [WIRING.md](WIRING.md).

## Wheel-speed sensing

Falls out of FOC for free: the rotor observer already estimates electrical angle and speed, so
mechanical RPM is that divided by pole pairs, and wheel speed is that divided by the final drive.
No encoders, no hall sensors, no extra wiring.

The catch is that this measures **wheel** speed, not **ground** speed, and the difference between
those two is wheelspin — which is precisely the thing you most want to measure and precisely when
the measurement is least trustworthy. That's why GPS is in the build:

```
slip = (wheel_speed - gps_ground_speed) / wheel_speed
```

GPS at 10 Hz is far too slow to close a traction-control loop against. It is exactly right for
*tuning* one: you drive, you log both, and you calibrate the fast wheel-only slip estimator against
the slow ground truth afterwards. Phase 5 establishes the correlation; Phase 6 tunes against it.

## Failure modes and what happens

| Failure | Detected by | Response |
|---|---|---|
| Overcurrent | Hardware comparator → TIM break | PWM gates off immediately, latched, no software |
| Phase overtemp | Thermistor on ADC | Torque fold-back, then cut |
| RC link lost | CRSF frame timeout in `rc_input.c` | Torque to zero, coast. **Not** brake — braking a truck at 70 mph with no steering input is worse than coasting. |
| Axle desync | `safety.c`, both channels compared each tick | **Both** channels cut. Never one. |
| Observer loses the rotor | Angle-tracking error threshold | That channel cuts, which trips desync, which cuts both |
| ESP32 dies | Nothing needs to detect it | Driving continues; telemetry stops |
| G4 dies | Watchdog | Reset into a zero-torque state; receiver failsafe as backstop |

**Coast, not brake, on RC loss** is a deliberate departure from what a hobby ESC does. A hobby ESC
brakes on failsafe because it's usually in a car doing 20 mph with a driver watching. At 70 mph on
a 190 mm wheelbase, sudden braking with no steering authority puts the truck sideways. Coasting is
undignified and much safer.

## What carried over from the crawler

- **The two-tier split and its test.** Different chips, same rule, same explicit pull-the-plug test.
- **JSON-lines telemetry over UART**, so `host-tools/` ports across nearly unchanged. See
  [PROTOCOL.md](PROTOCOL.md).
- **The driver-interface discipline.** `inverter.h` is to this project what `motor_driver.h` was to
  the crawler: if a hardware change forces the interface to change, the abstraction was wrong.
- **Phase gates with pass criteria you can fail.** A checkbox you can't fail isn't a gate.

What did *not* carry over: brushed motors, encoders, the dog clutch, torque vectoring across four
independent wheels, and the Raspberry Pi tier. This truck has two motors and needs to be light.
