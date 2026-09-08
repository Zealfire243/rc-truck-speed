# Bill of Materials

**Phase-gated on purpose.** You own a body and nothing else, which means you can buy in the order
you actually need things and let each phase's results inform the next purchase. Committing to gears before μ
is measured is how projects end up geared for grip they do not have, or grip they cannot use.

Prices are rough, USD, for planning only — **check the linked pages, they move.**

### Verified product pages

| Item | Link |
|---|---|
| NUCLEO-G474RE *(bench only)* | [ST](https://www.st.com/en/evaluation-tools/nucleo-g474re.html) · [DigiKey](https://www.digikey.com/en/products/detail/stmicroelectronics/NUCLEO-G474RE/497-19491-ND/10231585) · [Mouser](https://www.mouser.com/en/new/stmicroelectronics/stm-nucleo-g474re-board) |
| X-NUCLEO-IHM08M1 *(bench only)* | [ST](https://www.st.com/en/evaluation-tools/x-nucleo-ihm08m1.html) · [UM1996 manual](https://www.st.com/resource/en/user_manual/um1996-getting-started-with-xnucleoihm08m1-lowvoltage-bldc-motor-driver-expansion-board-based-on-stl220n6f7-for-stm32-nucleo-stmicroelectronics.pdf) |
| **Flipsky Mini FSESC6.7 Pro** *(flight)* | [Flipsky](https://flipsky.net/products/mini-fsesc6-7-pro-70a) · [dual variant](https://flipsky.net/products/dual-mini-fsesc6-7-pro-140a) |
| **QuicRun 3650 G2 sensored motor** | [Hobbywing — full spec table for every winding](https://www.hobbywingdirect.com/products/quicrun-3650-sensored-2-pole-brushless-motor) |
| ELRS nano receiver | [RadioMaster RP1](https://radiomasterrc.com/products/rp1-expresslrs-2-4ghz-nano-receiver) · [BetaFPV](https://betafpv.com/products/elrs-nano-receiver) |
| 10 Hz GPS | [Holybro M10](https://holybro.com/products/m10-gps) |
| Pan-car foam tires | [rcMart](https://www.rcmart.com/Foam-Tires-RC-Pan-Car) — covers 1:12, 1:10 and 1:8 |

Read the [UM1996 manual](https://www.st.com/resource/en/user_manual/um1996-getting-started-with-xnucleoihm08m1-lowvoltage-bldc-motor-driver-expansion-board-based-on-stl220n6f7-for-stm32-nucleo-stmicroelectronics.pdf)
before Phase 1 — the three-shunt jumper settings (J5/J6) and the shunt values you
need for `csense_init()`'s `amps_per_count` are both in it.

---

## The three-stage hardware plan

**You are writing your own FOC firmware, and you are not designing a PCB.** Those two
commitments used to be in tension — the only high-current power stage you could drive
with your own code was one you laid out yourself. They aren't in tension any more,
because VESC-class hardware is an STM32 bolted to a 70 A inverter with open schematics,
and you can flash your own firmware onto it.

| Stage | Hardware | Current | Purpose |
|---|---|---|---|
| **1 — Learn** (P1–P2) | Nucleo-G474RE + X-NUCLEO-IHM08M1 | **≤ 10 A** | Make the classic mistakes into a $35 board with a vendor manual open |
| **2 — Port** (P2b) | Flipsky Mini FSESC6.7 Pro, one motor | 70 A capable | Same algorithms, new HAL. Bounded, well-understood work |
| **3 — Fly** (P3+) | 2 × FSESC6.7 Pro + ESP32 | 70 A per channel | One MCU per motor, no pin conflicts, no custom PCB ever |

**What this reverses.** An earlier version of this file bought the G474 on day one
specifically to *avoid* a port later. That argument is dead: there is now a planned port
at Stage 2. The trade is deliberate — **you pay a bounded HAL port to avoid designing,
fabricating and debugging a power stage.** That is a very good trade, and the G474 is
still worth its $25 as the platform where you learn, because ST ships a reference manual
and MCSDK examples to diff against when your commutation does something inexplicable.

**The G474 does not ship in the vehicle.** It is a bench instrument. The F405s on the two
FSESC boards run your firmware in the truck, and the ESP32 supervises them exactly as
[ARCHITECTURE.md](ARCHITECTURE.md) already specifies.

> ⚠ **`pin_map.h` is currently 353 lines of G474 assignments.** Stage 2 rewrites it, along
> with `inverter.c` and `current_sense.c`. `foc.c`, `observer.c`, `speed_loop.c` and
> `safety.c` are math and policy — they port unchanged. Keep that boundary clean from
> Phase 1 onward and Stage 2 stays a week rather than a month.

### Two vehicles, one truck

The duty cycles are different enough to be worth naming, because they pull the
hardware in different directions:

| | Speed run | Course running |
|---|---|---|
| Duration at power | ~2 s | **3–5+ min** |
| What binds | Peak current, grip | **Continuous thermal rating** |
| Cooling that matters | Thermal mass (trivial) | **Thermal resistance + airflow** |
| Duct | Blank it — cooling drag costs you | Open |
| Weight | Strip it | Less critical |

An earlier version of this file argued that "only launches challenge the rating, and
only for seconds at a time." **That is true for the speed run and false for course
running**, and it is why the motor's *continuous* rating is now a first-class criterion
rather than a footnote.

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

## Stage 1 / Phase 1–2 — one motor on the bench

| Item | Notes | ~$ |
|---|---|---|
| **Nucleo-G474RE** | **The learning platform, not flight hardware.** Bought because ST ships a reference manual and MCSDK examples you can diff against when commutation misbehaves — that is worth $25 on its own. TIM1/TIM8/TIM20, five ADCs, CORDIC. Retired to the bench at Stage 2. | 20–25 |
| **X-NUCLEO-IHM08M1** | Known-good 3-phase power stage that plugs straight onto the Nucleo. L6398 gate drivers + STL220N6F7 FETs, 8–48 V, three-shunt current sensing (jumpers J5/J6 to the 3-Sh side) — exactly the topology `current_sense.h` assumes. **Read the current ceiling below before planning around it.** | 30–40 |
| **Brushless motor ×2** | Same batch, open only one. See *Motor selection* below. | 30–45 ea |
| Bench supply, current-limited | **Essential, not optional.** This is the single most important line in the file — it is what makes Stage 1 mistakes survivable. Start the limit at 2 A. | 60–120 |
| Oscilloscope access | For deadtime verification. Borrowed is fine; this is a handful of measurements. | — |
| ST-Link / SWD debugger | On-board on the Nucleo — **and it detaches.** Pull the two **CN2** jumpers and the Nucleo's ST-Link becomes a standalone programmer driving an external target through the **CN4** SWD header. So the Nucleo can flash the FSESC at Stage 2 and the separate debugger below is probably unnecessary. Verify against UM1724 before skipping the purchase. | — |
| **4S LiPo + balance charger** | **DECIDED and in hand: 4S 1550 mAh 100C softpack, 72 × 35 × 37 mm, XT60.** Softpack is mandatory — a hardcase is ~105 mm against a 97 mm bay. Capacity is set by the 3–5 min course case (~3.7 min at a 20 A average), not by the speed run, and the C rating is bought for low IR rather than for the label: real draw is only ~21C at the cap. Full derivation in [MECHANICAL.md](MECHANICAL.md#-pack-current-is-not-phase-current-and-an-earlier-version-of-this-file-confused-them). Get the charger right; it's a fire-safety item. | 60–100 |

### ⚠ The IHM08M1 tops out near 12 A, and no amount of cooling changes that

An earlier version of this file said the 15 A<sub>RMS</sub> rating "carries you further than it
looks" and treated it as a thermal limit you could design around. **It is not thermal. It is
current-sense amplifier saturation**, and that distinction matters because it means heatsinks,
ducting and short duty cycles all buy you exactly nothing:

```
10 mΩ shunt × ~13.7 total op-amp gain, bidirectional, biased at 1.65 V
1.65 V ÷ 13.7 ÷ 0.010 Ω ≈ 12 A peak measurable
```

Past that the amplifier clips and the current reading silently stops being true — which is the
one failure mode Phase 1 exists to prevent. The STL220N6F7 FETs are 60 V / ~2.4 mΩ parts that
would pass 50 A given cooling; **the board's power stage is not the constraint, its instrumentation
is.** The 30 A peak OCP trip is separately programmable via CURRENT REF on CN10-27 (a DAC output),
so that number is firmware, not silicon.

> ⚠ **Verify the gain for your jumper configuration before computing `amps_per_count`.** UM1996
> Table 1 lists **JP2 as "op-amp gain modification"**, and the FOC configuration wants JP1/JP2
> *closed*. The 13.7× figure above may not be your gain. Measure it.

**Run `IQ_LIMIT_A` at 10 A through Stage 1** and stay well inside the honest measurement range.
That is plenty to prove current sensing, close both loops, and get the observer handing over —
which is everything Phases 1 and 2 are for. Full current arrives at Stage 2 with the port.

**There is no drop-in replacement.** The IHM08M1 is the top of the entire X-NUCLEO motor-control
family — the next one down, IHM07M1, is 1.4 A<sub>RMS</sub>. TI's BOOSTXL-DRV8323RS and
DRV8353RS-EVM are both 15 A too. Every vendor's low-voltage eval board lands in the same place,
because that is what a bare PCB with no airflow will take. Shopping for a bigger shield is a dead
end; that is why Stage 2 exists.

### Why not the B-G431B-ESC1?

Its gate drivers are hard-wired to its own onboard G431, so you cannot reach them from your own
MCU — the same reason a bundled RC ESC cannot become part of this architecture. It is a fine
second bench rig if you find one cheap, and distributor stock has historically run months out.

The FSESC6.7 Pro does what the ESC1 could not: it is an integrated MCU + high-current stage
**whose MCU you are allowed to program.**

### Motor selection — read before ordering

Two earlier framings in this file were wrong and are worth correcting explicitly, because
both of them pointed at the wrong motor.

**Wrong framing 1: "kV cancels, so shop on current and power rating."** True only while the
gear ratio is free, and it led to comparing motors at a fixed 2.34:1 — which flatters
low-kV motors and penalises high-kV ones for no physical reason.

**Wrong framing 2: "only launches challenge the rating."** True for the speed run, false for
3–5 minutes of course running. Continuous rating is now a first-class criterion.

#### The one number that actually ranks motors

Take Hobbywing's published table for the QuicRun 3650 G2 line and compute `kV² × R` for
every winding:

| Turns | kV | R (mΩ) | Max output | A @ MOP | **kV²·R** | **I²R at MOP** |
|---|---|---|---|---|---|---|
| 10.5T | 3600 | 18.3 | 240 W | 62 A | 237,200 | 70 W |
| 13.5T | 2850 | 28.9 | 180 W | 50 A | 234,700 | 72 W |
| 17.5T | 2170 | 48.8 | 125 W | 35 A | 229,800 | 60 W |
| 21.5T | 1800 | 74.7 | 95 W | 28 A | 242,000 | 59 W |
| 25.5T | 1520 | 100 | — | — | 231,000 | — |

**Five windings, one constant — ±2.6%.** That is not coincidence: it is the same copper
rearranged, turns up, kV down as `1/N`, resistance up as `N²`. The quantity is the standard
**motor constant** `Km = Kt/√R`, and it measures copper volume — i.e. can size.

Two consequences fall straight out.

**First, the current column is a thermal limit wearing five different hats.** Every row
dissipates 60–72 W of copper loss at its rated current. That is what a 186 g 3650 can shed.
Each winding's rating is just `√(60/R)`. **For 3–5 minute course running, halve it** — at
30 W sustained the 17.5T gives `√(30/0.0488) = 25 A`, which is exactly the "25 A cont."
figure this document already carried from another source. Same curve, half the dissipation.

**Second, motor heating does not depend on pack voltage.**

```
I   = τ·kV/8.27           (current for torque τ)
I²R = (τ/8.27)² · kV²R    ← and kV²R is constant for a given can
```

**Copper loss is a function of torque and can size alone.** Not voltage, not kV, not RPM.
Pick the right winding for whatever pack you choose and the motor runs exactly as hot:

| Pack | Winding | kV | R | Current @ 30 W | Power |
|---|---|---|---|---|---|
| 2S (7.4 V) | 6.5T | 5750 | ~7 mΩ | 65 A | ~480 W |
| 3S (11.1 V) | 10.5T | 3600 | 18.3 mΩ | 40 A | ~450 W |
| **4S (14.8 V)** | **13.5T** | **2850** | **28.9 mΩ** | **32 A** | **~475 W** |
| 5S (18.5 V) | 17.5T | 2170 | 48.8 mΩ | 25 A | ~460 W |

Same power, same motor temperature, every row. **Voltage only decides where the amps are** —
and since ESC and harness loss both go as `I²`, higher voltage is easier on everything
downstream of the motor. That is the entire case for 4S over 3S: 1.8× less controller
heating for identical motor behaviour.

> ⚠ **Compare `kV²R` within a brand only.** Some manufacturers quote phase-to-phase
> resistance, some per-phase — a 2× difference that will silently corrupt any cross-brand
> ranking. A HobbyStar 3670 quotes 16.5 mΩ at 1850 kV, implying twice the 3650's motor
> constant for a can only 33% longer. That is too good; measurement convention is the likely
> explanation.

#### Cell count is an RPM limit in disguise

Hobbywing publishes cell ratings for some windings and not others. Work out what they mean:

| Winding | kV | Rated cells | kV × V<sub>max</sub> |
|---|---|---|---|
| 6.5T | 5750 | 2S (8.4 V) | **48,300 rpm** |
| 10.5T | 3600 | 2–3S (12.6 V) | **45,360 rpm** |
| 13.5T | 2850 | *unstated* | 4S → **47,880 rpm** |
| 17.5T | 2170 | *unstated* | 5S → **45,570 rpm** |

Every rating lands on the same ~48,000 rpm ceiling — the can's mechanical limit, bearings and
magnet retention, expressed per-winding as a cell count.

**The rule: `kV × V_max ≤ ~48,000 rpm`.**

A 13.5T on 4S spins 47,880 rpm; the 6.5T on its endorsed 2S spins 48,300. Mechanically
identical duty. So 4S/13.5T is exactly as legitimate as the configurations printed on the box,
and the old worry that 4S pushes you outside the catalog was unfounded — **provided you pick
the winding by this rule rather than by the printed cell count.**

#### ERPM — the column this table never had

`ERPM = mechanical RPM × pole pairs`. It is how fast the *field* rotates, and it is what the
controller's loop rate has to keep up with:

```
104,160 ERPM ÷ 60          = 1,736 electrical rev/sec
30 kHz control loop ÷ 1736 = 17 updates per electrical revolution   ← desync territory
60 kHz control loop ÷ 1736 = 35 updates per electrical revolution   ← fine
```

Below roughly 20 updates per electrical revolution the angle estimate lags and the motor
desyncs. That is the whole mechanism behind every published "ERPM ceiling."

**Pole count is a free factor of two, and it is the cheapest win in this document.**

| Motor | Poles | kV | No-load rpm @ 4S | **Max ERPM** | |
|---|---|---|---|---|---|
| **QuicRun 3650 G2 13.5T** | **2** | 2850 | 47,880 | **47,880** | ✅ 2× headroom on VESC 6 |
| GoolRC 3650 3100 kV | 4 | 3100 | 52,080 | **104,160** | ❌ |
| Tekin ROC412 | 4 | 1800 | 26,640 | 53,280 | ✅ but crawler-rpm design |
| HobbyStar 3670 | 4 | 1850 | 27,380 | 54,760 | ✅ sensorless |

A 2-pole motor at 2850 kV is *easier* on the controller than a 4-pole at 1800 kV spinning
half as fast.

#### DECIDED — QuicRun 3650 G2 13.5T on 4S

2850 kV · 28.9 mΩ · **2-pole** · **sensored** · 36 × 52.8 mm · 186 g · 3.17 mm shaft.

- **~32 A sustained** (30 W dissipation) → roughly 475 W continuous, which is the course case
- **50 A peak** (72 W) → ~740 W on launch, which is the speed-run case
- **Max ERPM 47,880** — comfortably inside VESC 6 with the loop at 30 kHz
- `kV × V_max = 47,880` — right on the family's mechanical ceiling, same as a 6.5T on 2S
- Fits the 36 × 50 mm can the [packaging study](MECHANICAL.md#drive-unit-packaging--phase-4a)
  was drawn around

> ⚠ **`POLE_PAIRS = 1`, not 2.** `pin_map.h` currently has 2, which was correct for the
> 4-pole cans previously under consideration. The QuicRun 3650 is a **2-pole** motor. Get this
> wrong and every RPM and angle the observer produces is out by 2×. Count the magnets and set
> it before Phase 1.

**Fallback if the FSESC's 14 V floor turns out to be real** (see Stage 2 below): **5S with the
17.5T.** 2170 kV × 21 V = 45,570 rpm ✓, 25 A sustained for the same ~460 W, *lower* current
than 4S, and 18.5 V nominal sits mid-range on the controller with margin at both ends. The only
cost is that 5S packs are a thinner market.

#### Why the GoolRC 3650 3100 kV is out

It was the previous recommendation on the strength of a 56 A / 900 W headline. Three
independent problems, any one of which is disqualifying:

1. **104,160 ERPM.** Four poles at 3100 kV puts it past VESC 6's 100k default and near the
   150k ceiling even after switching-frequency tuning.
2. **Sensorless**, and 3100 kV gives ~36% less back-EMF per rpm than 1980 kV (`Ke = 1/kV`) —
   making the Phase 2 observer harder on the project's longest pole.
3. **The 56 A is a peak-ish claim**, not a continuous rating, which is the number that matters
   for course running. The ROAR-rated motors' conservative continuous figures — the thing that
   made them look weak — are the honest spec.

**Sensored is worth paying for.** Halls cost nothing to ignore if sensorless works, they turn
the biggest Phase 2 risk from "buy a different motor and restart the phase" into "use the
halls," and on a course they carry the low-speed corner exits where observers are weakest.

**Avoid:** 1/8-scale cans (42 mm+, ~300 g each — two would nearly double vehicle mass and
force a chassis redesign), 4-pole motors above ~2000 kV (ERPM), and crawler motors unless you
can get a max-RPM figure — they are built for a few thousand rpm, not twenty thousand.

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

## Stage 2 / Phase 2b — port to the flight power stage

Still one motor. Same algorithms, new hardware underneath them. This is where the current
ceiling goes away.

| Item | Notes | ~$ |
|---|---|---|
| **Flipsky Mini FSESC6.7 Pro** | VESC 6.6 based: **STM32F405** (M4F with FPU), 70 A continuous / 200 A peak, 14–60 V, three-shunt current sensing scaled for real current, HALL / ABI / AS5047 sensor inputs. **67 × 39 × 18.7 mm including its heatsink.** You are buying it as a *programmable power stage*, not as an ESC. | 125 |
| Standalone SWD debugger | **Probably not needed — see the Stage 1 note.** The Nucleo's on-board ST-Link detaches via the CN2 jumpers and programs an external target through CN4, so the bench rig you already own can flash the FSESC. Buy one only if you want both rigs live at once, or if CN4 turns out not to work for you. | 0–40 |
| Bullet connectors, XT60, silicone wire | 12 AWG for the phase and battery runs at these currents. | 20–30 |

**Why this board is the answer to a question that looked unanswerable.** The original plan
needed a high-current three-phase stage driven by your own MCU, and no such product is sold —
every eval board stops at 15 A. A VESC is exactly that product, just marketed as a finished
ESC. $125 buys a professionally laid-out 70 A inverter with an STM32 you are allowed to
program, and you never open a PCB tool.

**F405 is plenty.** Park/Clarke, two PI loops and SVM run in a few microseconds on a 168 MHz
M4F with an FPU — roughly 10–15% CPU at a 30 kHz loop. The G474's CORDIC would save about a
microsecond. It is a nicety, not a requirement.

### ⚠ Verify before ordering — any one of these can sink the plan

Status as of the pre-order check (2026-09-04). **Two of four are closed; the two that
remain are the ones that actually sink the plan, and both need a reply from Flipsky.**

- ✅ **SWD access on the exact board.** Closed. Flipsky's own user manual for the Mini
  FSESC6.7 Pro lists **SWD** among the board's pinouts, alongside CAN, SENSE, COMM and USB.
  Confirm physically on arrival, but this is documented, not hoped for.
- ❌ **Schematic availability for that revision — STILL OPEN, and still non-negotiable.**
  The product page offers no schematic or hardware design files, and none surfaced in
  search. VESC 6.6 upstream is open hardware, but this is a vendor derivative and
  `amps_per_count` comes straight out of *its* shunt values and CSA gain. **Email Flipsky
  and get the schematic in hand before paying.** If they will not supply one, buy hardware
  from someone who will — that is what this line has always meant.
- ❌ **Gate driver part number — STILL OPEN.** Not published anywhere findable; VESC 6
  revisions have shipped both DRV8301 and DRV8323, their SPI register maps differ, and you
  are initialising it yourself. Ask in the same email. (If the schematic arrives, this
  answers itself.)
- ✅ **The 14 V floor — closed, and it is not a floor.** Flipsky's own listing gives the
  range as **14–60 V (4S–13S; safe for 4S–12S)** — i.e. they name 4S as supported while
  quoting 14 V, so the number is a nominal-pack designation, not a cutoff. VESC 6 silicon
  runs to ~6 V, and on your own firmware the undervoltage cutoff is a constant you set
  (`VBAT_LOW_V`, currently 12.8 V). **Stay on 4S with the 13.5T.** The 5S / 17.5T fallback
  is no longer needed.

One bonus confirmation from the same listing: **150,000 ERPM maximum**, against the
QuicRun's 47,880. That is 3.1× headroom, and it retires the ERPM question entirely.

Ask Flipsky all of it in one message — schematic for the revision they will ship, gate
driver part number, and shunt resistance plus current-sense amplifier gain.

> ⚠ **Do not skip Stage 1 to get here faster.** At Stage 2 your bugs cost $125 a board plus a
> motor, and a shoot-through at 70 A on 4S is spectacular rather than educational. The whole
> value of the IHM08M1 rig is that it makes the expensive mistakes cheap.

**Avoid VESC 4.x hardware entirely** — the Mini FSESC4.20 and its relatives. The DRV8302 gate
driver accounts for roughly 80% of all VESC field failures, concentrated specifically in FOC
mode. It is the one board family that goes down to 3S, which is a large part of why 3S was
rejected.

---

## Stage 3 / Phase 3 — second channel

The second motor was bought in Phase 1–2 — this is where the sealed one gets opened. **Buy the
second controller identical to the first**; both channels share gains and the interlock watches
the difference between them.

| Item | Notes | ~$ |
|---|---|---|
| **2nd Flipsky Mini FSESC6.7 Pro** | One MCU per motor. **This is what removes Phase 3's hardest problem.** | 125 |
| **Harness materials** | Silicone-insulated stranded wire, **adhesive-lined heatshrink**, twisted pairs for the two UART runs to the ESP32, zip ties and adhesive anchor pads. See [WIRING.md](WIRING.md#strain-relief-and-harness-routing). | 20–30 |
| Header connectors | Latching or friction-lock, not bare Dupont. Dupont crimps walk off 0.1 in pins under vibration. | 10 |
| Heatsink stock + duct material | See *Cooling* below. Flat plate, not a fin tower. | 10–20 |

**Two boards instead of one dual-channel G4 is now the better architecture, not a compromise.**
The original plan put both motors on one G474 using TIM1 and TIM8. That required a second
IHM08M1 hand-remapped onto TIM8 with cut traces and flying leads, because both shields hardwire
to the same morpho pins. Two independent controllers eliminate that entirely and add:

- **Independent failure domains.** One board dying does not take both axles.
- **A full timer and ADC set per motor.** No shared-ISR deadline analysis.
- **Two genuinely independent telemetry streams** for the desync interlock, over separate
  UARTs — which is what `safety.c` wants anyway.

Physically, two boards at 67 × 39 mm sit side by side in 78 × 67 mm, comfortable on the
electronics deck above the pack in the 97 × 100 mm bay.

### Cooling — the numbers, before you over-build it

Loss budget per controller at a 50 A launch: conduction `3 × 35² × 0.002 ≈ 7 W`, plus 5–15 W
switching depending on `f_sw`. Call it **10–25 W for the ~2 seconds a launch lasts** — a few
tens of joules. Any lump of metal absorbs that; a 200 g sink is ~80–100 J/K, so the launch is a
**sub-1 K rise.**

**Course running is the case that actually sizes the cooling.** Repeated corner exits average
maybe 15–20 A per motor, so ~10–18 W per controller *continuously*. Now thermal **resistance**
matters, not mass:

- **Dense fin stacks are the wrong geometry passively.** Tight pitch chokes the buoyant flow
  that is supposed to replace the fan. Widely-spaced straight fins aligned with the airflow
  beat a workstation cooler with no fan on it.
- **A flat plate bolted to the chassis does this job.** ~15 W is not a heat-pipe problem.
- **Clamp the PCB to a chassis-mounted sink, never hang a sink off the PCB.** A finned tower
  cantilevered off a board in an RC car is a solder-joint fatigue liability.
- **The real risk is heat-soak inside a closed body**, not sink performance. That is a ducting
  problem.

**Ducting: front inlet, outlet under the spoiler.** This connects the highest-pressure point on
the car (front stagnation) to a genuine suction region — a spoiler's underside is a suction
surface and the base behind the body is separated flow. Textbook radiator ducting, and dumping
into the base can even trim base drag slightly by energising the wake.

- **Mass flow is governed by the outlet, not the inlet.** An inlet without a deliberate
  low-pressure exit moves almost nothing. **Size the outlet ≥ the inlet.**
- A converging section does raise velocity past the sink, but constriction raises resistance as
  `v²`. You only get the naive continuity velocity if flow holds up.
- An inlet costs cooling drag — **blank it for the speed-run configuration.**

> **On sustained course running the motor will hit thermal limits before the controller does.**
> 70 A of controller against a motor that peaks at 50 A means the power stage has stopped being
> the constraint. Spend the cooling effort on the motor.

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
| Drive gears, single spur pair, no bevel | **Do not cut these until μ is measured.** Ratio depends on measured grip and the motor's current rating. Same ratio for every tire profile; the gearing never changes with tires. See `accel.py --match`. **Steel pinion is not optional** — the spur runs at 17.7 m/s pitch-line. | 20–50 |
| **Gear diffs ×2, 1/18 class** | Buy on *width*, not torque — the across-the-cups dimension sets half-shaft length, which bounds Phase 8 actuator travel. Worst-case load is 0.2 N·m per output, so 1/18 is still overbuilt. Plus heavy silicone oil for the limited-slip behaviour. See [MECHANICAL.md](MECHANICAL.md#independent-uprights--settled-not-open). | 25–50 |
| **CVDs ×4, real constant-velocity — not dogbones** | A dogbone is not truly CV and produces a velocity fluctuation at angle; at 8,537 wheel rpm that is a vibration source into the bearings. A proper CV joint takes 30–40° against the 14.1° the geometry asks for. Cheapest fix in the suspension. | 20–40 |
| **Static ride-height adjusters** | Shock collars or adjustable links — sets ride height through Phases 4–7 and absorbs the 7.5 mm of axle rise on a tire swap. A few grams, no failure modes. Powered actuators are Phase 8. | 15 |
| ABS or ASA filament | **Not PLA.** See [MECHANICAL.md](MECHANICAL.md#material). | 25–35 |
| Fasteners, thread-lock, nutserts | Buy more than you think. | 30 |

---

## Phase 5 — supervisor tier

| Item | Notes | ~$ |
|---|---|---|
| **ESP32-S3 DevKit** (or classic ESP32 **committed to CAN**) | **Not "any of them" — the UART count binds.** See below. 8 MB flash, PSRAM preferred, USB-C, and a **u.FL external antenna option** if you can get it. | 10–20 |
| **10 Hz GPS module** | 10 Hz, not 1 Hz. A 1 Hz module samples once every 31 meters at target speed, which tells you nothing useful. | 20–40 |
| **6-axis IMU** (MPU-6500 / ICM-42688 / LSM6DSO class) | **Yaw rate, ≥200 Hz.** See below — this is the cheapest high-value part in the BOM. | 3–10 |
| microSD module + card | Logging. | 10 |

### ⚠ The supervisor's UART count binds — "any ESP32" is wrong

An earlier version of this line said "any of them, it's doing nothing hard." That was
written when the supervisor talked to **one** motor controller. The rearchitecture to
per-axle controllers added a second link, and the classic ESP32 has exactly **three UART
controllers**:

| UART | Wanted for | |
|---|---|---|
| UART0 | USB console + flashing | you give this up reluctantly |
| UART1 | GPS, 10 Hz | |
| UART2 | Front controller | |
| — | **Rear controller** | **nothing left** |

`supervisor/main/pin_map.h` still reflects the single-controller world — one link on UART2,
GPS on UART1, and **no IMU allocated at all**. Both gaps are Phase 5 work.

**Two clean ways out, and you should pick one before ordering:**

1. **Classic ESP32 + CAN.** Put both controllers on one TWAI bus, which is what
   [ARCHITECTURE.md](ARCHITECTURE.md#the-desync-interlock-across-two-controllers) prefers
   anyway — the F405 has bxCAN natively, one bus serves all three nodes, and it frees both
   UARTs. This works and costs nothing extra, but it makes CAN load-bearing rather than the
   preferred option.
2. **ESP32-S3.** Native USB-Serial-JTAG means the console no longer consumes UART0, so you
   get three usable UARTs *plus* USB. The UART fallback stays available as a fallback,
   which is the point of having one.

**Recommended: the S3**, because it keeps the fallback real for ~$5 more. Note the port is
not free — `sdkconfig.defaults` pins `CONFIG_IDF_TARGET="esp32"` and the pin map is
classic-ESP32 GPIO. It is a small change, but do it deliberately.

> **ESP32-C3 is out: two UARTs.** It cannot do this job in either configuration.

> **Get the external-antenna option if you can.** [Phase 7](BUILD_PHASES.md#phase-7--aluminum-chassis)
> puts this radio inside an **aluminum** chassis. A PCB trace antenna in a metal box is a
> problem you would rather buy your way out of now than debug at 60 mph.

### Why the IMU, specifically

The whole directional-stability analysis in
[MECHANICAL.md](MECHANICAL.md#-directional-stability--the-layout-as-drawn-is-divergent-at-50-mph)
rests on numbers nobody has measured: the body's `dCy/dβ`, where its centre of
pressure actually sits, and the front and rear lift coefficients. The predicted
critical speed has already moved once on better inputs (36 → ~50 mph), and it
is the difference between a wobble and an unrecoverable spin at 70 mph.

**A gyro measures the answer directly.** Yaw rate at 200 Hz gives you:

- **Damping, empirically.** Hit a seam, watch the yaw-rate decay envelope. That
  is `ζ` and `f_n` measured rather than estimated, at every rung of the ladder.
- **Divergence, before it bites.** As speed approaches critical the decay slows
  and then reverses. Plotted against speed across the Phase 6 ladder, it
  *extrapolates* — you see the critical speed coming rather than discovering it.
- **The rake and ballast experiments become real experiments.** Change one
  thing, run the same disturbance, compare decay. Without a gyro these are
  matters of opinion.
- **Yaw rate is the missing traction-control signal.** Slip is currently
  inferred from inter-axle RPM difference alone, which a differential corrupts
  (see MECHANICAL.md). Yaw rate is an independent measurement of what the
  vehicle is actually doing.

**What it does not measure: angle.** A 6-axis IMU gives angular *rate* (gyro)
and *specific force* (accelerometer — vehicle acceleration and gravity mixed
together, inseparably). There is no angle output. That is fine for everything
above, which all reads yaw *rate* directly, but it has two consequences:

- **Heading angle needs integration and drifts.** Over a 1–2 s disturbance
  event a calibrated part (~0.2 °/s residual bias) accumulates ~0.4° against a
  ~3.4° disturbance — acceptable. Over a 30 s run it is useless. Use short
  windows, and if you want true heading, fuse against **GPS course-over-ground**,
  which is already in this BOM and is absolute whenever the truck is moving.
  Do *not* reach for a 9-axis part — two brushless motors will swamp the
  magnetometer.
- **⚠ Do not close the Phase 8 ride-height/rake loop on IMU pitch.**
  Accelerometer-derived pitch is corrupted by longitudinal acceleration, and at
  a 1.1 g launch the sensor cannot distinguish that from a 48° nose-up attitude
  — the estimate is worst exactly when you most want it. **Rake and ride height
  want position feedback on the actuators**: lead screws give it nearly free
  via step counting or a cheap encoder plus an endstop reference. See
  [MECHANICAL.md](MECHANICAL.md#rake-is-aero-control-not-just-cg-control).

**The capability worth naming explicitly: sideslip.** Lateral acceleration, yaw
rate and wheel speed together give sideslip *rate* directly, `β̇ = a_y/V − r`.
`β` is the actual state variable that diverges above the critical speed — so the
IMU measures the state, not just a symptom of it. Integrated over the short
windows that matter, that is `β` itself.

**It stays on the supervisor — and that is now allowed to matter.** This paragraph
used to say the IMU was an instrument only, with log analysis its whole job, and
that any control-loop role meant moving it to the motor MCU. **That has been
amended.** The supervisor now fuses this IMU with the 10 Hz GPS into a 200 Hz
ground-speed estimate and publishes it to both controllers as an **advisory**
input — one that can only ever *reduce* a torque limit, enforced by a `min()`.

That is what makes it acceptable without relocating hardware: a wrong IMU causes
an unnecessary torque cut, which is a safe failure, and it is structurally
incapable of raising a limit. See
[ARCHITECTURE.md](ARCHITECTURE.md#the-supervisor-exception--advisory-input-that-can-only-subtract).

**Why it earns that role:** detecting *all four wheels spinning together* needs an
acceleration reference independent of the wheels, and nothing else on the vehicle
has one. Inter-axle slip cannot see it — that is the launch case, and the launch
is the whole point.

Mount it near the CG, rigidly, and record its orientation. At 8,537 wheel rpm
there is a lot of vibration to alias — pick a part with a configurable low-pass
and set it well below the sample rate.

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
| 6061-T6 plate | Flat parts only — it cracks when bent tightly. The main chassis plate is one of these. | — |
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
| **STM32G474** *(chosen — bench)* | 170 MHz M4F, TIM1/TIM8/**TIM20** — three advanced timers. 5 ADCs. CORDIC sits exactly where `foc_sincos()` wants it. Purpose-built for this, and the reason Stage 1 happens here: ST ships a reference manual and MCSDK examples to diff against. |
| **STM32F405** *(chosen — flight)* | What the FSESC6.7 carries. 168 MHz M4F **with FPU**, no CORDIC/FMAC. Adequate with room to spare: the full FOC loop is ~10–15% CPU at 30 kHz. Chosen not on merit but because it comes attached to a 70 A power stage for $125. |
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
| **X-NUCLEO-IHM08M1** *(chosen, bench only)* | **~12 A honest measurement ceiling** (amplifier saturation, not thermal — see Stage 1), 8–48 V, three-shunt. Plugs onto the Nucleo. Top of the entire X-NUCLEO family; there is nothing above it. |
| B-G431B-ESC1 | Integrated MCU + stage, compact. Dead-ends at Phase 3 (gate drivers hard-wired to its own MCU) and supply-constrained. |
| TI BOOSTXL-DRV8305 | ~45 V, ~15 A, integrated current-sense amps. Pairs naturally with a C2000 LaunchPad. |

**Flight hardware (~50 A peak, 4S) — DECIDED: VESC-class hardware running your own firmware.**

The reframe that settled this: *a VESC is not an ESC you buy instead of building, it is an
STM32 bolted to a 70 A inverter that you are allowed to program.* Every objection below was
written against using its stock firmware.

| Option | Notes |
|---|---|
| **Flipsky Mini FSESC6.7 Pro** *(chosen)* | VESC 6.6, STM32F405, 70 A cont / 200 A peak, 14–60 V, 3-shunt, sensored, **67 × 39 × 18.7 mm with heatsink**, ~$125. Open schematics. You flash your own FOC onto it. Previously rejected as "built for e-skateboards, bulk and cost" — the Mini is smaller than the IHM08M1 shield it replaces, and the cost buys a power stage you would otherwise have to design. |
| Custom DRV8353 / DRV8323 board | **The plan this replaces.** Real headroom, mounts cleanly, and the correct silicon (DRV8323RS + CSD88599Q5DC, 60 V / 40 A power blocks, SPI-programmable CSA gain). But it is a PCB project whose hardest part — shunt placement, Kelvin connections, keeping analog off switching nodes — decides whether your current readings are honest. The FSESC has already solved that layout. Revisit only if no vendor will supply a schematic. |
| MESC (davidmolony) | Open hardware *and* open firmware, 2-layer 100 × 50 mm sized for JLCPCB, ~$24 BOM, run to 100 A on F303. The best starting point **if** you end up needing a custom board — a proven high-current layout beats your first one. |
| Spintend UBOX V2.1 | Dual channel, 100 A/ch, VESC 6 based. **12 V minimum kills it on 3S and leaves no margin on 4S.** |
| Flipsky Mini Dual FSESC**4.20** | The obvious compact dual buy, and a trap: **DRV8302, ~80% of VESC field failures, concentrated in FOC mode.** Also 60,000 ERPM ceiling. Avoid. |
| Drone ESC hardware | Right size and current, cheap. Gate drivers wired to their own MCU, so the only route is the documented off-ramp: flash **AM32** and command it over bidirectional DShot behind `inverter.h`. |
| ODrive S1 / Pro | Excellent hardware, high current, own firmware ecosystem. Sized and priced for robotics. |

## Deliberately not on this list

- **A commercial ESC, used as a commercial ESC.** You are buying VESC *hardware* and running your
  own firmware on it; that is not the same thing. Stock VESC firmware, or a pair of sensored RC
  ESCs, would make a fast truck — but a PWM throttle input cannot express "give this axle 18 amps"
  and a dumb ESC cannot tell the ESP32 that the rear is spinning 15% faster than the front. **The
  traction control and the desync interlock both die with dumb ESCs**, and traction control is
  where the launch actually lives. The off-ramp still exists and is still legitimate: if the
  Phase 2 observer turns into a swamp, flash stock VESC firmware or AM32 and command it behind the
  `inverter.h` interface while you keep developing.
- **A Raspberry Pi.** The crawler has one for the camera. There is nothing here a Pi does that the
  ESP32 doesn't, and it would want more power and more space for no return.
- **A center diff.** Two motors on two axles means the electronic torque split *is* the
  differential. Whether that needs mechanical backup is a Phase 6 question, not a Phase 0 purchase.
