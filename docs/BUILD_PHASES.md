# Build Phases

**This document is the entry point for the whole project.** Each phase ends with a specific,
checkable result. Don't start the next one until the current one passes — every phase exists
because skipping it makes a later problem impossible to diagnose, and on this vehicle several of
them exist because skipping them destroys hardware or hurts someone.

| Phase | Goal | Done when |
|---|---|---|
| [**P0**](#phase-0--numbers-before-parts) | Lock the gearing math, then buy | BOM committed, numbers close with headroom |
| [P1](#phase-1--one-motor-current-sensing-proven) | One motor, phase current measured true | Currents read zero at rest and sum to zero spinning |
| [P2](#phase-2--closed-loop-foc-on-one-motor) | Torque then velocity control, sensorless | Holds commanded RPM through the whole range |
| [P3](#phase-3--both-channels-on-one-g4) | Dual-motor FOC, synchronized | Both axles track one setpoint; desync trips the interlock |
| [P4](#phase-4--printed-rolling-chassis-20-mph-cap) | Printed mule, rolling, capped | Straight line at 20 mph, failsafe proven |
| [P5](#phase-5--supervisor-tier--gps) | ESP32, GPS, logging | Yank the ESP32 at speed and nothing happens |
| [P6](#phase-6--the-speed-ladder-on-printed-parts) | 30 → 40 → 50 mph, gated | Each step survived and logged; find what breaks |
| [P7](#phase-7--aluminum-chassis) | Cut-and-bend aluminum | Re-validates every earlier gate on metal |
| [P8](#phase-8--70-mph) | The number | GPS-verified 70+ mph, truck intact |

**Two things carry the whole plan.** First: the **speed cap lives in the STM32G4**, is enforced in
firmware below the RC input, and goes up exactly one step per phase gate. Second: **the ESP32 has
no authority over the motors, ever** — if that ever stops being true, the architecture has failed.
See [ARCHITECTURE.md](ARCHITECTURE.md).

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

   | | |
   |---|---|
   | Final drive | 2.5:1 |
   | Pack | 4S, 14.4 V under load |
   | **Required motor kV** | **~2,440** |
   | Motor RPM at the 70 mph cap | 21,345 (61% of no-load) |
   | **Wheel RPM — every configuration** | **8,537** |
   | Steady-state draw | 175 W, 6.1 A per motor |

   **Two tire configurations, one drivetrain.** Top speed changes by changing
   wheels, and nothing else changes at all:

   | Profile | Tire | Top speed | Wheel RPM |
   |---|---|---|---|
   | `street` | 55 mm, tucks in the stock arches | 55 mph | 8,537 |
   | `speed` | 70 mm, for the attempt | 70 mph | 8,537 |

   The reason this works, and it is not obvious: **wheel RPM is constant across
   every tire size.** The motor's RPM ceiling divided by the final drive sets
   it; tire diameter only changes how far the truck travels per revolution.

   That is what retires the tire-RPM problem rather than merely managing it.
   Chasing 70 mph on a 55 mm tire needed 10,866 wheel rpm — 36% over typical
   1/12 pan-car duty. Chasing it on a 70 mm tire with taller gearing needs
   8,537, in *both* configurations. Run `gearing.py --swap`.

   6S at 4.0:1 is the upgrade path if Phase 6 shows heat as the limiter; the
   extra ~80 g costs almost nothing at top speed, which is aero-limited, not
   mass-limited.

   **Gearing costs nothing off the line, so spend it on motor longevity.**
   `accel.py` puts 0-50 mph at ~2.4 s across every ratio from 2.5:1 to 5:1,
   because the entire run is traction-limited — the motors make ~1.7× more
   force than the tires will transmit. And the cap ceilings are set by the
   *tire's* rpm rating, not by gearing, so they don't move either. Gearing is
   therefore free, and the right thing to buy with it is rotational wear.

   At a sustained 70 mph, relative to 3.5:1 (`accel.py --thermal 70`):

   | Ratio | Motor rpm | Loss | Bearing life | Centrifugal stress |
   |---|---|---|---|---|
   | **2.5:1** | **21,345** | **−30%** | **1.40×** | **0.51×** |
   | 3.5:1 | 29,882 | — | 1.00× | 1.00× |
   | 5.0:1 | 42,689 | +66% | 0.70× | 2.04× |

   Bearing L10 life goes as 1/rpm and magnet-retention stress as rpm², so both
   punish a short gear far harder than heat does. 29,882 rpm also sits near the
   practical ceiling for grease-lubricated small bearings; 21,345 is comfortably
   inside it.

   The resulting over-specification is deliberate: a 2,441 kV motor at 2.5:1
   could reach ~96 mph and is capped at 70 by the tire. **Running a motor at 61%
   of its no-load speed IS the wear reduction.**

   If you want a quicker launch, the lever is grip (μ 0.7 → 1.3 moves 0-50 from
   4.0 s to 2.0 s) — not the ratio, not more inverter, and not less mass.

   **A CVT or 2-speed is not worth it here, and the reason is specific.** Both
   exist to give you a short ratio off the line and a tall one at the top. You
   are traction-limited off the line, so the short ratio buys nothing — you want
   the tall one always, which is a fixed gear. A variator would add ~10% drive
   loss plus its own sliding, belted, high-speed wear items: more rotational
   wear than it removes.

   Two consequences worth acting on:

   - **~20 A per motor is where launch force meets the traction ceiling.** 10 A
     is genuinely slow; 45 A is identical to 20 A. The 30 A inverter spec is
     right *as headroom*, not because the extra is usable.
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
- [ ] **Both** tire sets are **foam** and rated above 8,600 rpm. That is only ~7% over typical
      1/12 pan-car duty, which is the entire point of gearing this way. If a vendor won't state an
      RPM rating, treat that as a no.
- [ ] Battery, motor, and inverter current ratings agree with each other at peak, not average.

**Why this is a phase and not a footnote:** buying a motor before doing this arithmetic is how
speed builds end up geared for 45 mph with no way to fix it short of new everything.

---

## Phase 1 — One motor, current sensing proven

The entire point of this phase is to prove you can **measure phase current correctly**. Nothing in
FOC works if the current numbers are wrong, and every later phase compounds the error. This is the
exact analogue of the crawler's "prove you can read an encoder" — same reasoning, harder sensor.

One motor. **No wheel, no load, motor clamped.** A spinning wheel on a bench supply is how bench
work becomes an injury.

**Hardware:** STM32G4 (a B-G431B-ESC1 discovery board is a ready-made single-channel FOC rig and an
excellent Phase 1 target), one motor, a current-limited bench supply. Current limit set low —
start at 2 A. See [WIRING.md](WIRING.md).

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
      **full Phase 8 motor RPM** — 30,000. Test the top of the range now, on the bench, with no
      wheel. Finding an observer that falls apart at 28,000 rpm is much cheaper here.
- [ ] Startup ramp hands over to the observer cleanly, **20 attempts in a row**, no stalls and no
      audible cogging at handover.
- [ ] Recovers from a step change without overshooting into current limit.
- [ ] Pinching the shaft (carefully, gloved, low speed) makes q-axis current rise to compensate.
      An unloaded motor holds a setpoint with almost any gains — load is the real test.

---

## Phase 3 — Both channels on one G4

Second motor, second inverter, same chip. TIM1 and TIM8 both running FOC off one control tick.

Split out as its own phase because dual-channel timing is where the "one G4 instead of two" bet
either pays off or doesn't, and debugging it alongside a rolling chassis is miserable.

**Steps:**

1. Bring up channel B standing alone; re-run every Phase 1 and Phase 2 criterion on it. Yes, all of
   them. This is fast and it's where you catch a swapped phase.
2. Run both channels off the same control ISR. Confirm the loop still meets its deadline with two
   FOC instances — instrument it, don't assume.
3. Implement the torque split and the **sync interlock** (`safety.c`).

**Pass criteria:**

- [ ] Both channels pass Phase 1 + 2 criteria independently.
- [ ] Both hold the same commanded RPM within ~2% of each other, unloaded.
- [ ] Control loop meets its deadline with both channels live, measured, with margin.
- [ ] **The sync interlock actually fires.** Test it deliberately: stall one motor by hand at low
      speed and confirm *both* channels cut, not just the stalled one. An axle losing torque at
      70 mph while the other keeps pushing is a spin.
- [ ] Front/rear torque split responds to the commanded bias.

---

## Phase 4 — Printed rolling chassis, 20 mph cap

Make it a vehicle. **ABS or ASA, not PLA** — PLA creeps near motors and lets go under sustained
load. [MECHANICAL.md](MECHANICAL.md) has the geometry and the design rules.

Print every structural part as the **flat-pattern-folded** geometry it will eventually be in
aluminum. That constraint costs you almost nothing now and means Phase 7 is a material change
rather than a redesign.

**Mechanical:** ~190 mm wheelbase, ~125 mm track, body sitting over it. Steering servo with real
Ackermann. Battery as low and as far forward as it will go.

**Steps:**

1. Print and assemble the chassis, uprights, motor mounts, and steering.
2. Wire the CRSF receiver **directly to the G4** — not through the ESP32. See
   [ARCHITECTURE.md](ARCHITECTURE.md#why-the-receiver-goes-to-the-g4).
3. Set the firmware speed cap to **20 mph** and confirm you cannot exceed it with the stick pinned.
4. Drive it in a straight line somewhere empty.

**Pass criteria:**

- [ ] **RC failsafe works: turn the transmitter off and confirm the truck stops.** Test this
      before the truck is anywhere it can hurt something. Every session, not just once.
- [ ] The speed cap holds with the stick pinned. Verify against wheel RPM.
- [ ] Drives straight at constant throttle. Curving means an axle is scaled wrong or the geometry
      is off — fix it now, at 20 mph, not at 50.
- [ ] Nothing on the truck is too hot to hold after a five-minute run. Motors, inverters, battery.
- [ ] Wheels stay on. Check retention after every run in this phase.

---

## Phase 5 — Supervisor tier + GPS

Add the ESP32, the GPS, and the logging. None of it touches the motors.

**Steps:**

1. Mount the ESP32 and the 10 Hz GPS module. Wire G4 ↔ ESP32 over UART (**common ground required**).
2. Bring up logging and the WiFi telemetry stream.
3. Correlate GPS ground speed against wheel-RPM-derived speed on a Phase 4 run.

**Pass criteria:**

- [ ] Logs a full run without dropping the link.
- [ ] GPS ground speed and wheel-derived speed agree within a few percent on a clean run. Where
      they *disagree* is wheelspin, and that's the signal you'll tune traction control against.
- [ ] **Killing the ESP32 mid-run does not affect driving.** Pull its power at 20 mph and keep
      driving. This is the whole justification for the two-tier split — test it explicitly.

---

## Phase 6 — The speed ladder, on printed parts

Raise the cap one step at a time: **30, then 40, then 50 mph.** Each step is its own gate. This
phase is where you find out what your printed chassis actually can't do, at speeds where finding
out is survivable.

**Steps:**

1. Raise the cap one step. Run it. Read the logs before raising it again.
2. Tune launch and traction control against the GPS-vs-wheel-RPM slip signal from Phase 5.
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

- Foam tires, balanced, RPM-rated above 10,900. Fresh — not the set you did Phase 6 on.
- **~200 m of smooth, closed pavement.** Accelerating to 31 m/s and stopping again needs it; the
  arithmetic is in [SAFETY.md](SAFETY.md#run-site).
- A spotter. Two-person rule, no exceptions, at these speeds.
- Read [SAFETY.md](SAFETY.md) start to finish the morning of.

**Steps:**

1. Raise the cap 55 → 60 → 65 → 70, one run each, logs read between every step.
2. Fit the ride-height actuators if you're building them. **Phase 8 is where
   they belong** — they buy in-motion CG control (drop as speed builds, raise
   for launch), which is a refinement on a truck that already works, not a
   prerequisite for one. Phases 4–7 run static adjusters: shock collars or
   adjustable links, a few grams, no failure modes. Re-run the Phase 4 and
   Phase 6 gates after fitting them; four lead screws is four new failure
   modes in the suspension.
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

- **G4 ↔ ESP32 transport.** UART with the JSON-lines protocol today, because it reuses the crawler's
  host tools nearly unchanged. FDCAN is the upgrade path if UART framing gets tight at high
  telemetry rates — the G4 has it natively and the ESP32's TWAI peripheral speaks classic CAN.
- **Sensorless vs. sensored.** The plan is sensorless with a forced-commutation startup. If the
  Phase 2 handover proves nasty, hall sensors or an encoder are the fallback — decide it with
  Phase 2 data, not in advance.
- **Center diff vs. independent axles.** Two motors on two axles with no mechanical coupling means
  the electronic torque split *is* the differential. Whether that wants a mechanical center diff
  as backup is a Phase 6 question.
- **Body mounting and aero.** Where the body sits, how much the wells get trimmed, and whether the
  spoiler needs its angle changed are all Phase 8 tuning, not Phase 4 decisions.
