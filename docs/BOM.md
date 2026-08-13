# Bill of Materials

**Phase-gated on purpose.** You own a body and nothing else, which means you can buy in the order
you actually need things and let each phase's results inform the next purchase. Buying the Phase 8
motor during Phase 0 is how projects end up geared for the wrong speed with no way back.

Prices are rough, USD, for planning only.

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

> **Note the IHM08M1's rating: 15 A RMS nominal, 30 A peak.** That is ample for Phase 1–2, where
> the bench supply is current-limited to a few amps anyway, and marginal for the vehicle's 25 A
> launch bursts. Treat it as a bring-up and bench tool, not necessarily the flight hardware — the
> Phase 3 power stages are still a design decision, and by then you will have measured the motor
> and know what current you actually need.

**Buy two motors, from the same batch — but only open one.**

An earlier version of this file said buy one, to limit the loss if Phase 2 revealed the kV or RPM
ceiling was wrong. That under-weighted the other side:

- **Batch matching matters here.** Kt, R and L vary between production runs, and this is a
  dual-motor design where both channels ideally share gains and where the desync interlock watches
  the difference between two axles. A mismatched pair makes every one of those harder.
- **A specific SKU can go out of stock**, leaving you to pair a motor with something that is not
  quite the same part.
- **Bench work kills motors.** A spare has value whatever happens.

The spec risk that motivated "buy one" is genuinely low — a 28 × 47 mm 2400 kV inrunner is a close
match to the design point. So buy the pair, characterise the first one on the bench, and **leave
the second sealed until Phase 3** so it can still go back if Phase 2 changes the spec.

**The Phase 2 outcome that would change the motor** is the sensorless observer failing to track
this particular motor — low saliency or high inductance can make handover nasty. If that happens
the answer may be a sensored motor, which is a different part. That is the real risk, not kV.

---

## Phase 3 — second channel

| Item | Notes | ~$ |
|---|---|---|
| Second motor | Identical to the first. Order the *same* part, same batch if possible. | 30–60 |
| **2nd 3-phase power stage** | The Nucleo already has TIM8 free, so Phase 3 is one more power stage, not a new controller. Only one shield can plug onto the Nucleo, so the second wires to the morpho headers. **Size for ~30 A continuous per channel** — above the 25 A limit and the IHM08M1's 15 A RMS, so this is where the bring-up hardware likely gets replaced on both channels. `accel.py` shows launch force meets the traction ceiling at ~23 A — below that you are genuinely slower, above it you gain nothing. So 30 A is headroom over a real requirement, not a guess, and 45 A would be wasted copper. | 40–150 |
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
