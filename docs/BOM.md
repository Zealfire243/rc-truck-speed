# Bill of Materials

**Phase-gated on purpose.** You own a body and nothing else, which means you can buy in the order
you actually need things and let each phase's results inform the next purchase. Buying the Phase 8
motor during Phase 0 is how projects end up geared for the wrong speed with no way back.

Prices are rough, USD, for planning only.

---

## Phase 0 — nothing

Run `gearing.py`. Decide the numbers. Spend zero dollars.

---

## Phase 1–2 — one motor on the bench

| Item | Notes | ~$ |
|---|---|---|
| **STM32G4 dev board** | A **B-G431B-ESC1** discovery is the strong pick: it's a complete single-channel FOC test rig with gate driver, FETs, and current sensing already laid out and known-good. Being able to trust the hardware while you debug your own FOC is worth a great deal in Phase 1. A Nucleo-G474RE is the alternative if you'd rather build the power stage yourself. | 25–35 |
| **One brushless motor** | ~2,440 kV, 4S-capable. **Check the manufacturer's max RPM against 30,000 with margin before ordering** — this is the spec most often omitted and most often exceeded. | 30–60 |
| Bench supply, current-limited | Essential, not optional. You will be glad of the current limit. | 60–120 |
| Oscilloscope access | For deadtime verification. Borrowed is fine; this is a handful of measurements. | — |
| ST-Link / SWD debugger | On-board on both dev boards above. | — |
| 4S LiPo + balance charger | Get the charger right; it's a fire-safety item. | 60–100 |

**Buy one motor, not two.** If Phase 2 reveals the kV or the RPM ceiling is wrong, you've learned
it for the price of one motor.

---

## Phase 3 — second channel

| Item | Notes | ~$ |
|---|---|---|
| Second motor | Identical to the first. Order the *same* part, same batch if possible. | 30–60 |
| Custom dual inverter board | This is where the B-G431B-ESC1 runs out — it's single-channel. Either two of them driven by one G4, or a purpose-built dual board. **Size the FETs for ~30 A continuous per channel.** `accel.py` shows launch force meets the traction ceiling at ~20 A — below that you are genuinely slower, above it you gain nothing. So 30 A is headroom over a real requirement, not a guess, and 45 A would be wasted copper. | 40–150 |
| Gate drivers, FETs, shunts, bulk caps | If rolling your own power stage. | — |

---

## Phase 4 — printed rolling chassis

| Item | Notes | ~$ |
|---|---|---|
| **Foam tires + wheels**, 55 mm OD (`street`) | Tucks in the stock arches, good for 55 mph. **RPM-rated above 8,600** — pan-car foams are the right category and this is only ~7% over their usual duty. If the vendor won't state a rating, buy elsewhere. | 25–50 |
| Steering servo | Fast, metal-gear, decent torque. Not a cheap nylon-gear servo. | 20–40 |
| **CRSF / ELRS receiver + transmitter** | CRSF gets you a 2–4 ms link versus SBUS's 7–14 ms. On this vehicle the RC link is the slowest thing that isn't you, so it's worth the money. | 80–200 |
| Bearings | Wheel and driveline. Buy quality — 10,900 rpm finds cheap bearings. | 20–40 |
| Drive gears, 3.5:1 | Per the Phase 0 math. Same ratio for both tire profiles — the gearing never changes. | 20–50 |
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
