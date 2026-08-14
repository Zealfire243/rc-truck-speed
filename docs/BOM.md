# Bill of Materials

**Phase-gated on purpose.** You own a body and nothing else, which means you can buy in the order
you actually need things and let each phase's results inform the next purchase. Committing to gears before μ
is measured is how projects end up geared for grip they do not have, or grip they cannot use.

Prices are rough, USD, for planning only — **check the linked pages, they move.**

### Verified product pages

| Item | Link |
|---|---|
| NUCLEO-G474RE | [ST](https://www.st.com/en/evaluation-tools/nucleo-g474re.html) · [DigiKey](https://www.digikey.com/en/products/detail/stmicroelectronics/NUCLEO-G474RE/497-19491-ND/10231585) · [Mouser](https://www.mouser.com/en/new/stmicroelectronics/stm-nucleo-g474re-board) |
| X-NUCLEO-IHM08M1 | [ST](https://www.st.com/en/evaluation-tools/x-nucleo-ihm08m1.html) · [UM1996 manual](https://www.st.com/resource/en/user_manual/um1996-getting-started-with-xnucleoihm08m1-lowvoltage-bldc-motor-driver-expansion-board-based-on-stl220n6f7-for-stm32-nucleo-stmicroelectronics.pdf) |
| Turnigy XK2845-1980KV motor | [HobbyKing](https://hobbyking.com/en_us/turnigy-xk2845-1980kv-brushless-inrunner.html) — B28-47-16S is discontinued |
| ELRS nano receiver | [RadioMaster RP1](https://radiomasterrc.com/products/rp1-expresslrs-2-4ghz-nano-receiver) · [BetaFPV](https://betafpv.com/products/elrs-nano-receiver) |
| 10 Hz GPS | [Holybro M10](https://holybro.com/products/m10-gps) |
| Pan-car foam tires | [rcMart](https://www.rcmart.com/Foam-Tires-RC-Pan-Car) — covers 1:12, 1:10 and 1:8 |
| B-G431B-ESC1 (fallback) | [ST](https://www.st.com/en/evaluation-tools/b-g431b-esc1.html) — long lead times |

Read the [UM1996 manual](https://www.st.com/resource/en/user_manual/um1996-getting-started-with-xnucleoihm08m1-lowvoltage-bldc-motor-driver-expansion-board-based-on-stl220n6f7-for-stm32-nucleo-stmicroelectronics.pdf)
before Phase 1 — the three-shunt jumper settings (J5/J6) and the shunt values you
need for `csense_init()`'s `amps_per_count` are both in it.

---

## Phase 0 — the μ rig, ~$40–100

Run `gearing.py` and `accel.py`. Then buy the one thing that unblocks everything
downstream, because **μ can be measured without a chassis.**

| Item | Notes | ~$ |
|---|---|---|
| Digital luggage / fish scale, kg | Reads in kgf, which makes the arithmetic trivial — see below. | 10 |
| Candidate tire sets, 2–3 compounds | The things being compared. Sizes and classes in [MECHANICAL.md](MECHANICAL.md#sourcing). | 30–90 |
| A board and some ballast | Scrap. | 0 |

**The drag test.** Mount the wheels under a board, ballast it to the truck's
target mass, and pull it horizontally on the pavement you intend to run on.
Record the force at which it breaks away. Because the scale reads kgf and the
rig mass is in kg, the units cancel:

```
μ = pull force (kgf) ÷ rig mass (kg)
```

A 0.9 kg rig that slides at 1.0 kgf is μ ≈ 1.11.

**What it is and is not.** This measures *sliding* μ. Peak dynamic μ — what a
rolling tire makes at ~10–20% slip — is typically 10–30% higher, so treat the
result as a **conservative lower bound**. What it gives you reliably is the
*relative ranking between compounds*, which is what tire selection actually
needs. Load the rig realistically (μ is load-sensitive), test on the real
surface, and refine with the Phase 4 GPS measurement.

That one number feeds `TIRE_PROFILES[].mu_nominal`, `accel.py --match`, and
therefore the final drive ratio.

---

## Phase 1–2 — one motor on the bench

| Item | Notes | ~$ |
|---|---|---|
| **Nucleo-G474RE** | **The final MCU, bought on day one.** G474 is what the vehicle ships with — TIM1 and TIM8 both free for dual-channel FOC, five ADCs, and no port from a different part later. Also actually in stock, which the B-G431B-ESC1 currently is not. | 20–25 |
| **X-NUCLEO-IHM08M1** | Known-good 3-phase power stage that plugs straight onto the Nucleo. L6398 gate drivers + STL220N6F7 FETs, 8–48 V, three-shunt current sensing (jumpers J5/J6 to the 3-Sh side) — exactly the topology `current_sense.h` assumes, laid out by people who do this for a living. | 30–40 |
| **Brushless motor ×2** | Same batch, open only one. See *Motor sizing* below — the previously listed B28-47-16S is **discontinued**. | 25–60 ea |
| Bench supply, current-limited | Essential, not optional. You will be glad of the current limit. | 60–120 |
| Oscilloscope access | For deadtime verification. Borrowed is fine; this is a handful of measurements. | — |
| ST-Link / SWD debugger | On-board on both dev boards above. | — |
| 4S LiPo + balance charger | Get the charger right; it's a fire-safety item. | 60–100 |

### Why not the B-G431B-ESC1?

It was the earlier recommendation here and it is no longer the better buy. Its whole value was
being a known-good integrated power stage for Phase 1 — but the Nucleo + IHM08M1 pair gives you
the same property, from the same vendor, with the same reference documentation, and adds two
things the ESC1 cannot:

- **It is the final MCU.** The ESC1's G431 is a Phase 1–2 platform you leave behind at Phase 3,
  because its gate drivers are hard-wired to its own onboard MCU. Starting on G474 means no port.
- **It is available.** ESC1 distributor stock is running months out. That is the deciding factor:
  a cheaper board you cannot get is not cheaper.

The ESC1 remains a perfectly good fallback if you find one in stock cheap and want a second bench
rig.

> **The IHM08M1's 15 A RMS rating carries you further than it looks.** Run `IQ_LIMIT_A` at 15 A
> through Phases 1–6 and the boards stay inside their rating; the only cost is a 4.0 s 0-50 instead
> of 2.4 s, which is irrelevant while tuning traction control and chasing geometry. Cruise draw is
> 5.5 A per motor regardless, so only launches challenge the rating and only for seconds at a time.
> The limit is phase-gated in `pin_map.h` exactly like the speed cap, and rises to whatever the
> motor and final stages allow — 20 A with the XK2845, more with a bigger motor.
>
> **So the custom power-stage design is deferred, not cancelled.** You reach it at Phase 7 with the
> motor characterised, μ measured, and real current data from six phases of running — which is a far
> better position to design a board from than guessing now.

### Motor sizing — read before ordering

The B28-47-16S (2400 kV, 470 W, 28 A) is discontinued. Replacing it exposed a relationship worth
understanding, because it means **kV is not the thing to shop on.**

Required wheel force is fixed by traction (μmg). Wheel force is `Kt × iq × FD`, and
`Kt = 8.27/kV`. So if you re-gear to keep motor RPM where you want it, **kV cancels and the
required current is the same.** Lower kV just means a shorter ratio. What kV actually changes is
motor RPM at cruise — the wear axis — and lower wins there.

So shop on **current and power rating**, then pick the ratio to suit.

**The candidate in stock:** [Turnigy XK2845-1980KV](https://hobbyking.com/en_us/turnigy-xk2845-1980kv-brushless-inrunner.html)
— 1980 kV, 4 poles (2 pole pairs, matches `POLE_PAIRS`), 28 × 46.5 mm case, 125 g, 2–4S,
**220 W / 24 A max**, ~$24.

Geared **2.34:1** at a **20 A** limit: full traction-limited launch (2.39 s to 50 mph), ~90 mph
capability capped to 70, and 19,971 rpm at the cap — 70% of no-load, comfortably inside the
bearing zone.

> **It is tighter than the part it replaces.** 197 W peak per motor against 220 W rated is 10%
> margin; 20 A against 24 A is 17%. Workable — and Phases 1–6 run at 15 A regardless — but there is
> no room to raise `IQ_LIMIT_A` later. If you find a 28–35 mm inrunner with a higher power rating
> for similar money, take it instead.

#### If you want better than 2.39 s to 50 mph

**The motor is not what is holding that number — grip is.** The launch is
traction-limited end to end, so a bigger motor changes nothing today. The floor
is `a = μg`:

| μ | 0-50 mph |
|---|---|
| 1.1 | 2.39 s |
| 1.3 | 1.99 s |
| 1.5 | 1.71 s |
| 1.8 | 1.41 s |

So sub-2 s means μ ≥ 1.3, and ~1.5 s means μ ≈ 1.7. That is compound and width,
not hardware — see [MECHANICAL.md](MECHANICAL.md#width-and-compound-are-the-grip-levers--diameter-is-not).

**But the motor becomes the limit the moment you succeed at that**, and the
XK2845 runs out early. Holding the wear-friendly ratio, the current each grip
level demands:

| μ | Matched FD | Motor rpm at cap | Current needed | XK2845 (24 A)? |
|---|---|---|---|---|
| 1.1 | 2.34:1 | 19,971 (70% no-load) | 20.0 A | yes |
| 1.3 | 2.76:1 | 23,603 (83%) | 23.4 A | barely |
| **1.4** | 2.98:1 | 25,418 (89%) | 25.0 A | **no** |
| 1.5 | 3.19:1 | 27,234 (96%) | 26.7 A | **no** |

**The XK2845 caps you at roughly μ 1.35, which is about 1.95 s.** Note the
second squeeze too: chasing grip forces a shorter ratio, which pushes motor RPM
from 70% of no-load to 96% — straight back into the bearing-marginal zone that
2.5:1 was chosen to avoid.

So if sub-2 s is a real goal rather than a nice-to-have, **spec ≥ 30 A and
≥ 350 W**, which is roughly what the discontinued B28-47-16S offered
(28 A / 470 W). That buys the current to use μ 1.5 *and* the freedom to keep the
ratio tall enough to protect the motor. A 30–35 mm can rather than a 28 will
usually get you there, and the extra ~100 g is irrelevant — mass cancels in
traction-limited acceleration.

#### Candidates at ≥ 30 A / ≥ 350 W

HobbyKing does not carry one. Looking wider, the 1/10 **3650** class (36 mm can)
is where the ratings live. Compared at a fixed 2.34:1 so the columns are
like-for-like:

| Motor | Poles | Rating | A @ μ1.1 | A @ μ1.5 | μ ceiling | Sensored |
|---|---|---|---|---|---|---|
| XK2845-1980KV *(current)* | 4 | 24 A / 220 W | 20.0 | 26.7 | **1.34** | no |
| [QuicRun 3650 G2 17.5T](https://www.hobbywingdirect.com/products/quicrun-3650-sensored-2-pole-brushless-motor) — 2170 kV | 2 | 25 A / 420 W cont. | 21.8 | 29.1 | **1.28** | **yes** |
| QuicRun 3650 G2 13.5T — 2850 kV | 2 | 25 A / 420 W cont. | 28.1 | 37.8 | 0.97 | yes |
| [GoolRC 3650 3100KV](https://www.amazon.com/GoolRC-Sensorless-Brushless-ESC%EF%BC%88Electric-Controller%EF%BC%89/dp/B0832DK6P7) | 4 | **56 A / 900 W**, 2–4S | 30.5 | 41.0 | **2.07** | no |

*"μ ceiling" = the grip beyond which that motor's current rating binds.*

**The awkward finding: the sensored options do not solve the current problem.**
The sensored 3650s are ROAR racing motors with modest *continuous* ratings, and
their lower Kt means they draw slightly **more** current than the XK2845 for the
same wheel force. The 17.5T's μ ceiling of 1.28 is worse than what you have.

Only the high-current sensorless class removes the motor from the equation, and
it does so completely — μ 2.07 is beyond anything a foam tire will produce.

> **Continuous ≠ burst.** 25 A continuous is a thermal rating; a 2–3 second
> launch at 29 A is very likely fine. So the QuicRun is not really disqualified,
> it is just unverified. If you can get a burst figure from Hobbywing, it may
> well clear.

**So the two preferences pull opposite ways**, and which you buy depends on
which risk you would rather carry:

- **Buy grip headroom** → GoolRC-class, 3650 sensorless, ≥ 50 A. Meets your
  stated spec outright and you never revisit the motor. Cost: sensorless, and
  3100 kV gives ~36% less back-EMF per rpm than 1980 kV (`Ke = 1/kV`), which
  makes the Phase 2 observer *harder* — on the exact axis that is already the
  project's long pole.
- **Buy schedule certainty** → QuicRun 3650 G2 17.5T, sensored, 2170 kV. Halls
  eliminate the biggest Phase 2 risk entirely, and 2170 kV is in the sweet spot.
  Cost: you may hit its current limit around μ 1.3 and be back here later.

**Recommendation: the GoolRC-class motor**, because it is the only one that
meets the spec you asked for, and because a sensorless observer that will not
track is recoverable — you can bolt an encoder or hall set to the shaft — while
a motor that runs out of current at μ 1.3 means buying motors again.

> ⚠ **Pole count changes firmware.** The QuicRun 3650 is **2-pole**
> (`POLE_PAIRS = 1`); the GoolRC is **4-pole** (`POLE_PAIRS = 2`, which is what
> `pin_map.h` currently has). Get this wrong and every RPM and angle the
> observer produces is out by 2×. Count the magnets and set it before Phase 1.

#### The bundled 60 A ESC — keep it, don't plan around it

The GoolRC motor ships as a combo with a 60 A ESC. **It is not part of this
architecture** and cannot become one: like the B-G431B-ESC1, its gate drivers
are wired to its own onboard MCU, so the G474 cannot reach them. It takes a
throttle signal, not gate commands. 60 A is comfortably above the 30–41 A you
would draw, but that is beside the point.

It is still worth having, for three things:

1. **Motor characterisation before any of your firmware exists.** Spin the motor
   on the ESC with a servo tester and a bench supply and you can read off
   **kV** (rpm ÷ volts at no load) and **`i_noload`** (the supply current while
   it spins free). Both feed straight into `accel.py`, and `i_noload` is
   currently a guess that anchors the entire thermal model. This is Phase 0 work
   you can do while waiting on the Nucleo.
2. **A known-good reference during Phases 1–2.** When your FOC does something
   inexplicable, putting the same motor on the ESC answers "is it my code or my
   motor?" in about a minute. During observer bring-up that is worth a lot.
3. **The documented off-ramp.** If the sensorless observer turns into a swamp,
   commercial ESCs get the truck rolling while you keep developing FOC behind
   `inverter.h`. That was already listed as a legitimate fallback.

**Buying rule:** if the combo costs roughly what a bare motor does, take the
combo — uses 1 and 2 alone justify it. If bare motors are meaningfully cheaper,
buy bare and skip it.

> ⚠ **Check the voltage rating on the exact listing.** Some 3650 3100 kV motors
> are sold as 2S–4S, others as 2S–3S, with otherwise identical specs. That
> rating is really an *RPM and thermal* limit at typical 1/10 gearing, and you
> are geared to ~20,000 rpm at the cap against a 44,640 rpm no-load — so the
> motor never goes near the speed the 3S rating is protecting. The firmware cap
> enforces that. Worth confirming with the vendor anyway, and note the **bundled
> ESC** may be 3S-only even where the motor is not.

**Sensored is still worth paying for where you can get it.** Hall sensors cost nothing to ignore if sensorless works,
and the single biggest risk in Phase 2 is the observer failing to track *this particular motor* —
low saliency or high inductance make the startup handover nasty. A sensored motor turns that from
"buy a different motor and restart the phase" into "use the halls." Cheap insurance on the long
pole.

**Avoid:** 1/8-scale cans (42 mm+, ~300 g each — two would nearly double vehicle mass and force a
chassis redesign), and the low-kV 1/10 car motors, most of which are rated 2–3S only.

### Buy two, from the same batch — but only open one

An earlier version of this file said buy one, to limit the loss if Phase 2 revealed the kV or RPM
ceiling was wrong. That under-weighted the other side:

- **Batch matching matters here.** Kt, R and L vary between production runs, and this is a
  dual-motor design where both channels ideally share gains and where the desync interlock watches
  the difference between two axles. A mismatched pair makes every one of those harder.
- **A specific SKU can go out of stock**, leaving you to pair a motor with something that is not
  quite the same part.
- **Bench work kills motors.** A spare has value whatever happens.

Buy the pair, characterise the first on the bench, and **leave the second sealed until Phase 3** so
it can still go back if Phase 2 changes the spec.

---

## Phase 3 — second channel

The second motor and second IHM08M1 were bought in Phase 1–2 — this is where the sealed ones get
opened.

| Item | Notes | ~$ |
|---|---|---|
| **2nd X-NUCLEO-IHM08M1** *(bought in Phase 1–2)* | The Nucleo already has TIM8 free, so Phase 3 is one more power stage, not a new controller. Buy the *same board again* — identical channels matter where both share gains and the interlock watches the difference between them. Only one can plug onto the shield headers; the second wires to the morpho pins. | 30–40 |
| **Harness materials** | For wiring stage 2 to the morpho headers. Silicone-insulated stranded wire (flexes without fatiguing), **adhesive-lined heatshrink**, a shielded or twisted pair run for the three analog current-sense lines, zip ties and adhesive anchor pads. See [WIRING.md](WIRING.md#strain-relief-and-harness-routing) — the analog lines must not share a bundle with gate drive. | 20–30 |
| Header connectors | Latching or friction-lock, not bare Dupont. Dupont crimps walk off 0.1 in pins under vibration. | 10 |
| Gate drivers, FETs, shunts, bulk caps | If rolling your own power stage. | — |

---

## Phase 4 — printed rolling chassis

| Item | Notes | ~$ |
|---|---|---|
| **Cheap foam tires**, 55 mm (`street`) + 70 mm (`test`) | Consumables for Phases 4–6. Ordinary ~8,600 rpm pan-car foams are fine *because the firmware caps them to ~50 and ~63 mph accordingly*. Buy spares; the ladder destroys tires and that is its job. | 50–90 |
| **Cheap wide foams**, 70 × ~35 (`wide`) | The grip configuration, and where the Phase 4–6 μ work pays off. Same OD and rating as `test`, so same ceiling. **Verify its rating separately — same OD does not mean same rating**, since a wider carcass carries more mass at radius. | 30–55 |
| Steering servo | Fast, metal-gear, decent torque. Not a cheap nylon-gear servo. | 20–40 |
| **CRSF / ELRS receiver + transmitter** | CRSF gets you a 2–4 ms link versus SBUS's 7–14 ms. On this vehicle the RC link is the slowest thing that isn't you, so it's worth the money. | 80–200 |
| **2 × gear differential** + silicone diff oil, assorted weights | One per drive unit, both identical. Tunable oil gives limited-slip behaviour — a Phase 6 tuning knob rather than a fixed choice. Buy a range of weights. | 40–70 |
| Half-shafts / CVDs, 1/16–1/18 class | One per corner, independent suspension. A known weak point at speed. | 25–45 |
| Bearings | Wheel bearings see 8,537 rpm at the cap; driveline and motor bearings see up to ~20,000. Buy quality — those speeds find cheap bearings. | 20–40 |
| Drive gears, single spur pair, no bevel | **Do not cut these until μ is measured.** Ratio depends on measured grip and the motor's current rating — 2.34:1 with the XK2845 at μ 1.1. Same ratio for every tire profile; the gearing never changes with tires. See `accel.py --match`. | 20–50 |
| **Static ride-height adjusters** | Shock collars or adjustable links — sets ride height through Phases 4–7 and absorbs the 7.5 mm of axle rise on a tire swap. A few grams, no failure modes. Powered actuators are Phase 8. | 15 |
| ABS or ASA filament | **Not PLA.** See [MECHANICAL.md](MECHANICAL.md#material). | 25–35 |
| Fasteners, thread-lock, nutserts | Buy more than you think. | 30 |

---

## Phase 5 — supervisor tier

| Item | Notes | ~$ |
|---|---|---|
| ESP32 DevKit | Any of them. It's doing nothing hard. | 8–15 |
| **10 Hz GPS module** | 10 Hz, not 1 Hz. A 1 Hz module samples once every 31 meters at target speed, which tells you nothing useful. | 20–40 |
| microSD module + card | Logging. | 10 |

---

## Phase 6 — the speed ladder

Mostly consumables, because this phase is where things break. That's its job.

| Item | Notes | ~$ |
|---|---|---|
| Spare tires | You will destroy some. Budget for it. | 50 |
| Spare printed parts | Print duplicates of anything load-bearing before you go out. | — |
| Temperature probe / IR thermometer | For the post-run checks. | 20 |

---

## Phase 7 — aluminum

| Item | Notes | ~$ |
|---|---|---|
| **5052-H32 sheet**, 1.5 mm and 2.0 mm | For anything with a bend in it. | — |
| 6061-T6 plate | Flat parts only — it cracks when bent tightly. | — |
| Cut and bend | In-house. | — |
| Nutserts + setting tool | Thin sheet has no thread engagement worth trusting. | 40 |

---

## Phase 8 — the attempt

| Item | Notes | ~$ |
|---|---|---|
| **`record` foam tires**, 70 × 25 | **Good for ≥ 9,500 rpm — this is the spec, not a preference.** A derated 8,600 rpm tire only licenses ~63 mph, so cheap tires physically cannot pass the Phase 8 gate. Vendors rarely publish RPM; derive it from the speed the tire is sold for. Fresh, not the Phase 6 set. | 60–120 |
| **4 × linear actuator**, lead-screw class | Per-corner ride height. Self-locking so they hold position with no power and fail in place. **Usable travel is bounded by half-shaft angle, not actuator stroke** — see [MECHANICAL.md](MECHANICAL.md#independent-uprights--settled-not-open). | 80–200 |
| Wheel balancer | At 8,537 rpm and ~2,850 g of centrifugal load at the tread, imbalance is a real force. | 20 |
| Radar gun | Only if you want the number to be official. GPS is enough to know. | 100+ |

---

## Alternatives considered — controllers and power stages

Ratings below are ballpark; verify against the datasheet for whatever you
actually buy.

### Controllers

The requirement is specific: **two advanced-control timers** (3 complementary
PWM pairs each, with break inputs) and **ADCs that can be triggered by the
timer** so phase current is sampled at the same instant every PWM cycle. That
second one eliminates most general-purpose micros.

| Part | Verdict |
|---|---|
| **STM32G474** *(chosen)* | 170 MHz M4F, TIM1/TIM8/**TIM20** — three advanced timers, so a third channel is even possible. 5 ADCs. CORDIC accelerator sits exactly where `foc_sincos()` wants it. Purpose-built for this. |
| STM32G431 | Same family, fewer ADCs and less RAM. What the B-G431B-ESC1 carries. No reason to prefer it once you own a G474. |
| **TI C2000** (e.g. LAUNCHXL-F280049C) | Genuinely the best silicon for this — sub-cycle PWM resolution, very fast ADCs, a coprocessor for control math. The cost is a different toolchain (Code Composer), a much smaller hobby community, and a steeper start. If motor control itself were the project rather than the truck, this would be the pick. |
| STM32H7 | Far more compute, similar motor peripherals. Overkill; the G474 is not the bottleneck. |
| STM32F303 | The previous generation's motor-control part. Works, but slower and superseded. |
| ESP32 / ESP32-S3 | **Not for the FOC layer.** No PWM-synchronized injected ADC conversion, which is the whole requirement. Correctly used as the supervisor — see [ARCHITECTURE.md](ARCHITECTURE.md). |
| RP2040 / RP2350 | PIO is clever, but the ADC is slow and cannot be timer-triggered into a synchronized sample. Same disqualification. |

### Power stages

**Expect two different boards.** Nothing off-the-shelf is both convenient for
bench bring-up and small enough for a 105 mm-wide truck. That is a real split,
not a failure to shop properly.

**Bring-up (bench, current-limited, convenience wins):**

| Option | Notes |
|---|---|
| **X-NUCLEO-IHM08M1** *(chosen)* | ~15 A RMS / 30 A peak, 8–48 V, three-shunt. Plugs onto the Nucleo. The shield footprint is awkward to mount and fragile in a crash, but it fits and its mass is irrelevant — see the [CG budget](BUILD_PHASES.md#check-4--cg-height-budget). |
| B-G431B-ESC1 | Integrated MCU + stage, compact. Dead-ends at Phase 3 (gate drivers hard-wired to its own MCU) and supply-constrained. |
| TI BOOSTXL-DRV8305 | ~45 V, ~15 A, integrated current-sense amps. Pairs naturally with a C2000 LaunchPad. |

**Flight hardware (~25 A+, 4S):**

| Option | Notes |
|---|---|
| **Custom DRV8353 / DRV8323 board** | Gate driver with integrated current-sense amplifiers, your FETs, your shunts, your layout. The route that gives real current headroom in a package that mounts cleanly. The risk is layout — shunt placement, Kelvin connections, and keeping analog away from switching nodes are what decide whether Phase 1's current readings are honest. |
| Drone ESC hardware | Right size, right weight, right current, cheap. Either repurpose the power stage with your own gate drive, or take the documented off-ramp: flash **AM32** and command it over bidirectional DShot behind `inverter.h`. |
| VESC / Flipsky (incl. dual-channel) | Open-source FOC, excellent firmware, and dual-channel boards exist. But they are built for e-skateboards — 200–300 g and rated for several times the current needed. Mass is not the objection (it cancels in traction-limited acceleration); bulk and cost are. |
| ODrive S1 / Pro | Excellent hardware, open source, high current. Same problem: sized and priced for robotics, not for a 900 g truck. |

## Deliberately not on this list

- **A commercial ESC.** The whole point of the STM32G4 route is that you're building the
  commutation yourself. Worth noting the fallback exists though: if the Phase 2 sensorless observer
  turns into a swamp, a pair of ESCs running AM32 with bidirectional DShot would get the truck
  running while you keep working on your own FOC behind the `inverter.h` interface. That's a
  legitimate off-ramp, not a failure.
- **A Raspberry Pi.** The crawler has one for the camera. There is nothing here a Pi does that the
  ESP32 doesn't, and it would want more power and more space for no return.
- **A center diff.** Two motors on two axles means the electronic torque split *is* the
  differential. Whether that needs mechanical backup is a Phase 6 question, not a Phase 0 purchase.
