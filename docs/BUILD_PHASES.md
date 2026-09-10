# Build Phases

**This document is the entry point for the whole project.** Each phase ends with a specific,
checkable result. Don't start the next one until the current one passes — every phase exists
because skipping it makes a later problem impossible to diagnose, and on this vehicle several of
them exist because skipping them destroys hardware or hurts someone.

| Phase | Goal | Done when |
|---|---|---|
| [**P0**](#phase-0--numbers-before-parts) | Lock the gearing math, then buy | BOM committed, numbers close with headroom |
| [P1](#phase-1--one-motor-current-sensing-proven) | One motor, phase current measured true | Currents read zero at rest and sum to zero spinning |
| [P2](#phase-2--closed-loop-foc-on-one-motor) | Torque then velocity control | Holds commanded RPM through the whole range |
| [**P2b**](#phase-2b--port-to-the-flight-power-stage) | Same firmware, flight hardware | Every P1 and P2 gate re-passed on the FSESC, at full current |
| [P3](#phase-3--both-channels) | Dual-motor FOC, synchronized | Both axles track one setpoint; desync trips the interlock |
| [P4](#phase-4--printed-rolling-chassis-20-mph-cap) | Printed mule, rolling, capped | Straight line at 20 mph, failsafe proven |
| [P5](#phase-5--supervisor-tier--gps) | ESP32, GPS, IMU fusion, logging | Yank the ESP32 at speed and the truck drives on, TC degraded |
| [P6](#phase-6--the-speed-ladder-on-printed-parts) | 30 → 40 → 50 mph, gated | Each step survived and logged; find what breaks |
| [P7](#phase-7--aluminum-chassis) | Cut-and-bend aluminum | Re-validates every earlier gate on metal |
| [P8](#phase-8--70-mph) | The number | GPS-verified 70+ mph, truck intact |

**Two things carry the whole plan.** First: the **speed cap lives in the motor-control MCU**, is
enforced in firmware below the RC input, and goes up exactly one step per phase gate. Second:
**the ESP32 is never the reason something is safe, and never the reason the truck can go faster.**
It may supply *advisory* data that can only ever **reduce** a torque limit — enforced by a `min()`,
not by discipline — and it can never raise one, command torque, or clear a fault. See
[the supervisor exception](ARCHITECTURE.md#the-supervisor-exception--advisory-input-that-can-only-subtract).

> **The hardware moves once, deliberately, at P2b.** Phases 1–2 run on a Nucleo-G474RE +
> X-NUCLEO-IHM08M1 at ≤ 10 A, because that is a cheap place to make expensive mistakes and ST
> ships a reference manual for it. Phases 2b onward run your same firmware on Flipsky Mini
> FSESC6.7 Pro boards — STM32F405 attached to a 70 A inverter — one per motor. The full
> reasoning is in [BOM.md](BOM.md#the-three-stage-hardware-plan).
>
> **`foc.c`, `observer.c`, `speed_loop.c` and `safety.c` port unchanged.** Only `pin_map.h`,
> `inverter.c` and `current_sense.c` are hardware. Keep that boundary clean from Phase 1 and
> P2b is a week rather than a month.

---

## Phase 0 — Numbers before parts

### ⭐ START HERE

You own a body and nothing else. That is an advantage — nothing is already decided, so decide it
with arithmetic instead of with a shopping cart.

**Steps:**

1. Run both solvers. `gearing.py` answers "how fast eventually"; `accel.py`
   answers "how quickly, and what actually limits it" — you need both, and
   they disagree by ~7% on top speed for reasons worth understanding (see
   below).
   ```bash
   cd host-tools && uv run gearing.py --sweep
   cd host-tools && uv run accel.py --sweep
   ```
2. Pick a tire diameter and final drive that put required kV inside the 1200–3500 band with the
   pack you want. The current working point:

   > ✅ **Re-run against the motor actually bought.** Every number below now comes from the
   > solvers with the **QuicRun 3650 G2 13.5T — 2850 kV, 28.9 mΩ, 2-pole (`pole_pairs = 1`)**
   > as their default, matching [BOM.md](BOM.md#decided--quicrun-3650-g2-135t-on-4s) and
   > `motor-ctrl/inc/pin_map.h`. Two constraints the solvers still do not enforce, so check
   > them by hand on any ratio change:
   >
   > - **`kV × V_max ≤ ~48,000 rpm`** — the 3650 can's mechanical ceiling. At 4S that is satisfied
   >   exactly (47,880), so there is no headroom above 4S for this winding.
   > - **ERPM = rpm × pole pairs** against the controller's loop rate. At 1 pole pair this is
   >   comfortable; it would not be at 2.
   >
   > Nothing in this section is settled — including the 2.5:1 ratio, the tire diameter, and the
   > cell count. Treat the tables below as a worked method, not as decisions.

   | | |
   |---|---|
   | Final drive | 2.5:1 |
   | Pack | 4S, 14.4 V under load |
   | Motor | QuicRun 3650 G2 13.5T, **2,850 kV** |
   | kV *required* for the 70 mph cap on 70 mm | 1,744 — so the motor is **63% over-specified** |
   | Motor RPM at the 70 mph cap | 21,345 (45% of no-load, 61% of the 0.85-derated ceiling) |
   | **Wheel RPM at the cap — every configuration** | **8,537** |
   | Wheel RPM the drivetrain could reach *uncapped* | 13,954 (**+62% over tire rating**) |
   | Steady-state draw | 200 W, 6.9 A per motor |

   **Two tire configurations, one drivetrain.** Top speed changes by changing
   wheels, and nothing else changes at all:

   | Profile | Tire | Rated RPM | Ceiling |
   |---|---|---|---|
   | `street` | 55 × 22, cheap | 8,600 | ~50 mph |
   | `test` | 70 × 25, cheap | 8,600 | ~63 mph |
   | `wide` | 70 × 35, cheap | 8,600 | ~63 mph |
   | `record` | 70 × 25, high-rated | 12,000 | ~89 mph |

   **Tire rating is a purchasing decision, and the ceiling is computed from it**
   (rating × 0.90 derate) rather than declared. A better tire raises the cap
   automatically; a cheap one lowers it. So you burn cheap tires through the
   Phase 6 ladder at a cap they can survive, and fit rated tires for the
   attempt — and the truck enforces that itself.

   The reason this works, and it is not obvious: **wheel RPM is constant across
   every tire size.** The motor's RPM ceiling divided by the final drive sets
   it; tire diameter only changes how far the truck travels per revolution.

   That is what retires the tire-RPM problem *at the cap*: chasing 70 mph on a
   55 mm tire needed 10,866 wheel rpm, 36% over typical 1/12 pan-car duty,
   whereas a 70 mm tire at the cap needs 8,537 in every configuration.

   **⚠ But gearing no longer backstops it, and the real motor made this worse.**
   At 2.5:1 the drivetrain can spin the wheels to 13,954 rpm — **62%** past the
   tire rating — if the cap is not holding. (It was +39% against the 2,441 kV
   placeholder; the 2,850 kV motor bought that much more overspeed.) Even at
   3.5:1 the margin is +16%, so no ratio in the usable range restores a
   mechanical backstop. Gearing taller for motor wear
   spent the mechanical backstop, which means **the firmware speed cap is now
   load-bearing safety rather than a convenience.** That is why the cap
   ceilings are tire-derived (`safety.c TIRE_PROFILES`), why the profile is
   bound to the ceiling, and why selecting the right profile is a pre-run
   checklist item. Run `gearing.py --swap` to see the current margin.

   **The three-way tradeoff on the ratio**, now that all three are quantified:

   | Ratio | Motor wear | Grip usable to | Tire-overspeed margin |
   |---|---|---|---|
   | 1.87:1 | best | μ 1.33 | +117% |
   | **2.5:1** | 1.40× bearing life | μ 1.78 | +62% |
   | 3.5:1 | baseline | μ 2.50 | +16% |

   The "grip usable to" column moved up a lot from an earlier draft (1.10 /
   1.47 / 2.0+) for one reason: it was computed at 30 A per motor, and the
   QuicRun takes **50 A peak**. More current means more torque before the
   inverter runs out, so each ratio now uses more grip than it used to.

   Taller protects the motor and wastes grip and mechanical tire protection;
   shorter does the reverse. 2.5:1 is the current pick and it is provisional
   until Phase 4 measures μ.

   ⚠ **6S at 4.0:1 was listed here as the upgrade path if Phase 6 showed heat as
   the limiter. It no longer works with the chosen motor**: 2850 kV × 25.2 V is
   71,820 rpm against the 3650 can's ~48,000 rpm ceiling. Going above 4S means a
   higher-turn winding — 21.5T at 1800 kV gives `1800 × 25.2 = 45,360` ✓ — and
   that trade is neutral on motor heating and *reduces* current, so it is a real
   option. It is a **motor swap**, not a pack swap. See
   [BOM.md](BOM.md#the-one-number-that-actually-ranks-motors).

   **Gearing costs nothing off the line, so spend it on motor longevity.**
   `accel.py` puts 0-50 mph at ~2.4 s across every ratio from 2.5:1 to 5:1,
   because the entire run is traction-limited — the motors make ~1.3× more
   force than the tires will transmit. And the cap ceilings are set by the
   *tire's* rpm rating, not by gearing, so they don't move either. Gearing is
   therefore free, and the right thing to buy with it is rotational wear.

   At a sustained 70 mph, relative to 3.5:1 (`accel.py --thermal 70`):

   | Ratio | Motor rpm | Loss | Bearing life | Centrifugal stress |
   |---|---|---|---|---|
   | **2.5:1** | **21,345** | **−23%** | **1.40×** | **0.51×** |
   | 3.5:1 | 29,882 | — | 1.00× | 1.00× |
   | 5.0:1 | 42,689 | +60% | 0.70× | 2.04× |

   The loss column shrank from −30%/+66% against the placeholder motor: the
   QuicRun's 28.9 mΩ winding and single pole pair both cut the speed-dependent
   term. Bearing life and centrifugal stress are pure geometry and did not
   move.

   Bearing L10 life goes as 1/rpm and magnet-retention stress as rpm², so both
   punish a short gear far harder than heat does. 29,882 rpm also sits near the
   practical ceiling for grease-lubricated small bearings; 21,345 is comfortably
   inside it.

   The resulting over-specification is deliberate: the 2,850 kV motor at 2.5:1
   models out to ~111 mph and is capped at 70 by the tire. **Running a motor at
   45% of its no-load speed IS the wear reduction** — and that surplus is a
   deliberate consequence of picking the winding on motor constant and ERPM
   rather than on required kV. Required kV here is only 1,744.

   If you want a quicker launch, the lever is grip (μ 0.7 → 1.3 moves 0-50 from
   3.98 s to 1.98 s) — not the ratio, not more inverter, and not less mass.

   **Grip is bought with tire WIDTH and COMPOUND, not diameter.** Rubber and
   foam do not obey `F = μN`; their friction has adhesion and hysteresis terms
   that make μ *load-sensitive*, falling as contact pressure rises (roughly
   μ ∝ N^−0.1…−0.2). A wider tire spreads the same load over more area, runs
   lower pressure, and genuinely grips harder. Diameter is a different lever
   entirely — it sets gearing, wheel rpm, rotational inertia, and CG height,
   but not μ. `accel.py --tire` only sweeps diameter, which is why acceleration
   looks flat there; grip enters through `--mu`.

   **⚠ These two choices interact, and the ratio is NOT free once grip
   improves.** 2.5:1 leaves ~1.6× torque headroom over a μ = 1.1 traction limit.
   Push grip past **μ ≈ 1.78** and the launch becomes current-limited — wider or
   stickier tires then buy nothing:

   | μ | 0-50 @ 2.5:1 | 0-50 @ 3.5:1 |
   |---|---|---|
   | 1.1 | 2.38 s | 2.38 s |
   | 1.4 | 1.83 s | 1.83 s |
   | 1.5 | 1.70 s | 1.70 s |
   | 1.8 | **1.41 s — current-limited** | 1.40 s |
   | 2.0 | **1.41 s — grip wasted** | 1.25 s |

   The crossover moved from μ ≈ 1.47 to μ ≈ 1.78 purely because the real motor
   takes 50 A where the placeholder took 30 A. That is genuinely good news: it
   means a sticky tire is still worth buying at 2.5:1, which it was not before.

   > **⚠ SUPERSEDED — the gears are no longer cut, and the μ ordering below no
   > longer gates anything.** The ratio now arrives with the differential: the
   > motor drives an off-the-shelf ring-and-pinion directly and that single mesh
   > is the whole transmission, at **2.33–3.20:1** depending on the ring and
   > input pinion actually fitted. See
   > [MECHANICAL.md](MECHANICAL.md#the-chosen-drive-unit--mjx-hyper-go-114-metal-diff).
   >
   > The `--match` reasoning below is still correct arithmetic and still worth
   > reading for *why* the ratio choice is not free. What changed is that it was
   > being used to justify a purchase gate, and it cannot: `accel.py --sweep`
   > gives **2.27 s to 50 mph at every ratio from 1.6:1 to 5.0:1**, because the
   > launch is grip-limited throughout. Every candidate ratio in the band is
   > shorter than matched for any realistic μ, so μ does not select between them.
   > μ remains the *only* acceleration lever — it just is not a gearing input.

   So the ordering matters: **choose the tire, measure μ, then cut the gears.**
   `accel.py --match` solves for the ratio where launch torque exactly meets
   launch grip — all the grip used, motor spinning as slowly as possible.
   Shorter than matched wastes motor life; taller wastes tire.

   | Measured μ | Matched FD | Motor rpm at cap |
   |---|---|---|
   | 1.0 | 1.40:1 | 11,961 |
   | 1.1 | 1.54:1 | 13,157 |
   | 1.3 | 1.82:1 | 15,549 |
   | 1.5 | 2.10:1 | 17,941 |

   **⚠ These ratios all came down sharply** — μ = 1.1 matched at 1.87:1 against
   the placeholder motor and matches at **1.54:1** against the real one, for the
   same 30 A → 50 A reason. Matching the measured μ literally would mean gearing
   much taller than 2.5:1, which costs tire-overspeed margin you cannot afford
   (see the +62% above). **Do not treat `--match` as the whole answer** — it
   optimises motor wear alone and knows nothing about the overspeed margin.

   2.5:1 is the matched ratio for μ ≈ 1.78. It is a *provisional* pick standing
   on a guessed μ — treat it as the working assumption to be replaced by the
   Phase 4 measurement, not as a decided number.

   **A CVT or 2-speed is not worth it here, and the reason is specific.** Both
   exist to give you a short ratio off the line and a tall one at the top. You
   are traction-limited off the line, so the short ratio buys nothing — you want
   the tall one always, which is a fixed gear. A variator would add ~10% drive
   loss plus its own sliding, belted, high-speed wear items: more rotational
   wear than it removes.

   Two consequences worth acting on:

   - **~31 A per motor is where launch force meets the traction ceiling** at
     μ = 1.1. 15 A is genuinely slow; 50 A is identical to 31 A. The QuicRun's
     50 A peak is right *as headroom*, not because the extra is usable — and
     note this threshold scales with μ, so better tires do make more of the
     current usable (see the μ ≈ 1.78 crossover above).
   - **Building light is for safety, cost, and part stress — not performance.**
     Mass cancels in traction-limited acceleration, and a 1300 g truck actually
     posts a marginally quicker 0-50 than a 700 g one.

   **The two models disagree, and you should know which to trust when.**
   `gearing.py` applies a generic 0.85 no-load-RPM factor and predicts 70 mph;
   `accel.py` models back-EMF against real impedance and predicts 75. The truth
   is in between — `accel.py` omits some losses, `gearing.py`'s 0.85 is a rule
   of thumb borrowed from vehicles under far more load than this one. Treat the
   pair as a bracket until Phase 2 bench data replaces both.

3. **Size the inverters for peak, not steady state.** 6 A holds 70 mph; acceleration pulls several
   times that. Budget ~30 A continuous per channel with headroom above it.
4. Commit the BOM — [BOM.md](BOM.md) is phase-gated so you're not buying Phase 7 parts yet.

**Pass criteria:**

- [ ] Required kV lands in the 1200–3500 band, not at an edge.
- [ ] Chosen motor's manufacturer max RPM exceeds your Phase 8 motor RPM **with margin**. This is
      the single most commonly ignored spec in speed builds.
- [ ] **Record tires good for ≥ 9,500 rpm.** At 2.5:1 the wheels turn 8,537 rpm at 70 mph, and a
      10% derate on an 8,600 rpm tire only licenses ~63 mph. **Vendors rarely publish an RPM
      number** — derive it from the speed the tire is sold as good for:
      `rated_rpm = proven_mph × 0.44704 ÷ (π × od_m) × 60`. Speed-run foams are marketed by
      intended speed for exactly this reason. Cheap tires for Phases 4–6 are fine and expected;
      the cap follows whatever you enter.
- [ ] Battery, motor, and inverter current ratings agree with each other at peak, not average.
- [ ] **Ring and input-pinion tooth counts confirmed on the physical part**, not from photography.
      The ratio arrives with the diff rather than being cut, so there is nothing to defer — but
      until the teeth are counted, firmware must use the LOW end of the range (2.80:1 for a 10T
      input pinion, 2.33:1 for a 12T). Speed is derived as `motor_rpm / final_drive`, so
      over-assuming the ratio makes the cap fail **open**: assume 3.00 when the truth is 2.80 and
      the cap holds at 75.0 mph on a tire rated for 70.5.

**Why this is a phase and not a footnote:** buying a motor before doing this arithmetic is how
speed builds end up geared for 45 mph with no way to fix it short of new everything.

---

## Phase 1 — One motor, current sensing proven

The entire point of this phase is to prove you can **measure phase current correctly**. Nothing in
FOC works if the current numbers are wrong, and every later phase compounds the error. This is the
exact analogue of the crawler's "prove you can read an encoder" — same reasoning, harder sensor.

One motor. **No wheel, no load, motor clamped.** A spinning wheel on a bench supply is how bench
work becomes an injury.

**Hardware:** Nucleo-G474RE + X-NUCLEO-IHM08M1 (three-shunt jumpers set to 3-Sh), one motor, a
current-limited bench supply. Current limit set low — start at 2 A. See [WIRING.md](WIRING.md)
and [BOM.md](BOM.md).

> ⚠ **`IQ_LIMIT_A` = 10 A for this whole stage, and the reason is not thermal.** The IHM08M1's
> current-sense amplifier saturates near **12 A** — 10 mΩ shunt × ~13.7 gain against ±1.65 V of
> swing. Past that the reading silently stops being true, which is precisely the failure this
> phase exists to catch. Cooling does not help; you cannot cool a clipped op-amp.
>
> **Verify the gain for your jumper configuration before computing `amps_per_count`.** UM1996
> lists JP2 as "op-amp gain modification" and the FOC configuration wants JP1/JP2 *closed*, so
> 13.7× may not be your number. Measure it.

> ⚠ **`POLE_PAIRS = 1` for the QuicRun 3650.** It is a **2-pole** motor. `pin_map.h` currently
> has 2, which was right for the 4-pole cans previously under consideration. Get this wrong and
> every RPM and angle the observer produces is out by 2×. Count the magnets before you start.

**Steps:**

1. Bring up PWM on TIM1 with **deadtime configured and verified on a scope before the gate driver
   is connected.** Shoot-through from missing deadtime destroys a half-bridge in microseconds.
2. Configure ADC injected conversion triggered by the TIM1 update event, so phase current is
   sampled at the same instant every PWM cycle. See `current_sense.h`.
3. Run the offset calibration routine with the motor at rest.
4. Force open-loop rotation (`bringup.c`) at low duty and watch the phase currents.

**Pass criteria — all four:**

- [ ] **Currents read zero at rest**, after calibration, with no drift over a minute. Nonzero here
      is offset error and it will read as phantom torque later.
- [ ] **Ia + Ib + Ic ≈ 0** at every instant while spinning. This is Kirchhoff, not an
      approximation — if the three don't sum to zero, at least one is wrong.
- [ ] Currents are **sinusoidal and smooth**, not spiky or clipped. Spikes mean you're sampling
      during switching transitions; move the sample point.
- [ ] Scope-verified deadtime on every half-bridge, matching what the gate driver datasheet wants.

**Do not proceed until all four pass.** A current-sense scale error you tolerate here becomes a
torque loop that quietly lies to you at Phase 6, and you will not find it by reading the FOC code.

---

## Phase 2 — Closed-loop FOC on one motor

Same hardware. Close the current loop, then the velocity loop, then make it work sensorless across
the full RPM range.

**Steps:**

1. Close the d/q current loops. Tune `id` to zero-hold first — it's the easy one and it validates
   the Park transform independently of anything mechanical.
2. Bring up the rotor observer (`observer.c`, currently stubbed with the algorithm in the header).
   **Sensorless FOC has no rotor angle at zero speed** — you need a forced-commutation startup ramp
   that hands over to the observer above some threshold RPM. That handover is the fiddliest part of
   this whole project.
3. Close the velocity loop on top. Tune with the plot in one terminal and the tuner in another:
   ```bash
   cd host-tools && uv run telemetry_plot.py
   cd host-tools && uv run tune.py
   ```

**Pass criteria:**

- [ ] Holds a commanded RPM within ~3% unloaded, from the observer handover threshold up to
      **the full motor RPM your chosen gearing implies at the Phase 8 cap.** Test the top of the
      range now, on the bench, with no wheel. Finding an observer that falls apart near the top
      is much cheaper here. For reference the 13.5T's no-load ceiling on 4S is 47,880 rpm; you
      will be geared well below that, but know both numbers.
- [ ] **Sanity-check ERPM against your loop rate.** `ERPM = rpm × pole pairs`, and below roughly
      20 control updates per electrical revolution the angle estimate lags and the motor
      desyncs. At 2 pole pairs you would have half the headroom — the 2-pole QuicRun is why this
      is comfortable rather than marginal. If the observer degrades at the top of the range,
      raise the switching frequency before you touch the gains.
- [ ] Startup ramp hands over to the observer cleanly, **20 attempts in a row**, no stalls and no
      audible cogging at handover.
- [ ] Recovers from a step change without overshooting into current limit.
- [ ] Pinching the shaft (carefully, gloved, low speed) makes q-axis current rise to compensate.
      An unloaded motor holds a setpoint with almost any gains — load is the real test.

---

## Phase 2b — Port to the flight power stage

**Still one motor.** Same algorithms, new hardware under them. This phase exists so that the
port is debugged against a known-good single-motor baseline, rather than tangled up with
dual-channel bring-up or a rolling chassis.

**Hardware:** one Flipsky Mini FSESC6.7 Pro, standalone SWD debugger, the same motor and bench
supply. **Keep the Nucleo rig assembled and working** — it is your reference when the new board
misbehaves, and that comparison is the whole reason Stage 1 happened.

**Before you power anything:**

1. **Get the schematic for your exact board revision.** You need the pin mapping, the gate driver
   part number (DRV8301 and DRV8323 variants both exist, with different register interfaces), and
   the current-sense scaling. `amps_per_count` comes straight out of it. If no schematic is
   obtainable, stop and buy hardware where one is — do not reverse-engineer a 70 A inverter.
2. **Confirm SWD access.** Without it you cannot debug, only flash blind.
3. **Confirm the 14 V floor** against your pack choice. See [BOM.md](BOM.md#-verify-before-ordering--any-one-of-these-can-sink-the-plan).

**Steps:**

1. Rewrite `pin_map.h` for the F405. Timer, ADC and sensor pins all move.
2. Rewrite `inverter.c` — same TIM1 complementary-PWM structure, different deadtime register
   values and a gate driver that needs SPI initialisation the IHM08M1 did not.
3. Rewrite `current_sense.c` for the new shunt value and amplifier gain.
4. **Nothing else.** If you find yourself editing `foc.c` or `observer.c`, stop and work out why —
   the algorithms are hardware-independent and a change there means something is wrong in the HAL.

**Pass criteria:**

- [ ] **Every Phase 1 criterion re-passed**, current-limited, at low current. Zero at rest, sum to
      zero spinning, sinusoidal, scope-verified deadtime. A new power stage is a new sensor chain.
- [ ] **Every Phase 2 criterion re-passed**, including 20 clean observer handovers.
- [ ] `amps_per_count` **validated against a clamp meter or bench-supply reading**, not just
      derived from the schematic. This is the number the old board could not give you honestly
      above 12 A, and the entire point of the port is that it now can.
- [ ] Current ramped past the old 12 A ceiling — **to 30 A at minimum** — with the readings still
      linear and the residual still near zero. Do this on the bench supply, motor clamped.
- [ ] Thermal check: run the motor at ~20 A for five minutes and record controller and motor
      temperature. This is the course-running duty cycle and the first real data on it.

> ⚠ **Your bugs now cost $125 a board plus a motor.** A shoot-through at 70 A on 4S is
> spectacular rather than educational. Keep the bench supply current-limited through this entire
> phase; do not connect a LiPo until every criterion above passes.

---

## Phase 3 — Both channels

Second motor, second FSESC. **Two independent controllers, one per motor**, coordinated by the
ESP32 supervisor.

Split out as its own phase because inter-axle coordination is where the desync interlock either
works or doesn't, and debugging it alongside a rolling chassis is miserable.

> **This phase got substantially easier.** The original plan ran both motors from one G474 on
> TIM1 and TIM8, which required a second IHM08M1 hand-remapped with cut traces and flying leads —
> both shields hardwire to the same morpho pins. Two separate boards remove that problem
> completely, and add independent failure domains, a full timer and ADC set per motor, and two
> genuinely independent telemetry streams for the interlock.

**Steps:**

1. Bring up channel B standing alone; re-run every Phase 1, 2 and 2b criterion on it. Yes, all of
   them. This is fast and it's where you catch a swapped phase.
2. Establish the two UART links to the ESP32 and confirm both telemetry streams are live,
   timestamped, and distinguishable. See [PROTOCOL.md](PROTOCOL.md).
3. Implement the torque split and the **sync interlock** (`safety.c`).

**Pass criteria:**

- [ ] Both channels pass Phase 1 + 2 + 2b criteria independently.
- [ ] Both hold the same commanded RPM within ~2% of each other, unloaded.
- [ ] Each control loop meets its deadline with margin — measured on both boards, not assumed.
- [ ] **The sync interlock actually fires.** Test it deliberately: stall one motor by hand at low
      speed and confirm *both* channels cut, not just the stalled one. An axle losing torque at
      70 mph while the other keeps pushing is a spin.
- [ ] **The interlock survives a dead link.** Unplug one controller's UART mid-run and confirm
      both axles cut. With two independent MCUs this is a new failure mode that one-G4 did not
      have, and it is the one that matters most.
- [ ] Front/rear torque split responds to the commanded bias.

---

## Phase 4 — Printed rolling chassis, 20 mph cap

Make it a vehicle. **ABS or ASA, not PLA** — PLA creeps near motors and lets go under sustained
load. [MECHANICAL.md](MECHANICAL.md) has the geometry and the design rules.

Print every structural part as the **flat-pattern-folded** geometry it will eventually be in
aluminum. That constraint costs you almost nothing now and means Phase 7 is a material change
rather than a redesign.

**Mechanical:** ~190 mm wheelbase, ~135 mm track, body sitting over it. **Independent uprights at
all four corners**, half-shaft from each drive unit to its upright — settled, because Phase 8's
per-corner ride-height control requires it. Steering servo with real Ackermann. Battery as low and
as far forward as it will go. See [MECHANICAL.md](MECHANICAL.md#independent-uprights--settled-not-open).

### 4a — The packaging study

**Do this in 2D before any 3D modelling.** Side view and top view, real dimensions, an hour of
work. It answers the question that could invalidate the whole layout: *do two drive units, a 4S
pack, two FSESC controllers (67 × 39 × 18.7 mm each) and the ESP32 actually fit inside 105 mm of
width at a 190 mm wheelbase?* Find that out in 2D in an hour, not in 3D after a week.

(105 mm is the right number for this question even though the body is 124.3 mm at the fender
flares — the flares sit over the wheels, and the bay lives under the necked-in cab/bed section.)

> **Done — the answer is yes, with two purchasing consequences.** The layout,
> the motor-offset decision, and what is left in the middle are written up in
> [MECHANICAL.md § Drive unit packaging](MECHANICAL.md#drive-unit-packaging--phase-4a).
> Headlines: motors go **inboard and horizontal**, leaving a **97 mm bay**; a
> standard 4S hardcase pack **does not fit** (buy an 850–1000 mAh softpack);
> electronics deck **above** the pack; and the gear pair runs at 17.7 m/s
> pitch-line, so the **pinion must be steel**.

Three decisions come out of it, and all three are inputs to housing geometry — which is a
cut-and-bend part you want to design once.

#### Decision 1 — spool or differential in each drive unit — **DECIDED: diffs both ends**

Each axle can lock its two wheels together (spool) or let them turn independently (diff). It is a
**housing-geometry input**, so it had to be settled before the drive units were drawn. The
comparison is kept below as the record of why.

| | Spool | Differential |
|---|---|---|
| Weight / parts | Lighter, fewer parts, effectively unbreakable | ~20–40 g per axle, more to package, more to fail |
| Straight line | Slight yaw-stability benefit | Neutral |
| Turning | Scrubs; pushes on turn-in | Correct behaviour |
| Under one-wheel slip | Torque still reaches the gripping wheel | Torque goes to the *slipping* wheel |
| Wheel-speed signal | Unambiguous — motor rpm maps to both wheels | Corrupted by a lifted or slipping wheel |

**The last row is the one specific to this truck, and it is easy to miss.** Speed is derived from
motor RPM, and the desync interlock compares front motor RPM against rear. With open diffs, a
single slipping or lifted wheel spikes that axle's motor RPM — which inflates computed speed and
can **false-trip the desync interlock, cutting both channels at speed.** A nuisance trip at 60 mph
is not a nuisance. Spools keep that signal clean.

Front and rear did not have to match, but both landed the same way:

- **Front: diff.** A spooled front axle fights the steering directly — the classic cause of
  understeer — and on a 190 mm wheelbase that is not a small effect.
- **Rear: diff.** A spool would have been simplest and strongest for straight-line duty, but the
  truck is meant to corner, and a spooled rear scrubs and pushes in every turn while chewing the
  tires this project is trying to preserve.

**The deciding question was how much non-straight-line driving the truck is for.** A pure speed
machine only has to hold a line and turn around at walking pace, and spools would have given the
control code a cleaner wheel-speed signal for free. This truck also has to be a 55 mph
`street`-tire toy, so cornering won.

The cost is paid in firmware and in diff oil, both covered above.

**Status: DECIDED — differentials both ends.** The truck is meant to corner, not
only run in a straight line, and a spool at either end scrubs and pushes in
every turn. Both drive units are now identical, so there is one housing design
instead of two.

Use **gear diffs with tunable oil** rather than pure open diffs — heavy silicone
oil gives limited-slip behaviour, resisting the fast speed differences of a
spinning wheel while still allowing the slow ones of cornering. Fixed mechanical
choice becomes a Phase 6 tuning knob.

Both consequences are already absorbed: the desync detector tolerates a wheel
lift (see `pin_map.h`), and telemetry readers know motor RPM tracks the fastest
wheel on its axle (see [MECHANICAL.md](MECHANICAL.md#spool-vs-differential--decided-differentials-both-ends)).

#### Decision 2 — wheel offset for the `wide` tires — **DECIDED: no offset**

A wider tire at fixed track either pushes the wheels further out (more frontal area, more wheel
wake) or further in (fouling the chassis). **Neither: both tires share one centreline at ±67.5 mm,
and the `wide` tire simply grows 5 mm each way** — inner face 55 → 50 mm from centreline.

That keeps one hub spec, one upright, and makes the `wide` swap a tire change rather than a corner
rebuild. It is affordable because the binding constraint turned out to be the **chassis plate
outline**, which is free to change, rather than the drive unit, which is not. See
[MECHANICAL.md](MECHANICAL.md#steering-sweep--what-the-front-tires-can-hit).

The plate consequence, which is the part to carry into the drawing:

- **Front axle station waisted to 70 mm wide** (±35 mm) → 30° of lock on the `wide` tire, a 329 mm
  turning radius. Far more than this truck needs; even 20° gives 522 mm.
- **Middle bay stays ~100 mm** — nothing sweeps between the axles.
- **Rear axle station needs no waist at all.** No steering means no sweep, so it only has to clear
  the static 50 mm inner face and can stay ~90 mm wide.

Front waisted, rear not. If plate stiffness argues against narrowing the whole station, scallop
just the two corners at x = ±22 mm instead — that is where the pinch actually is.

#### Decision 3 — weight distribution — **TARGET: 60% front**

Not a trim option. Net aero yaw moment is destabilising and the tires have to
supply the stability through forward weight bias. **At 50/50 the critical speed
is ~50 mph — inside this ladder.** 60% front clears 70 mph even on the
pessimistic aero assumption.

Weigh the build-up and compute the split on the 2D drawing, then add nose
ballast to hit it (~150 g at the bumper for 60%). It costs ~1 W of top speed.

**Note the balance does not stay put.** Aero load at 70 mph is 79% of vehicle
weight, so a static 60/40 becomes ~43/57 at the cap — and since the truck
reaches 50 mph in ~2.4 s, the whole swing happens *inside the launch*. That
makes the torque split a launch-controller output, not a static tune. See
[MECHANICAL.md](MECHANICAL.md#-directional-stability--the-layout-as-drawn-is-divergent-at-50-mph).

#### Check 4 — CG height budget

Do this once, on the 2D side view, by weighing parts and reading heights off the drawing.

A vehicle rolls rather than slides when lateral acceleration exceeds `(track/2) / h`. With a
135 mm track that is **61 mm at μ 1.10, and 56 mm at μ 1.20** with the `wide` tires. Above it the
truck stops sliding when it runs out of grip and starts tipping — at speed, a tumble instead of a
spin.

A first-pass build-up with 70 mm tires (axle centreline at 35 mm):

| Component | g | h |
|---|---|---|
| Chassis plate + drive units | 250 | 25 mm |
| 2 × motor, on the axle line | 160 | 35 mm |
| 4S pack, flat on the floor | 150 | 31 mm |
| Wheels + tires | 100 | 35 mm |
| Body shell (tall, light) | 80 | 50 mm |
| Electronics, flat | 100 | 20 mm |
| **Total** | **840** | **31 mm** |

> **The motor line is optimistic if you buy the recommended motor.** 160 g for
> both assumes a 28xx can. The chosen **QuicRun 3650 G2 13.5T is 186 g each** —
> 372 g for the pair, taking the total to **~1,050 g**. CG barely moves —
> 31 → ~31.9 mm, because they already sit at axle height — so nothing below
> changes. But `gearing.py` and `accel.py` default to `mass_kg = 0.90` and want
> re-running at ~1.05. See
> [MECHANICAL.md § Drive unit packaging](MECHANICAL.md#drive-unit-packaging--phase-4a).
>
> ⚠ **Mass feeds back into the motor sizing.** Traction force is `μmg`, so required current
> scales linearly with mass and the usable-μ ceiling scales inversely. Re-run the motor numbers
> in [BOM.md](BOM.md#decided--quicrun-3650-g2-135t-on-4s) against the final mass, not against 0.90.

**So the ceiling is not close, and this is a check rather than a design driver.** ~32 mm against a
61 mm limit is 29 mm of headroom, and the truck rolls at 2.1 g against 1.1 g of grip. Swapping the
flat electronics for a Nucleo-with-shield stack — 40 mm tall, ~150 g, centre at 35 mm — moves CG by
under 2 mm. That is not the "most of the budget" an earlier draft of this claimed.

The reason it barely matters: most of the truck's mass is at or below axle height already, and
almost nothing can sit high in a chassis this shallow. Ballast and the body shell are the only
things naturally up there.

**Ride height buys CG 1:1**, which is a Phase 8 bonus worth noting here:

| Ride height | CG | Rolls at |
|---|---|---|
| nominal | 32.9 mm | 1.90 g |
| −5 mm | 27.9 mm | 2.24 g |
| −10 mm | 22.9 mm | 2.73 g |

- [ ] Build-up done from real weights once parts arrive, and the result under 56 mm.
- [ ] Re-check after any ballast is added — ballast goes forward **and low** for this reason.

#### Decision 3 — where the μ measurement happens

Grip drives acceleration *and* the final drive ratio (`accel.py --match`), and it is still a guess.
Plan the measurement now: a straight, a known start line, and GPS logging — see 4c.

### 4b — Build it

1. Print and assemble the chassis, drive units, uprights, and steering.
2. Wire the CRSF receiver **directly to the motor controllers** — not through the ESP32. See
   [ARCHITECTURE.md](ARCHITECTURE.md#why-the-receiver-goes-to-the-controllers). With two controllers this
   means deciding which one owns the receiver and how the other gets its setpoint; whichever way
   you go, the ESP32 must not be in that path.
3. Fit **static** ride-height adjusters — shock collars or adjustable links. Powered actuators are
   Phase 8; see [MECHANICAL.md](MECHANICAL.md#ride-height-and-the-tire-swap).
4. Set `tire.profile` to match what is actually fitted, and the speed cap to **20 mph**. Confirm
   you cannot exceed it with the stick pinned.

### 4c — Drive it, and measure μ

5. Drive it in a straight line somewhere empty.
6. **Measure μ.** Full throttle from a standstill on each tire set, read `a = dv/dt` off the GPS
   trace, μ = a/g. Put the numbers into `TIRE_PROFILES[].mu_nominal`, then re-run
   `accel.py --match` and **cut the gears to the result.** Until this happens, the 2.5:1 ratio is a
   guess resting on a guessed μ.

**Pass criteria:**

- [ ] **RC failsafe works: turn the transmitter off and confirm the truck stops.** Test this
      before the truck is anywhere it can hurt something. Every session, not just once.
- [ ] The speed cap holds with the stick pinned. Verify against wheel RPM.
- [ ] Drives straight at constant throttle. Curving means an axle is scaled wrong or the geometry
      is off — fix it now, at 20 mph, not at 50.
- [ ] Nothing on the truck is too hot to hold after a five-minute run. Motors, inverters, battery.
- [ ] Wheels stay on. Check retention after every run in this phase.
- [ ] **μ measured on every tire set you own**, written into `TIRE_PROFILES`, and the final drive
      re-checked against `accel.py --match`. This is the measurement the whole gearing choice has
      been waiting on — do not carry a guessed μ into Phase 6.

---

## Phase 5 — Supervisor tier + GPS

Add the ESP32, the GPS, and the logging. None of it touches the motors.

**Steps:**

1. Mount the ESP32, the 10 Hz GPS, and the IMU. Wire **both controllers** ↔ ESP32
   (**common ground required**) — one CAN bus preferred over two UARTs.
2. Bring up logging and the WiFi telemetry stream.
3. Correlate GPS ground speed against wheel-RPM-derived speed on a Phase 4 run.
4. **Bring up the fused ground-speed estimate** — 200 Hz IMU integration, corrected by the 10 Hz
   GPS — and publish the advisory frame. Log it alongside raw GPS and wheel speed so you can see
   the fusion working before anything consumes it.
5. **Wire the advisory frame into the controllers' `min()` last**, after you have logged it for a
   full session and trust it. It can only subtract, but an over-eager estimator that subtracts
   constantly is still a truck that will not accelerate.

**Pass criteria:**

- [ ] Logs a full run without dropping the link.
- [ ] GPS ground speed and wheel-derived speed agree within a few percent on a clean run. Where
      they *disagree* is wheelspin, and that's the signal you'll tune traction control against.
- [ ] **Killing the ESP32 mid-run does not affect driving.** Pull its power at 20 mph and keep
      driving. This is the whole justification for the two-tier split — test it explicitly.
      **The test is now graded, not binary:** the truck must keep driving normally, with traction
      control degraded to inter-axle slip plus the wheel-acceleration threshold. You should feel
      the difference at launch and nowhere else. Anything more than that means supervisor data has
      leaked out of the `min()` and into the baseline.
- [ ] **The advisory frame degrades cleanly when stale**, not just when absent. Delay or corrupt it
      deliberately and confirm the `seq` freshness check drops the term rather than acting on old
      data.

---

## Phase 6 — The speed ladder, on printed parts

Raise the cap one step at a time: **30, then 40, then 50 mph.** Each step is its own gate. This
phase is where you find out what your printed chassis actually can't do, at speeds where finding
out is survivable.

**Steps:**

1. Raise the cap one step. Run it. Read the logs before raising it again.
2. Tune launch and traction control against the Phase 5 signals. **Three slip detectors, tuned in
   this order:** inter-axle difference first (it is the simplest and needs no supervisor), then the
   wheel-acceleration threshold against measured μ, then the supervisor's ground-speed estimate.
   Each one covers a case the previous cannot — see
   [ARCHITECTURE.md](ARCHITECTURE.md#traction-control-across-two-axles).
3. **Tune diff oil**, front and rear. Heavier resists one-wheel spin; lighter preserves steering.
   Start front-light / rear-heavy and adjust against launch traces. This is a mechanical tuning
   knob that does the job electronic traction control cannot — the firmware can reduce torque
   *per axle* but has no authority over which wheel *on* that axle receives it.
3. After every run: check wheel retention, tire condition, motor and inverter temps, and every
   printed part for cracks, creep, or elongated holes.

**Pass criteria, at each step:**

- [ ] Truck tracks straight through the full run with no correction. Twitchiness that appears at
      40 and not at 30 is a geometry or CG problem and it gets worse quadratically.
- [ ] No printed part shows cracking, creep, or hole elongation.
- [ ] Temps stable and safe after a full run.
- [ ] Braking and shutdown work from top speed — you need to be able to stop it.

**Expect to stop somewhere in here.** A printed chassis at 50 mph is doing well. The point of this
phase isn't to reach 50, it's to learn exactly which parts go to aluminum first and why.

---

## Phase 7 — Aluminum chassis

Cut-and-bend, from the flat patterns the printed parts were already designed as. Design rules,
material choice, bend reliefs, and K-factor are in
[MECHANICAL.md](MECHANICAL.md#designing-for-cut-and-bend).

**Steps:**

1. Export flat patterns with bend lines from the CAD you've been maintaining since Phase 4.
2. Have them cut and bent. Check bend angles against the drawing before assembly — ±1° is normal
   and it stacks across multiple bends.
3. Rebuild on metal. Re-weigh everything; the mass number feeds back into `gearing.py`.

**Pass criteria:**

- [ ] **Re-run every Phase 4 and Phase 6 gate on the aluminum chassis**, from 20 mph back up. A new
      chassis is a new vehicle. It will have different stiffness, different resonances, and
      different CG.
- [ ] No fastener loosening after a run. Thread-lock everything; sheet metal at speed vibrates
      differently than printed parts.
- [ ] Measured mass fed back into `gearing.py` and the gearing re-checked.

---

## Phase 8 — 70 mph

Everything is proven. Now it's tires, aero, ballast, and a long enough piece of pavement.

**Requirements before the first attempt:**

- The `record` tire set, rated ≥ 9,500 rpm, balanced. Fresh — not the set you did Phase 6 on.
- `tire.profile` set to `record`. On `test` tires the cap tops out around 63 mph and the
  attempt simply will not run, which is the interlock working, not a bug.
- **~200 m of smooth, closed pavement.** Accelerating to 31 m/s and stopping again needs it; the
  arithmetic is in [SAFETY.md](SAFETY.md#run-site).
- A spotter. Two-person rule, no exceptions, at these speeds.
- Read [SAFETY.md](SAFETY.md) start to finish the morning of.

**Steps:**

1. Raise the cap 55 → 60 → 65 → 70, one run each, logs read between every step.
2. Fit the per-corner ride-height actuators. The independent suspension has been
   waiting for these since Phase 4 — this is what it was for. **Phase 8 is where
   they belong** — they buy in-motion CG control (drop as speed builds, raise
   for launch), which is a refinement on a truck that already works, not a
   prerequisite for one. Phases 4–7 run static adjusters: shock collars or
   adjustable links, a few grams, no failure modes. Re-run the Phase 4 and
   Phase 6 gates after fitting them; four lead screws is four new failure
   modes in the suspension.

   **Check half-shaft angle across the full actuator range before using it.**
   Short 1/16-class shafts turn ±10 mm of ride height into ±13°, and dogbone
   joints at angle produce velocity fluctuation at 8,537 wheel rpm. The usable
   range is set by measured vibration, not by actuator stroke.
3. Tune ballast and CG height for straight-line stability. The spoiler is doing real work here —
   it moves the aerodynamic center of pressure rearward, which is stabilizing in yaw for the same
   reason fletching stabilizes an arrow. Keep the CG forward of it.
4. GPS-verify. Radar only if you want the number to be official.

**Pass criteria:**

- [ ] GPS-logged 70+ mph.
- [ ] Truck comes back intact and repeats it.
- [ ] Log shows the run was stable, not a lucky one — no slip spikes, no thermal fold-back, no
      voltage sag into the cap.

---

## Open items

Not yet decided. Resurface these rather than assuming they're settled:

- **Controller ↔ ESP32 transport.** UART with the JSON-lines protocol today, because it reuses the
  crawler's host tools nearly unchanged. **Two controllers means two links**, which is more wiring
  but also the independence the interlock wants. CAN is the upgrade path and is now more attractive
  than it was: one bus serves both controllers and the ESP32, the F405 has bxCAN natively, and the
  ESP32's TWAI peripheral speaks classic CAN. Revisit at Phase 3 when both links are real.

- **Which controller owns the RC receiver.** One of them must, since the ESP32 cannot. Options are
  a designated primary that relays the setpoint, or a split CRSF line into both. Decide with Phase
  3 data on link timing.
- **Sensorless vs. sensored.** ~~The plan is sensorless~~ — **the chosen motor has halls and the
  chosen controller reads them**, so this is now a firmware choice rather than a hardware bet. Bring
  up sensorless with a forced-commutation startup as planned, because it is the more interesting
  problem and the halls are free to ignore. But the fallback is now genuinely free, and on a course
  the halls carry the low-speed corner exits where observers are weakest — so sensored may win on
  merit rather than as a retreat. Decide with Phase 2 data.
- **Center diff vs. independent axles.** Two motors on two axles with no mechanical coupling means
  the electronic torque split *is* the differential. Whether that wants a mechanical center diff
  as backup is a Phase 6 question.
- **Body mounting and aero.** Where the body sits, how much the wells get trimmed, and whether the
  spoiler needs its angle changed are all Phase 8 tuning, not Phase 4 decisions.
