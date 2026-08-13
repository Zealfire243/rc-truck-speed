# Bill of Materials

**Phase-gated on purpose.** You own a body and nothing else, which means you can buy in the order
you actually need things and let each phase's results inform the next purchase. Buying the Phase 8
motor during Phase 0 is how projects end up geared for the wrong speed with no way back.

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

## Phase 0 — the μ rig, ~$40

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
> The limit is phase-gated in `pin_map.h` exactly like the speed cap, and rises to 25 A when final
> stages go in.
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

**Strongly consider a sensored motor.** Hall sensors cost nothing to ignore if sensorless works,
and the single biggest risk in Phase 2 is the observer failing to track *this particular motor* —
low saliency or high inductance make the startup handover nasty. A sensored motor turns that from
"buy a different motor and restart the phase" into "use the halls." Cheap insurance on the long
pole.

**Avoid:** 1/8-scale cans (42 mm+, ~300 g each — two would nearly double vehicle mass and force a
chassis redesign), and the low-kV 1/10 car motors, most of which are rated 2–3S only.

### Buy two, from the same batch — but only open one

An earlier version of this file said buy one, to limit the loss if Phase 2 revealed the kV or RPM
ceiling was wrong. That under-weighted the other side:

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

| Item | Notes | ~$ |
|---|---|---|
| Second motor | Identical to the first. Order the *same* part, same batch if possible. | 30–60 |
| **2nd X-NUCLEO-IHM08M1** | The Nucleo already has TIM8 free, so Phase 3 is one more power stage, not a new controller. Buy the *same board again* — identical channels matter where both share gains and the interlock watches the difference between them. Only one can plug onto the shield headers; the second wires to the morpho pins. | 30–40 |
| **Harness materials** | For wiring stage 2 to the morpho headers. Silicone-insulated stranded wire (flexes without fatiguing), **adhesive-lined heatshrink**, a shielded or twisted pair run for the three analog current-sense lines, zip ties and adhesive anchor pads. See [WIRING.md](WIRING.md#strain-relief-and-harness-routing) — the analog lines must not share a bundle with gate drive. | 20–30 |
| Header connectors | Latching or friction-lock, not bare Dupont. Dupont crimps walk off 0.1 in pins under vibration. | 10 |
| Gate drivers, FETs, shunts, bulk caps | If rolling your own power stage. | — |

---

## Phase 4 — printed rolling chassis

| Item | Notes | ~$ |
|---|---|---|
| **Cheap foam tires**, 55 mm (`street`) + 70 mm (`test`) | Consumables for Phases 4–6. Ordinary ~8,600 rpm pan-car foams are fine *because the firmware caps them to ~50 and ~63 mph accordingly*. Buy spares; the ladder destroys tires and that is its job. | 50–90 |
| Steering servo | Fast, metal-gear, decent torque. Not a cheap nylon-gear servo. | 20–40 |
| **CRSF / ELRS receiver + transmitter** | CRSF gets you a 2–4 ms link versus SBUS's 7–14 ms. On this vehicle the RC link is the slowest thing that isn't you, so it's worth the money. | 80–200 |
| **2 × gear differential** + silicone diff oil, assorted weights | One per drive unit, both identical. Tunable oil gives limited-slip behaviour — a Phase 6 tuning knob rather than a fixed choice. Buy a range of weights. | 40–70 |
| Half-shafts / CVDs, 1/16–1/18 class | One per corner, independent suspension. A known weak point at speed. | 25–45 |
| Bearings | Wheel and driveline. Buy quality — 10,900 rpm finds cheap bearings. | 20–40 |
| Drive gears, 2.5:1 (single spur pair, no bevel) | Per the Phase 0 math. Same ratio for both tire profiles — the gearing never changes. | 20–50 |
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
| **Foam tires + wheels**, 70 mm OD (`speed`) | The second configuration — same chassis, same gearing, 70 mph. Wheels sit proud of the arches. Also rated above 8,600 rpm; wheel RPM is identical to the street set. | 25–50 |
| **Cheap wide foams**, 70 × ~35 (`wide`) | Grip configuration for the Phase 4–6 μ work. Same OD and rating as `test`, so same ceiling. **Verify its rating separately: same OD does not mean same rating**, since a wider carcass carries more mass at radius. Enter whatever it actually is into `TIRE_PROFILES`. | 30–55 |
| Ride-height adjusters | Shock collars or adjustable links, to give back the 7.5 mm of CG the bigger tire adds. A few grams. Powered actuators are a separate, later decision — see [MECHANICAL.md](MECHANICAL.md#ride-height-and-the-tire-swap). | 15 |
| Wheel balancer | At 10,900 rpm imbalance is a real force. | 20 |
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
| **X-NUCLEO-IHM08M1** *(chosen)* | ~15 A RMS / 30 A peak, 8–48 V, three-shunt. Plugs onto the Nucleo. Arduino-shield footprint, so useless in the truck. |
| B-G431B-ESC1 | Integrated MCU + stage, compact. Dead-ends at Phase 3 (gate drivers hard-wired to its own MCU) and supply-constrained. |
| TI BOOSTXL-DRV8305 | ~45 V, ~15 A, integrated current-sense amps. Pairs naturally with a C2000 LaunchPad. |

**Flight hardware (~25 A, 4S, small and light):**

| Option | Notes |
|---|---|
| **Custom DRV8353 / DRV8323 board** | Gate driver with integrated current-sense amplifiers, your FETs, your shunts, your layout. Smallest and lightest, and the only route that fits two stages in this truck properly. The risk is layout — shunt placement, Kelvin connections, and keeping analog away from switching nodes are what decide whether Phase 1's current readings are honest. |
| Drone ESC hardware | Right size, right weight, right current, cheap. Either repurpose the power stage with your own gate drive, or take the documented off-ramp: flash **AM32** and command it over bidirectional DShot behind `inverter.h`. |
| VESC / Flipsky (incl. dual-channel) | Open-source FOC, excellent firmware, and dual-channel boards exist. But they are built for e-skateboards — 200–300 g and rated for several times the current needed. Too big and heavy for a 1/16-class truck. |
| ODrive S1 / Pro | Excellent hardware, open source, high current. Same problem: sized and priced for robotics, not for a 900 g truck. |

## Deliberately not on this list

- **A commercial ESC.** The whole point of the STM32G4 route is that you're building the
  commutation yourself. Worth noting the fallback exists though: if the Phase 2 sensorless observer
  turns into a swamp, a pair of ESCs running AM32 with bidirectional DShot would get the truck
  running while you keep working on your own FOC behind the `inverter.h` interface. That's a
  legitimate off-ramp, not a failure.
- **A Raspberry Pi.** The crawler has one for the camera. This truck needs to be light, and there's
  nothing here a Pi does that the ESP32 doesn't.
- **A center diff.** Two motors on two axles means the electronic torque split *is* the
  differential. Whether that needs mechanical backup is a Phase 6 question, not a Phase 0 purchase.
