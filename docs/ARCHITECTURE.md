# Architecture

Two tiers, one rule: **the thing that can kill the motors is not the thing running WiFi.**

> The supervisor's role was originally "no authority, full stop." It is now **advisory input that
> can only subtract** — it may lower a torque limit and never raise one. See
> [the supervisor exception](#the-supervisor-exception--advisory-input-that-can-only-subtract) for
> why, and for the `min()` structure that enforces it.

```
                        CRSF receiver
                              │  (UART, 2-4 ms, split to both)
                 ┌────────────┴────────────┐
                 ▼                         ▼
      ┌────────────────────┐    ┌────────────────────┐
      │  FRONT controller  │    │  REAR controller   │
      │  STM32F405 (FSESC) │    │  STM32F405 (FSESC) │
      │                    │    │                    │
      │  • 3-phase FOC     │    │  • 3-phase FOC     │
      │  • current sensing │    │  • current sensing │
      │  • rotor observer  │    │  • rotor observer  │
      │  • RC input        │    │  • RC input        │
      │  • speed cap       │    │  • speed cap       │
      │  • ALL interlocks  │    │  • ALL interlocks  │
      └──┬──────┬───────┬──┘    └──┬───────┬──────┬──┘
         │      │       │          │       │      │
         │      │       └──────────┼───┐   │      │
         │      │   peer link  ┌───┘   │   │      │
         │      │   (data)     │       │   │      │
         │      │              ▼       ▼   │      │
         │      │        ╔═════════════════╗      │
         │      │        ║ CROSS-WIRED     ║      │
         │      │        ║ FAULT LINES     ║      │
         │      │        ║ (hardware, into ║      │
         │      │        ║  TIM1 BKIN)     ║      │
         │      │        ╚═════════════════╝      │
         │      │                                 │
         │      └────────────┐       ┌────────────┘
         │        UART       ▼       ▼   UART
         │              ┌──────────────────┐
         │              │      ESP32       │
         │              │   supervisor     │
         │              │  • GPS, 10 Hz    │
         │              │  • IMU fusion    │
         │              │  • logging       │
         │              │  • WiFi stream   │
         │              │                  │
         │              │  ADVISORY ONLY:  │
         │              │  may only REDUCE │
         │              │  torque, never   │
         │              │  raise it        │
         │              └──────────────────┘
         ▼
    ┌─────────┐                        ┌─────────┐
    │  front  │                        │  rear   │
    │  motor  │                        │  motor  │
    └─────────┘                        └─────────┘
```

Each controller is a **Flipsky Mini FSESC6.7 Pro** — an STM32F405 attached to a 70 A three-phase
inverter — running **your** firmware, not stock VESC firmware. See
[BOM.md](BOM.md#the-three-stage-hardware-plan).

## Why two controllers instead of one

**This reverses an earlier decision, and the earlier reasoning was not wrong.** It is worth
keeping both halves in view.

The original plan put both motors on one STM32G474, using TIM1 and TIM8 off a single control ISR.
The argument for it was genuinely strong: with one controller, "the axles disagree" is a local
consistency check on two variables in the same tick, and the response is one register write to two
break inputs. With two controllers it becomes a distributed-systems problem — two boards, two
clocks, each having to decide whether the other is alive.

**What changed is hardware availability, not the logic.** The one-G4 plan required two power stages
wired to one MCU, and no such product exists above ~15 A: every low-voltage eval board in the
industry stops there, and the X-NUCLEO-IHM08M1's two shields both hardwire to the same morpho pins,
so the second would need cut traces and flying leads onto TIM8. The alternative was designing a
power stage. VESC-class hardware resolves it by being an MCU *already attached* to a 70 A inverter —
but that pairing is one MCU per motor, and it is not separable.

So the choice was never "one G4 or two controllers" in the abstract. It was **"one G4 plus a PCB
project" or "two controllers off the shelf."**

Having been forced there, the two-controller layout does bring real advantages:

- **Independent failure domains.** One board dying does not take both axles.
- **A full advanced timer and ADC set per motor.** The deadline analysis Phase 3 existed to perform
  — can one ISR complete two FOC pipelines in one PWM period? — simply goes away.
- **No shared-resource contention** between channels, and no clever pin remapping to get wrong.
- **Two genuinely independent telemetry streams**, which is what the interlock wanted anyway.

**Cost of the new bet — carried forward, not dismissed:** axle desync is now a distributed problem,
exactly as the original document warned. The next section is how that is paid for.

## The desync interlock across two controllers

The old design compared both axles inside one tick. That is gone, and replacing it properly is the
single most important consequence of this architecture change.

**The ESP32 cannot mediate it.** Routing the interlock through the supervisor would give the
supervisor motor authority, which is the one rule this document has. So the two controllers need to
talk to each other **directly**, and the safety-critical part of that must not depend on software
being correct.

Two mechanisms, deliberately layered:

| | Mechanism | Carries | Fails how |
|---|---|---|---|
| **Data** | Peer link — CAN preferred, UART acceptable | Each axle's RPM, current, and a heartbeat | Absence of a peer frame for *N* ms is itself a fault |
| **Cut** | **Cross-wired fault lines**, each controller's fault output into the other's TIM1 break input | Nothing — it is a wire | Hardware. Kills gates with no software and no link involved |

The second row is the same reasoning that already puts the overcurrent comparator on BKIN rather
than in an ISR: **the protection has to survive the software being wrong.** A fault on either
controller pulls both sets of gates down through hardware, in the same way and on the same path as
an overcurrent trip. The peer link decides *whether* there is a desync; the fault line executes the
consequence.

**Heartbeat timeout is a fault, not a warning.** Silence from the other axle is indistinguishable
from that axle having died, and at 70 mph they must be treated identically. This is tested
explicitly at [Phase 3](BUILD_PHASES.md#phase-3--both-channels): unplug one controller's link
mid-run and confirm *both* axles cut.

**CAN is now the preferred transport** and this is a change from the old default. One bus carries
both controllers and the ESP32; the controllers use it for the peer heartbeat and the ESP32 only
listens. The F405 has bxCAN natively and the ESP32's TWAI peripheral speaks classic CAN. UART
point-to-point works and is what [PROTOCOL.md](PROTOCOL.md) currently specifies, but it needs three
links instead of one bus.

## Why the receiver goes to the controllers

The crawler's best structural property is that killing the Raspberry Pi mid-run doesn't affect
driving. That property comes from the coordinator having no authority, and it only holds if the
control input doesn't route through it.

So the CRSF receiver wires **directly to the motor controllers** — never through the ESP32. The
ESP32 cannot command torque, cannot raise the speed cap, and cannot clear a fault latch. It
observes, logs, fuses the IMU and GPS into a ground-speed estimate, and talks to your laptop.

**Steering angle is in the CRSF frame both controllers already decode** — do not route it through
the supervisor. The only thing the supervisor uniquely provides is the ground-speed estimate; see
[the supervisor exception](#the-supervisor-exception--advisory-input-that-can-only-subtract).

**With two controllers, split the receiver's TX to both controllers' RX.** A UART receive input is
high-impedance, so one transmitter drives two listeners without buffering. Both controllers decode
every CRSF frame independently and both apply their own failsafe. This is better than nominating a
primary that relays: no relay latency, no single point of failure, and an RC-loss failsafe that
fires on both axles from the same evidence at the same time.

> ⚠ **Only one controller may drive the receiver's RX line.** CRSF telemetry back to the
> transmitter is a shared bus, and two transmitters will collide. Designate one controller as the
> telemetry talker and leave the other's TX pin unconnected.

This is tested explicitly in [Phase 5](BUILD_PHASES.md#phase-5--supervisor-tier--gps): pull the
ESP32's power at 20 mph and keep driving. If that test ever fails, something has crept into the
control path and needs to come out.

The corollary: **anything safety-relevant lives in the controllers.** Speed cap, overcurrent,
overtemp, RC-loss failsafe, axle desync, estop latch. The ESP32 can *ask* for things; it is never
the reason something is safe, **and never the reason the truck can go faster.**

> **Both controllers enforce the speed cap independently.** It is not a value one of them owns and
> shares. Each has its own copy, phase-gated in firmware, and each will cut on its own if its axle
> exceeds it.

## Control stack, top to bottom

Everything from "Vehicle policy" down now runs **twice, once per controller**, on independent
clocks. That is the distributed-systems cost named above.

| Layer | Rate | Lives in | Notes |
|---|---|---|---|
| Human | ~5 Hz | you | 150–250 ms reaction; dominates every other latency in this table |
| CRSF link | 250–500 Hz | receiver → both controllers | 2–4 ms. The slowest thing in the machine that isn't you. |
| Vehicle policy | 200–500 Hz | `speed_loop.c` | Traction control, launch, torque split, speed cap |
| Velocity loop | 1–2 kHz | `speed_loop.c` | Per-axle RPM control |
| Current loop (FOC) | 16–20 kHz | `foc.c` | d/q PI, one PWM period of latency |
| Commutation | 16–20 kHz PWM | TIM1 + inverter, **on each controller** | SVPWM, deadtime enforced in hardware |
| Overcurrent trip | asynchronous | comparator → BKIN | **Hardware.** Kills PWM with no software involved. |

The bottom row matters more than it looks. Software overcurrent protection has to survive the
software being wrong. Tying a comparator to the timer break input means a fault kills the gates
whether or not the ISR is still running. See [WIRING.md](WIRING.md).

## Traction control across two axles

**This is what the peer link is for.** Worth stating plainly, because it is easy to confuse with
the CRSF split: the receiver split carries the *driver's* input; the peer link carries the axles'
*state to each other*. Three physically separate paths, three different jobs.

| Path | Carries | Rate | If it dies |
|---|---|---|---|
| **CRSF split** | Throttle, steering | 250–500 Hz | Both controllers fail safe independently |
| **Peer link (CAN)** | Each axle's wheel speed, current, heartbeat | 500–1000 Hz | **TC degrades**, then heartbeat timeout coasts both |
| **Fault lines** | Nothing — it is a wire | instant | Hardware cut, no software involved |

The CRSF split exists precisely so that **the command path is independent of the coordination
path.** If the receiver fed one controller which relayed throttle to the other, a peer-link failure
would leave one axle with no throttle source. Split, each controller always has its own command and
the peer link is purely coordination — so losing it degrades traction control rather than removing
drive.

### The latency budget is not tight

```
Classic CAN, 1 Mbit/s, 8-byte frame        ≈ 130 µs on the wire
Two nodes at 1 kHz                         = 26% bus load
Worst-case latency (one arbitration loss)  ≈ 260 µs
TC loop at 500 Hz                          = 2 ms period
                                           → peer data is 13% of a period stale
```

An order of magnitude of headroom. UART at 921600 gets similar numbers (~174 µs for a 16-byte
binary frame) and is an acceptable fallback. **CAN is preferred mainly because it is differential**
and this wiring runs alongside 50 A of switching.

> **The peer frame is not [the JSON-lines protocol](PROTOCOL.md).** It is a control-loop input with
> a hard latency budget: binary, fixed-length, 8 bytes. JSON lines are for telemetry to the ESP32.

### Each controller computes only its own torque

The failure mode to design out is both controllers independently deriving a torque *split*,
disagreeing slightly because their data is timestamped differently, and fighting each other.

The fix is structural: **make each controller a pure function of `(own_state, last_peer_frame)`
that outputs only its own torque.**

```
v_ref      = min(own_wheel_speed, peer_wheel_speed)
own_slip   = (own_wheel_speed - v_ref) / v_ref
own_torque = commanded_torque × f(own_slip)
```

Neither controller needs to *agree* with the other about anything — it only needs to *know* the
other's wheel speed. No negotiation, no arbitration, no primary. A dropped peer frame degrades one
controller's estimate for one tick instead of desynchronising a shared decision.

**This is why a designated-primary architecture is wrong here**, despite looking simpler. It makes
the front axle dependent on the rear for its ability to make torque at all, and adds a link round
trip to the command path.

### The case inter-axle slip cannot see

`min(front, rear)` is a ground-speed proxy **only while at least one axle has grip.** At launch all
four wheels can spin together — both axles slipping equally, inter-axle difference near zero, and
the estimator above sees nothing wrong. That is exactly the moment traction control matters most.

**Second signal, free, needs no extra sensor and no peer data:** the vehicle cannot accelerate
faster than `μg`, so a wheel climbing faster than that must be slipping.

```
a_max   = μg              → 10.8 m/s² at μ 1.1
α_wheel = a_max / r       → 309 rad/s² on a 70 mm tire
α_motor = α_wheel × FD    → ~6,900 rpm/s at 2.34:1
```

Motor RPM rising faster than that threshold means slip, even when both axles do it in unison. It
needs μ measured first — which is what [Phase 0](BUILD_PHASES.md#phase-0--numbers-before-parts)'s
drag rig produces — and margin for rotational inertia, but it costs nothing and covers precisely
the case the peer link misses.

**Third signal, best but not free:** measured longitudinal acceleration, compared against what the
wheels claim. That comes from the IMU, which lives on the supervisor — see the next section.

---

## The supervisor exception — advisory input that can only subtract

**This amends the "ESP32 has no authority" rule, which used to be absolute.** It is now graded, and
the grading is enforced structurally rather than by discipline.

### Why amend it at all

Detecting all-four-wheels-spinning needs an acceleration reference independent of the wheels. The
6-axis IMU produces exactly that — but it lives on the ESP32, alongside the GPS whose 10 Hz fix is
the only absolute speed truth on the vehicle. Duplicating that fusion onto two controllers, or
adding a second IMU, is worse than defining the interface carefully.

### What the supervisor actually contributes

Narrower than it first appears. The controllers already have, locally:

- wheel speed and current — their own, and the peer's
- **steering angle and throttle — already in the CRSF frame both controllers decode.** Do not route
  steering through the ESP32; it is sitting in a local variable already.

So the supervisor's unique contribution is one thing: **an estimate of actual ground speed.** Fast
signal from the IMU, slow truth from the GPS, fused where the GPS is, one number out.

> ⚠ **A 6-axis IMU gives *rate*, not angle.** Angular rate (gyro) and specific force
> (accelerometer); there is no angle output, and integrating to get one drifts. Traction control
> wants yaw *rate* anyway, so this costs nothing here — but do not design against a yaw *angle*
> that does not exist. See [BOM.md](BOM.md#why-the-imu-specifically).

### The rule, enforced by `min()`

```c
torque_limit = min(
    driver_command,
    local_limiter(own_state, peer_frame),    /* always valid, never skipped */
    supervisor_limiter(esp_frame)            /* only if fresh AND flags OK  */
);
```

Three properties, all structural rather than conventional:

1. **The supervisor can only subtract.** It is one term in a `min()`. There is no code path by
   which a value from the ESP32 raises a torque limit.
2. **Stale or invalid data is skipped entirely** — not clamped, not extrapolated. Check `seq`
   freshness against a window; miss it and the term drops out.
3. **The local limiter is never conditional on the supervisor.** It runs on data the controller
   owns and is the floor of the system's capability.

A wrong ESP32 therefore causes an **unnecessary torque cut** — a safe failure. It cannot cause an
unwanted torque *increase*, which is the only kind that hurts.

> ⚠ **This holds only because torque reduction is monotonically safe.** If traction control ever
> grows into torque *vectoring* — one axle given **more** to correct a yaw — the supervisor must
> not be an input to that decision. **Reduce total magnitude, never redistribute upward.** The same
> caveat applies to regen braking: reducing magnitude is always safe, changing sign is not.

### What this does to the pull-the-plug test

The test does not go away; it becomes graded, which makes it more informative.

| | Old rule | Amended rule |
|---|---|---|
| Yank the ESP32 at 20 mph | Nothing happens | **Truck keeps driving**, TC falls back to inter-axle slip + the wheel-acceleration threshold |
| What is lost | Telemetry, logging | Telemetry, logging, **and** the all-four-spinning detector |
| Passes if | Truck drives normally | Truck drives normally, with degraded TC |

You should feel the difference at launch and nowhere else. The property being protected is
unchanged: **the ESP32 is never the reason something is safe, and never the reason the truck can
go faster.**

### ⚠ Pitch corrupts `a_x`, inseparably

Gravity leaks into the longitudinal axis and no software can separate the two:

```
5° nose-up  → g·sin(5°)  = 0.86 m/s² error on a ~10.8 m/s² signal ≈ 8%
10° nose-up → g·sin(10°) = 1.70 m/s²                              ≈ 16%
```

For a slip *detector* this is absorbed by choosing the threshold with margin — you are looking for
wheels running away from the vehicle, which is a large signal, not an 8% one. **Do not attempt to
correct it in software at Phase 5.** At Phase 8 the ride-height actuators know their own position,
so pitch becomes measurable and can be corrected properly if it turns out to matter.

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

GPS at 10 Hz is far too slow to close a traction-control loop against, and this has not changed.
What changed is that it no longer has to be: the supervisor **fuses the 10 Hz GPS with the 200 Hz
IMU** and publishes a single ground-speed estimate at 200 Hz. Fast signal from the accelerometer,
slow drift correction from the GPS, one number to the controllers — see
[the supervisor exception](#the-supervisor-exception--advisory-input-that-can-only-subtract).

GPS remains exactly right for *tuning*: you drive, you log everything, and you calibrate the fast
estimators against the slow ground truth afterwards. Phase 5 establishes the correlation; Phase 6
tunes against it.

## Failure modes and what happens

| Failure | Detected by | Response |
|---|---|---|
| Overcurrent | Hardware comparator → TIM break | PWM gates off immediately, latched, no software |
| Phase overtemp | Thermistor on ADC | Torque fold-back, then cut |
| RC link lost | CRSF frame timeout in `rc_input.c` | Torque to zero, coast. **Not** brake — braking a truck at 70 mph with no steering input is worse than coasting. |
| Axle desync | `safety.c` on each controller, comparing own RPM against the peer frame | Local cut **plus** fault line asserted → **both** channels cut. Never one. |
| **Peer link silent** | Heartbeat timeout, *N* ms | Treated identically to the peer having died: both cut. Silence is a fault, not a warning. |
| Observer loses the rotor | Angle-tracking error threshold | That channel cuts and asserts its fault line, which cuts the other |
| ESP32 dies | `seq` staleness on the advisory frame | Driving continues; the supervisor term drops out of the `min()` and TC falls back to inter-axle slip + the wheel-acceleration threshold. Telemetry stops. |
| Supervisor sends garbage | Validity flags, plus `min()` structure | Worst case is an unnecessary torque cut. It is structurally incapable of raising a limit. |
| **One controller dies** | Peer heartbeat timeout on the survivor; cross-wired fault line if it faulted cleanly | Survivor cuts. **A hard power-loss death cannot assert the fault line, which is exactly why the heartbeat timeout exists** — it is the backstop for the backstop. |
| Both controllers die | Watchdog on each | Reset into a zero-torque state; receiver failsafe as backstop |

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
