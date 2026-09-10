# Mechanical

## Drivetrain arrangement

**Two motors, one per axle, each mounted perpendicular to the axle it drives,
driving through a ring-and-pinion diff** — motor, reduction, and axle as one
drive unit, front and rear identical. Everything else is downstream of it: the
[one controller per axle](ARCHITECTURE.md#why-two-controllers-instead-of-one), the electronic
torque split, the inter-axle slip signal traction control uses, and the desync interlock that
cuts both channels rather than one.

> **⚠ REVERSED — this was "motor parallel to the axle, single spur, no bevel"
> until the diff was actually sourced.** The old argument is preserved below
> because the reasoning was sound and only the weighting was wrong.

**The old case for parallel, and why it lost.** A 90° drive costs an extra mesh
and brings thrust loads, shimming, and a housing that has to locate two
perpendicular axes precisely; a motor lying alongside its axle needs one spur
pair on parallel axes, which a folded sheet-metal housing locates far more
easily. All true. Three things outweighed it:

1. **The efficiency claim was never priced.** It is worth **14 W out of 200 W**
   at the 70 mph cap — 7 W per motor against a 30 W dissipation budget. And it
   costs nothing in top speed, because top speed here is RPM-limited, not
   power-limited (see `gearing.py`'s header). Stating the efficiency delta
   without the wattage made it sound decisive when it is noise.
2. **The parallel layout has to invent a bevel drive; the 90° layout buys a
   finished one.** In an off-the-shelf ring-and-pinion diff the pinion shaft is
   the propshaft input, already on bearings inside a sealed housing with the
   shimming set at the factory. The thrust-load and alignment objection is
   answered by the part, not by the builder. What is left is coupling the motor
   to that shaft.
3. **Parallel meant fighting the entire parts market.** Ring-and-pinion is what
   RC differentials *are* — every buggy, truggy, SCT and shaft-drive touring
   car. Requiring a spur-in diff narrowed the field to touring-car parts that
   are physically too large for this truck.

Concretely: the motor drives the diff's bevel pinion directly, the ring gear is
the diff case, and **that single mesh is the entire transmission** — no spur
stage, no centre distance, no motor offset. Drivetrain efficiency in
`gearing.py` and `accel.py` is **0.88** for the two-mesh figure the old layout
assumed; a single bevel mesh is closer to **0.905**. Either way the difference
is ~14 W and does not move any decision.

There is no propshaft and no centre differential. The two axles are coupled
only through the ground and through `split.bias`, which means **the electronic
torque split IS the centre diff** — with no mechanical fallback if that code is
wrong.

### The chosen drive unit — MJX Hyper Go 1/14 metal diff

**ORDERED — Mesimsple diff + input gear set, B0DKJYNPJW, $35.88.** 45# steel
gears in a 6061 aluminum case; fits MJX Hyper Go 1/16
(16207/16208/16209/16210/H16BM/H16PL/H16DR) and 1/14
(14209/14210/14301/14302/H14BM). Sealed and oil-serviceable, so the tunable-oil
limited-slip behaviour below survives.

**Bought as a kit rather than diff-and-pinion separately, deliberately.** The
matched-set constraint below means the pinion is not a generic part; taking both
halves from one manufacturer removes the risk that a correctly-sized pinion
still meshes wrong. The kit replaced a diff-only order for that reason, not for
the price.

> **Verify on arrival — the listing does not state these:**
> 1. **Quantity.** "Front Rear ... Set" implies two diffs and two input gears,
>    but it is not spelled out. Two axles need two of each.
> 2. **The input gear is actually a BEVEL**, cone-cut to match the ring — some
>    listings in this family use "spur input gear" loosely, and the 50T motor
>    spur (part 16401) is a different part from the diff (16420).
> 3. **Input gear tooth count** — this is the number that fixes `FINAL_DRIVE`.
> 4. **Whether the input gear ships with its shaft**, which decides between the
>    two mounting approaches in
>    [Coupling the motor](#coupling-the-motor-to-the-bevel-pinion).
> 5. **Ring count on the physical gear** (confirm 30T), **across-the-cups
>    width**, and **mass on a scale**.

**Ring gear is 30T — CONFIRMED** from clear product photography (the exploded
shot showing the bevel ring alongside the case). **So the input pinion alone now
sets the ratio**, and it is a *separate part* that does not ship with the diff
(~$13; sold as a "10T bevel gear" or "umbellate tooth input gear", or bundled in
a "diff + input gear kit"):

| Input pinion | Ratio | Uncapped top (70 mm) | Motor rpm at cap | Matched μ |
|---|---|---|---|---|
| 10T | 3.00:1 | 95.3 mph | 25,614 | 2.14 |
| 11T | 2.73:1 | 104.9 mph | 23,285 | 1.95 |
| **12T** | **2.50:1** | **114.4 mph** | **21,345** | **1.79** |
| 13T | 2.31:1 | 123.9 mph | 19,703 | 1.65 |

**This is the ratio bracket the project originally wanted from swappable
pinions** — 2.31:1 to 3.00:1 without touching the diff. What it does NOT buy is
acceleration: every one of these is shorter than matched for any realistic μ, so
all four give 2.27 s to 50 mph. It is a top-speed and motor-wear selector.

> **⚠ Bevel pinions are matched sets, not interchangeable like spur pinions.**
> A ring and pinion are cut together to a shared module, pressure angle, cone
> angle and mounting distance. Any 48P spur pinion meshes any 48P spur; a
> generic bevel pinion of the right tooth count will **not** correctly mesh this
> 30T ring. Source the pinion as the mating part for *this* diff, and treat
> alternate tooth counts as available only if they exist for this diff.

The usable band for *any* candidate diff is roughly **2.0:1 to 3.5:1**. Above
4:1 the motor runs out of RPM before the truck reaches 70 mph and no gearing
trick recovers it; below 2:1 you stop using the available grip.

> **⚠ The pinion is the whole ratio decision now.** One tooth on a 10T input
> pinion is a 10% ratio change; the ring is confirmed and fixed. Balance matters
> here too — see [Coupling the motor](#coupling-the-motor-to-the-bevel-pinion).

> **⚠ Until the pinion is chosen, `FINAL_DRIVE` ships at 2.31:1** — the lowest
> ratio in the 10–13T space. Speed is
> derived as `motor_rpm / final_drive`, so **assuming a ratio higher than actual
> makes the cap fail open**: assume 3.00 when the truth is 2.80 and the cap holds
> at 75.0 mph on a tire rated for 70.5. Under-assuming engages the cap early,
> which costs a few mph and is fail-safe. This is the same failure mode as
> fitting the wrong tire profile.

### Coupling the motor to the bevel pinion

**This is now the longest pole in the drive unit.** The diff is a solved,
purchased problem; getting 25,000 rpm from a 3.17 mm motor shaft into a pinion
bored for the MJX driveshaft is not.

**Balance is the binding concern, and the numbers are not forgiving.** Imbalance
force is `F = m·e·ω²`, so it scales with the *square* of speed — at 25,000 rpm
(ω = 2,618 rad/s) a small eccentricity becomes a real cyclic load, at 417 Hz,
straight into the bearings:

| Rotating mass | Eccentricity | Force | vs the 12.18 N launch force |
|---|---|---|---|
| 5 g | 5 µm | 0.17 N | 1.4% |
| 5 g | 25 µm | 0.86 N | 7.0% |
| 10 g | 10 µm | 0.69 N | 5.6% |
| 10 g | 25 µm | 1.71 N | 14.1% |
| 20 g | 25 µm | 3.43 N | 28.1% |
| **20 g** | **50 µm** | **6.85 N** | **56.3%** |

A 20 g coupling running 50 µm out — an ordinary set-screw coupling, casually
fitted — generates over half the vehicle's entire launch force as vibration.
**The two levers are mass and eccentricity, and mass is the easier one.** Keep
everything on the motor side of the mesh as light and as concentric as possible.

**Two mounting approaches, and the trade is real:**

| | A — couple to the factory input shaft | B — bore the pinion onto the motor shaft |
|---|---|---|
| Mesh geometry | **factory, already shimmed** | motor mount must set it |
| Thrust | **taken by the diff's own bearings** | taken by the motor bearings (~4 N) |
| Rotating mass added | a coupling, 5–20 g | **almost none** |
| Eccentricity sources | **two** (coupling both ends) | **one** |
| Parts to make | motor mount + coupling | motor mount only, but precisely |

**A** preserves everything the manufacturer got right and confines the problem
to a purchasable precision part. **B** is better on the balance numbers above —
fewer joints, less mass — and ~4 N of bevel thrust is well within a 3650's
bearings. Its cost is that the motor mount alone must set bevel mesh depth, with
no shim adjustment, and bevel mesh is unforgiving of mounting distance.

Decide with the parts in hand. If A: **clamp-style rigid coupling if alignment
can be held, helical/beam if not — never a set-screw coupling**, which is
inherently eccentric (the screw is an offset mass and it drives the shaft off
centre). Torque is 0.168 N·m, so nothing here is strength-limited; specify on
runout and mass. Whatever is built, **check total indicated runout at the
coupling before the first spin-up**, and treat drive-unit vibration as a Phase 6
inspection item alongside gear temperature.

**Why this diff class rather than a 1/10 touring diff.** Two reasons, and the
first one corrected a misjudgement:

- **A differential going straight is a spinning mass, not a working gearbox.**
  The internal bevels only move relative to each other when the two wheels
  differ in speed — 0 rpm in a straight line, ~854 rpm cornering at a 10% wheel
  delta, ~4,269 rpm with one wheel spinning free (which happens at *launch*,
  when case speed is low). So running a diff above its donor platform's wheel
  RPM is a **bearing, balance and seal** question, not a gear-strength one. Ring
  rim load at the cap is 978 g, and 2,613 g uncapped; outdrive bearings see
  8,538 rpm against a 30–40k rating. Tooth load is 0.168 N·m.
- **Proven duty is still worth ranking**, derived the same way tire ratings are
  ([Sourcing](#sourcing)) — from the speed the platform is sold for:

| Platform | Sold at | Tire OD | Proven wheel rpm | vs the 8,538 needed |
|---|---|---|---|---|
| **MJX Hyper Go 1/14, 3S** | 49.7 mph | 65 mm | 6,528 | **0.76×** |
| MJX Hyper Go 1/16, 2S | 34.2 mph | 60 mm | 4,867 | 0.57× |
| Arrma Grom 1/18, stock | 20.0 mph | 60 mm | 2,846 | 0.33× |

Nothing off-the-shelf is *designed* for 8,538 wheel rpm — only 1/10 touring
comes close and those parts are too big. So the practical criteria are a metal
case, machined rather than moulded, and good outdrive bearings (a ~$10 swap on
any diff). **Diff and gear temperature after every run is a Phase 6 inspection
item.**

**Straight-cut bevels are acceptable here.** The objections to straight-cut are
dynamic tooth load and noise, both from tooth-at-a-time engagement. At 0.168 N·m
tooth load neither is a strength concern, and a small bevel pinion is *gentler*
on pitch-line velocity than the big spur the old layout forced: **10.7 m/s at
the cap and 17.5 m/s uncapped, against 17.7 and 30.9 for the 40.7 mm spur.**
What straight-cut does cost is NVH — a 4,269 Hz tooth-mesh frequency at the cap,
going into the bearings and the chassis. Set backlash carefully, use good
bearings, and do not overfill the oil.

**Alternatives considered.** Arrma GROM 1/18 (ARA311177V2), 30T/12T at 0.8 mod =
**2.50:1** — a known-exact ratio landing dead on the old assumption, and the
documented fallback if the MJX ratio comes back outside 2.0–3.5:1. Rejected on
proven duty (0.33×) and price. Losi 22S SCT (LOS232039) — a sound part, but it
sits at the end of a three-mesh layshaft box (spur → layshaft → idler → diff),
is built for high torque at low RPM, is sized for a 2 kg truck, its main gear
LOS232049 is discontinued, and it is ~$70 the pair. 1/10 touring diffs (Tamiya
TT-02/XV-02 39T/15T at 2.60:1, Traxxas 4-Tec 2.0 37T/13T at 2.85:1 spiral-cut
steel) — correct ratios and correct RPM duty, rejected purely on physical size
against a 1,080 g truck and a 97 mm bay.

There is no propshaft and no centre differential. The two axles are coupled
only through the ground and through `split.bias`, which means **the electronic
torque split IS the centre diff** — with no mechanical fallback if that code is
wrong.

### Independent uprights — settled, not open

**Every corner is independently sprung, with a half-shaft from the drive unit
to its own upright.** This is decided and it is not a Phase 4 question.

The Phase 8 goal of **per-corner ride-height control** requires it. A live axle
puts both wheels on one rigid shaft, so their heights are kinematically
coupled — you get heave and roll per axle, and camber that tilts with the axle,
never four corners you can address one at a time. Independent suspension is the
only architecture that makes the actuators mean anything.

Two things fall out of it that are worth designing around:

- **Unsprung mass stays low**, which is the right answer anyway. A live axle
  would put the shaft and part of the drive unit into unsprung mass, and on a
  light truck crossing pavement seams at 70 mph that is the worst place for it.
- **Half-shafts are now a load path**, with plunge and angle at each wheel.
  They are a known weak point at speed and want sourcing from the 1/16–1/18
  class along with the hubs.

> **⚠ Half-shaft angle bounds the usable actuator range, and the geometry was
> chosen to protect it.** Half-shafts on a chassis this size are short, so
> ride-height change turns into driveshaft angle fast. The number is not a
> guess — it falls out of the diff width and the track:
>
> | | Inner joint | Outer joint | Shaft | Angle at ±10 mm |
> |---|---|---|---|---|
> | 1/16 gear diff, 125 track | ±17 | ±50 | 33 mm | 17.6° |
> | 1/18 gear diff, 125 track | ±14 | ±50 | 36 mm | 16.1° |
> | **1/18 gear diff, 135 track** | **±14** | **±55** | **41 mm** | **14.1°** |
> | ball diff, 135 track | ±11 | ±55 | 44 mm | 13.1° |
>
> **Settled: 1/18-class gear diff on a 135 mm track — 41 mm of shaft, 14.1° at
> ±10 mm.** The diff is not sized by torque (see
> [drive unit packaging](#drive-unit-packaging--phase-4a) — the worst case in
> the project is 0.2 N·m per output, which a 1/18 diff shrugs off); it is sized
> purely to get the inner joints inboard. Widening the track 10 mm bought the
> other 5 mm per side, and threw in 4 mm of
> [CG ceiling](#the-hard-cg-ceiling) as a bonus.
>
> **Buy real CVDs, not dogbones — this is the cheap half of the fix.** The
> problem was never the angle as such; it is that a dogbone is *not* truly
> constant-velocity, so at angle it produces a second-order velocity
> fluctuation, and at 8,537 wheel rpm that is a vibration source feeding
> straight into bearings and chassis resonance. A proper CV joint tolerates
> 30–40° without it, which is well past the 14.1° the geometry now asks for.
> Costs nothing in weight or packaging.
>
> The ball diff row stays on the table as reserve. It is narrower still and it
> shrinks spur diameter too — which is the only lever on the
> [17.7 m/s gear speed](#the-gear-pair-sets-the-motor-offset) — but it trades
> the oil-tuned limited-slip for thrust-screw preload and adds a rebuild
> interval. Hold it for Phase 6, if the spur runs hot.
>
> Even so: set static ride height with the links in Phases 4–7 and keep the
> actuators for modest changes around that — see
> [ride height and the tire swap](#ride-height-and-the-tire-swap). Measure the
> vibration before trusting a large range.

### Spool vs differential — decided: differentials both ends

| Axle | Choice | Why |
|---|---|---|
| **Front** | Differential | A spooled front fights the steering directly, and on a 190 mm wheelbase that push is not small. |
| **Rear** | Differential | The truck is meant to corner, not only run in a straight line. A spooled rear scrubs and pushes in every turn and chews the tires this project is trying to preserve. |

Straight-line speed was never the only goal, so both ends get diffs. Both drive
units are now identical, which also means one housing design instead of two.

**Use gear diffs with tunable oil, not pure open diffs.** A gear diff filled
with heavy silicone oil behaves as a limited-slip: it resists rapid speed
differences (a spinning wheel) while still permitting the slow ones (cornering).
That recovers most of what a spool would have given you without any of the
steering penalty, and it turns a fixed mechanical choice into a tuning knob.

**Buy on width, not on scale class.** The diff's across-the-cups dimension is
what sets how much half-shaft you have left, and that is the binding constraint
on Phase 8 actuator travel. Torque is a non-issue: the worst case in the whole
project is **0.2 N·m per diff output** (μ 1.5, full rear bias, at launch), so
anything from 1/18 up is overbuilt. See
[the half-shaft table](#independent-uprights--settled-not-open).

> **⚠ An earlier version said "buy 1/18-class diffs, not 1/16," and elsewhere
> derived a 38 mm ring-gear floor from 1/16 sizing** — two claims that
> contradicted each other and were both the wrong frame. Scale class is a rough
> proxy for size, not a spec, and a complete diff assembly ships with internals
> sized to its own case, so there is no fit calculation to perform. Buy on the
> measured dimensions of the specific part. The chosen unit is
> [MJX Hyper Go 1/14 class](#the-chosen-drive-unit--mjx-hyper-go-114-metal-diff).
>
> **Across-the-cups width and mass are still unmeasured** — neither MJX nor the
> aftermakers publish them. Measure both on arrival and feed the width into the
> half-shaft table. Package weight suggests ~45 g per diff, ~8% of the 1,080 g
> vehicle for the pair.

Starting point, to be tuned in Phase 6:

- **Front lighter** — preserves steering, which is why it got a diff.
- **Rear heavier** — resists one-wheel spin on launch, where thrust matters.

**Diffs have a firmware consequence, and it is the one that argued against them
in the first place — now on both axles.** A differential lets a single wheel
spin up freely, so a wheel lifting over a pavement seam spikes that axle's motor
RPM with nothing actually wrong. Because speed is derived from motor RPM and the
desync interlock compares the two axles, a naive detector would read that as an
axle failure and cut both channels — **total loss of drive at 60 mph, over a
bump.**

That is handled rather than accepted. The detector scales its threshold with
speed and requires the divergence to persist for 50 ms before latching, which
rejects a wheel lift (tens of milliseconds) while still catching a stripped gear
or a dead inverter (which do not heal). The trade is explicit: up to 50 ms of
genuine desync is tolerated so that a bump cannot cut drive. See `pin_map.h`.

**And a second consequence, for reading telemetry.** With a differential, motor
RPM tracks the **fastest wheel on that axle**, not the average. So wheel-derived
speed *overstates* ground speed whenever any wheel is slipping. Two things
follow:

- The speed cap engages early during wheelspin. That is fail-safe, and it is
  also a free diagnostic: **the `cap` flag appearing well below the cap speed
  means a wheel is spinning**, not that you are near the limit.
- GPS is the only honest ground speed. This is what the Phase 5 correlation is
  for, and with diffs both ends it matters more, not less.

## What size is this thing?

Worth pinning down, because **the class is where you shop** — bearings, hubs,
driveshafts, servos, and motor cans all come in class-shaped sizes.

| | This build | 1/18 | 1/16 | 1/10 |
|---|---|---|---|---|
| Wheelbase | **190 mm** | ~160 | ~198 | 255–290 |
| Track | **135 mm** | ~120 | ~150 | 190–200 |
| Body length | **273 mm** | ~250 | ~290 | 400–450 |
| Tire OD | **55–70 mm** | 55–65 | 65–70 | 60–68 (touring) |

**It is a 1/16 by wheelbase, on a narrow 1/18-ish track, wearing a 1/18-ish
body.** It is emphatically **not 1/10** — 1/10 means a 255–290 mm wheelbase and
~190 mm of width, roughly half again this truck in every linear dimension.

The mismatch is deliberate and it is the geometry stretch: a body that scales
around 1/18 sitting over a chassis stretched to 1/16 proportions, which is
exactly why the wheels end up proud of the arches.

**What that means practically:**

- **Shop 1/16 and 1/18 running gear.** Bearings, hubs, uprights, driveshafts.
  1/10 parts are too big and too heavy, and their mass lands in the worst
  possible places for a truck this size.
- **Motors are 1/16-class, not 1/10-class.** Two motors at ~400 W peak each
  wants roughly 28–36 mm diameter cans (380 / 28xx through 3650). A 1/10
  540-can is overkill, heavy, and will not clear the 97 mm centre bay the
  [packaging study](#drive-unit-packaging--phase-4a) leaves between the axles.
- **Tires cross classes on purpose.** 55 mm is 1/10 pan-car *front*; 70 mm is
  1/10 pan-car rear or small 1/8. (Not 1/12 — those foams are 39–43 mm. See
  [Sourcing](#sourcing).) The `record` config runs 1/10-sized rubber on a 1/16
  chassis, which is where the "large wheeled" look comes from and is a
  legitimate speed-run choice — bigger tires mean lower wheel RPM at a given
  road speed.

For context on the target: RC speed records are usually set by 1/10 and 1/8
cars with long wheelbases and long stability arms. **70 mph on a 1/16-class
truck is genuinely aggressive** — at 31.3 m/s this thing covers about 115 of
its own body lengths every second.

## Geometry

The donor body, measured:

| | |
|---|---|
| Bumper to bumper | 273 mm |
| Wheel well centers, front to back | 165 mm |
| Wheel arch opening | **55 mm** |
| Width, skinniest point (cab/bed sides) | 105 mm |
| Width, widest point (fender flares) | **124.3 mm** |

> **Measure the flares, not the sides.** An earlier draft of this document
> treated 105 mm as *the* body width, and that number propagated into the
> frontal-area estimate, the "wheels proud" language, and the packaging study.
> The body is 124.3 mm across the fenders — nearly 20 mm wider than assumed —
> which is why the 135 mm track below costs far less visually than it looks
> like it should.

The chassis under it is deliberately not those numbers:

| | Body says | Chassis is | Why |
|---|---|---|---|
| Wheelbase | 165 mm | **~190 mm** | Yaw stability improves with wheelbase — but only weakly. Kept at 190 mm because it is free *here*, not because it buys much. See [what wheelbase actually buys](#what-wheelbase-actually-buys). |
| Track | ≤124.3 mm | **~135 mm** | Roll stability, a wider stability triangle against yaw upset, and ~5 mm per side of extra half-shaft length. |
| Hub vs. fender flare | under it | **5.35 mm proud per side** | The cost of the above, and it is small. |

**How proud is "proud", exactly?** Two different measurements, and the honest
one is the first:

| | Overall width | Sidewall vs. flare | Hub vs. flare |
|---|---|---|---|
| `street` 55 × 22 | 157 mm | 16.35 mm/side | **5.35 mm/side** |
| `test` 70 × 25 | 160 mm | 17.85 mm/side | **5.35 mm/side** |
| `wide` 70 × 35 | 170 mm | 22.85 mm/side | **5.35 mm/side** |

The hub column is what the track stretch actually cost — 5.35 mm per side, and
at the 125 mm track it considered earlier the hubs sat *flush* with the flares
to within half a millimetre. The sidewall column is larger but it is not the
stretch's fault: a wide tire on any track sticks out past the bodywork, and it
would do so at 125 mm too.

At 31.3 m/s a truck travels its own wheelbase in **6 milliseconds**. Any yaw disturbance — a seam
in the pavement, a gust, a momentary traction difference between axles — has to be resisted by
geometry and aero, because nothing else is fast enough to help.

#### What wheelbase actually buys

**Corrected.** Earlier drafts of this document claimed yaw stability "scales
hard with wheelbase" and called the stretch "the cheapest stability in the
project." Both overstate it. Worked through properly, wheelbase is a **weak,
linear** lever:

| | 190 mm | 185 mm | 180 mm |
|---|---|---|---|
| Yaw inertia `I_z` | 4.05e-3 | 3.83e-3 | 3.62e-3 kg·m² |
| Damping ratio `ζ` | 0.267 | 0.260 | 0.254 |
| Heading kept after a hit | 3.43° | 3.53° | 3.62° |
| **vs. 190 mm** | — | −2.5% | **−5.1%** |

The error was reading `N_r = L²C/2V` and stopping there. Yaw damping does go as
`L²` — but **yaw inertia grows with wheelbase too**, and the two very nearly
cancel. Work the disturbance response through and `I_z` drops out entirely:

```
ψ = F·Δt·V / (L·C)      heading kept after a yaw impulse
```

**Linear in 1/L.** Going 190 → 180 mm costs 5.6% more heading error and 5.1% of
damping ratio. That is not "scales hard," and it is not worth distorting the
layout for.

That −5.1% is robust even though the absolute numbers are not: sweeping spoiler
arm 15–40 mm, body `dCy/dβ` 2.5–3.5, and tire cornering stiffness 35–75 N/rad
moves `ζ` between 0.16 and 0.36, but the 180-vs-190 gap stays −5.1% in every
case. The uncertain terms cancel in the ratio.

**Keep 190 mm anyway** — it costs nothing here, and the
[packaging study](#drive-unit-packaging--phase-4a) wants every millimetre of
centre bay it can get, which argues the same direction. Just do not spend
anything else to protect it.

> **⚠ The number that should worry you is `ζ ≈ 0.27`, and it barely moves with
> wheelbase.** That is a lightly damped **2.3 Hz** yaw oscillation: ~42%
> overshoot on a disturbance and 0.77 s to settle, which at 31.3 m/s means **a
> single pavement seam rings for 24 metres of track.**
>
> The levers that actually move it, at 190 mm:
>
> | Change | Effect on `ζ` |
> |---|---|
> | Wheelbase 190 → 180 mm | −5.1% |
> | Tire compound, `C` 55 → 75 N/rad | **+36.4%** |
> | Cold/greasy tires, `C` 55 → 35 N/rad | **−36.4%** |
>
> **Tire compound is a 7× bigger lever than the entire wheelbase question**, and
> it cuts both ways: a cold first run of the day is a materially less stable
> truck than a warm one. Worth knowing before a Phase 8 attempt — see
> [SAFETY.md](SAFETY.md).
>
> Caveats: `C = 55 N/rad` is derived from the μ estimate and an assumed 6°
> peak-slip angle, and the aero terms are estimated against an unmeasured body
> height. Absolute `ζ` could be 0.16 or 0.36. The *comparisons* hold regardless.

### CG and the spoiler

Two rules, and they interact:

1. **CG as low as possible.** Battery flat on the floor of the chassis, motors low, nothing tall.
2. **CG forward of the aerodynamic center of pressure.** This is the arrow-fletching principle: if
   the aero center is behind the mass center, a yaw disturbance produces a restoring moment and the
   truck self-straightens. If it's ahead, the disturbance amplifies and the truck swaps ends.

The spoiler is doing real work on rule 2 — it adds rear side-area and pushes the center of pressure
back. It came with the body and it happens to be exactly what a speed truck wants. Don't remove it
for weight.

#### ⚠ Directional stability — the layout as drawn is divergent at ~50 mph

**This is the most important number in this document.** The truck as currently
laid out is **aerodynamically unstable in yaw above roughly 50 mph**, and the
fix is weight distribution, not the spoiler.

The measured body:

| | |
|---|---|
| Bottom of body to top of cab | 80 mm |
| Bottom of body to windshield base | 48 mm |
| Bottom of body to front of hood | 36 mm (most of the slope in the front 20 mm of a 78 mm hood) |
| Spoiler | 54 mm chord × 160 mm wide, crescent, ends wrapping the body, sitting above the cab |
| Spoiler pressure point | 27 mm from the rear of the body ≈ **20 mm behind the rear axle** |

##### The side profile is not a box, and that matters

Integrating the measured profile — 36 mm at the hood front, 48 mm at the
windshield base, 80 mm over the cab, ~50 mm bed rails:

| | Side area | Area centroid |
|---|---|---|
| Naive 273 × 80 box | 21,840 mm² | 136.5 mm from nose |
| **Measured profile** | **15,312 mm² (70%)** | **138.1 mm from nose** |

**The centroid lands essentially on the CG** (136.5 mm), because the low hood
removes area from exactly the place a destabilising moment would come from.
An earlier draft of this document put the CP at "25% of length," which is an
aircraft-fuselage rule and badly wrong here. Bluff road-vehicle bodies carry
their CP **10–20% of length ahead of the area centroid**, giving an arm of
26–53 mm rather than the 68 mm first assumed.

##### The spoiler is well placed, but arm alone cannot win

With the CG at mid-wheelbase, the spoiler acts **110 mm behind it**. At 160 mm
wide it reaches ~18 mm outboard of the flares into clean air, and sitting above
the cab keeps it out of the cab wake. It is a genuinely well-placed device.

But yaw stiffness is area × arm, and the body's side area is ~6× the spoiler's
crescent fins. Net aero is **destabilising** — which is normal. Most road cars
are the same, and they are stable because their **tires** supply the missing
yaw stiffness through forward weight bias. This truck must do the same.

##### Critical speed vs. front weight bias

Above the critical speed, a yaw disturbance grows instead of decaying. From the
linear bicycle model with the aero moment included, and with **tire load
sensitivity `C ∝ N^0.8`**:

| Front axle load | CG ahead of mid | CP 15% ahead | CP 10% ahead |
|---|---|---|---|
| **50% (as drawn)** | 0 mm | **48 mph** | **57 mph** |
| 55% | 9.5 mm | 59 mph | 78 mph |
| 57% | 13.3 mm | 64 mph | 86 mph |
| **60%** | **19.0 mm** | **72 mph** | **97 mph** |
| 65% | 28.5 mm | 82 mph | 114 mph |

> **The load-sensitivity exponent is not a detail — it is the whole mechanism.**
> If cornering stiffness were strictly proportional to load, then `a·C_f = b·C_r`
> at *every* CG position and weight bias would produce exactly zero yaw
> stiffness. Forward bias works only because real tires are load-sensitive
> (`C ∝ N^0.8`), and the entire effect lives in that exponent.

**Every item in the [CG build-up](BUILD_PHASES.md#check-4--cg-height-budget) is
symmetric about mid-wheelbase, so the layout as drawn is 50/50 — and ~50 mph is
inside the Phase 6 ladder.** That is not a disaster; it means the ladder finds
this exactly where it should. But do not be surprised by it, and do not
attribute it to a bad tune.

##### What to do about it

**Target 60% front, and treat it as a requirement rather than a trim option.**
It clears 70 mph even on the pessimistic column.

1. **Battery as far forward in the bay as it goes.** Already the plan; now it is
   load-bearing. Worth ~4 mm of CG shift on its own — real, not sufficient.
2. **Steering servo in the front overhang.** Already the plan for packaging
   reasons; it also happens to be ballast in the right place.
3. **Nose ballast to make up the rest.** ~150 g at the front bumper gets 60%.
   **It costs nothing measurable in top speed** — 1,080 g vs 1,200 g is 150 W vs
   151 W at the wheels, because the run is drag-limited and mass enters only
   through rolling resistance. Do not agonise over it.
4. **The spoiler is not the lever.** Doubling its fin area buys about **6 mph**.
   Keep it — it is free and it helps — but weight distribution is worth ten
   times more.

##### Vertical aero: why lift matters, and the front-lift trap

Downforce and lift produce **no yaw moment at all**. They matter only through
tire normal load → cornering stiffness → yaw damping and understeer. At this
scale that channel is huge:

**q·A = 8.34 N at 70 mph against 10.59 N of vehicle weight.** A `C_L` of 1.27
would lift the truck off the ground outright. A modest front lift coefficient of
0.20 removes **26% of the front axle load**.

- **The hood and windshield probably make lift, not downforce.** The raked
  windshield pushes down locally, but the hood's upper surface accelerates flow
  and generates lift, and that dominates. Net front lift is what most car-like
  bodies produce. Do not count on downforce from that shape without measuring.
- **⚠ Front lift is directionally *stabilising*, and that is a trap.** Less
  front grip means more understeer, which raises the critical speed — the model
  shows no divergence at all with `CLf = 0.20` up to front lift-off. But
  "stable" there means "cannot steer." **And this is 4WD** — the front axle is
  also a *driven* axle, so front lift costs drive traction as well as steering
  and cornering grip. Stability bought by unloading the front is not stability
  worth having.
- **The spoiler's downforce is unambiguously good**: more rear load, more `C_r`,
  more understeer, more stability, and more traction on the rear axle. That is
  a third way the spoiler earns its keep, on top of side area and arm.

##### The load balance inverts with speed — and it happens during the launch

Static 60/40 front, with `CLf = +0.20` and `CLr = −0.25` from the wing:

| mph | q·A | Front | Rear | Balance | `split.bias` to match |
|---|---|---|---|---|---|
| 0 | 0.00 N | 6.35 N | 4.24 N | **60/40** | −0.20 |
| 10 (0.44 s) | — | — | — | 59.6/40.4 | −0.19 |
| 30 (1.36 s) | 1.53 N | 6.05 N | 4.62 N | 56.7/43.3 | −0.13 |
| 50 (2.36 s) | 4.25 N | 5.50 N | 5.30 N | 50.9/49.1 | −0.02 |
| **70** | **8.34 N** | **4.69 N** | **6.32 N** | **43/57** | **+0.15** |

**The truck is front-biased at launch and rear-biased at top speed**, crossing
over around 50 mph — and because it reaches 50 mph in about 2.4 seconds, *the
entire 60/40 → 51/49 swing happens inside the launch event.* This is not a
separate high-speed regime to schedule for later.

`split_bias` in `speed_loop.h` is currently a single static tunable initialised
to `0.0f`. **A constant split is wrong at both ends.** The right shape is a
function of measured speed, and the launch controller is already the thing that
has to compute it — the same machinery that makes traction control work at
launch handles the balance shift for free, because they are the same event.

> **Uncertainty, stated plainly.** `dCy/dβ` is estimated at 2.0, CP position at
> 10–20% of length ahead of the centroid, and the lift coefficients are guesses.
> The critical speed has already moved once on better inputs (36 → ~50 mph). The
> *shape* of the result — destabilising body, insufficient spoiler, weight bias
> as the dominant lever, front lift as a trap — is robust across the whole
> plausible range. The exact number is not.
>
> **This is why the [IMU](BOM.md) is in the Phase 5 BOM.** Yaw rate measured
> directly turns the Phase 6 ladder into an experiment that settles these
> numbers, instead of a ladder that inherits them.
>
> It also supersedes the `ζ ≈ 0.27` quoted in
> [what wheelbase actually buys](#what-wheelbase-actually-buys), which assumed a
> *stabilising* aero moment. At a stable 60% front bias, `ζ ≈ 0.35` at
> `f_n ≈ 1.8 Hz`.

#### How much the spoiler is worth — and why `ζ` misreads it

Quantified at 190 mm, against a 2 N / 10 ms hit at the front axle:

| Spoiler arm | `ζ` | Peak yaw excursion | Settling |
|---|---|---|---|
| **0 mm (none)** | — | **3.43°, permanent** | **never** |
| 15 mm | 0.345 | 1.51° | 0.77 s |
| 25 mm | 0.267 | 1.28° | 0.77 s |
| 40 mm | **0.211** | **1.08°** | 0.77 s |

**More spoiler lowers `ζ` while improving everything that matters.** That looks
like a contradiction and is not — it is the reason `ζ` alone is the wrong metric
for this vehicle, so it is worth being explicit about the mechanism:

```
ζ = N_r / (2·√(N_β · I_z))
```

- **`N_r` — yaw damping, from the tires.** A yaw *rate* puts the front and rear
  tires at opposite slip angles, which resists the rotation.
- **`N_β` — yaw stiffness, from aero.** A sideslip *angle* puts the centre of
  pressure to one side of the CG, creating a restoring moment. This is the
  arrow-fletching term, and it is what the spoiler adds.

The spoiler raises `N_β` and leaves `N_r` untouched, so the *ratio* falls even
though the spring got stiffer. Exactly what happens to a car fitted with
stiffer springs and unchanged dampers: less damped relative to the new
stiffness, not worse in absolute terms.

**And the decay envelope does not move at all:**

```
ζ·ω_n = N_r / (2·I_z)        — N_β does not appear
```

which is why the settling column reads 0.77 s in every row. The spoiler changes
the *frequency* of the ringing and the *amplitude* of the excursion; it cannot
change how fast the envelope collapses. More spoiler buys a smaller peak at a
higher frequency inside an identical envelope.

**The top row is the actual argument.** With no spoiler `N_β = 0`, so there is
no restoring moment and `ζ` is undefined. The tires damp the yaw *rate* to zero
but nothing pulls the *heading* back — the truck stops rotating and then drives
away at its new 3.43° angle, permanently. That is the arrow-fletching principle
stated precisely: **tires give damping, aero gives stiffness, and only stiffness
restores a heading.**

So: don't remove the spoiler, and if anything **add rear side area rather than
trimming it.** A bigger spoiler reading `ζ = 0.21` is a better-behaved truck
than a smaller one reading `ζ = 0.35`.

#### The hard CG ceiling

Track sets a limit that has nothing to do with feel. A vehicle rolls rather
than slides when lateral acceleration exceeds `(track/2) / CG height`, so with
a 135 mm track:

| CG height | Rolls at | Verdict |
|---|---|---|
| 30 mm | 2.25 g | Slides first — good |
| 40 mm | 1.69 g | Slides first — good |
| 50 mm | 1.35 g | Slides first — good |
| **56 mm** | **1.21 g** | **Marginal at μ 1.20 (`wide`)** |
| **61 mm** | **1.11 g** | **Marginal at μ 1.10** |
| 65 mm | 1.04 g | **Rolls first** |

**Keep CG under ~61 mm**, and under ~56 mm once the `wide` tires raise μ to
1.20. Above that the truck stops sliding when it loses grip and starts tipping
instead, which at speed is the difference between a spin and a tumble.

> Going 125 → 135 mm track bought 4 mm of this ceiling (57 → 61 at μ 1.10,
> 52 → 56 at μ 1.20). That was not the reason for the change — half-shaft
> length was — but it is a real second dividend, and it is why widening is
> the one geometry lever here that improves two things at once.

**In practice this ceiling is not close.** A first-pass build-up with 70 mm
tires puts CG around **32 mm** — most of the truck's mass sits at or below axle
height and there is very little room to put anything high. Even a
Nucleo-with-shield stack, 40 mm tall and ~150 g at a 35 mm centre, only moves it
to ~33 mm. Treat 61 mm as a check to run in
[Phase 4a](BUILD_PHASES.md#check-4--cg-height-budget), not a constraint that
shapes the layout.

So neither mass nor height rules out mounting dev boards in the mule. The reason
the Nucleo and IHM08M1 are still bring-up hardware is **packaging, robustness, and a 12 A
measurement ceiling** — shield stacks, header pins, and Arduino footprints are awkward to mount
and fragile in a vehicle that crashes, and the shield cannot read the currents this truck pulls.
See [BOM.md](BOM.md#-the-ihm08m1-tops-out-near-12-a-and-no-amount-of-cooling-changes-that).

The flight controllers are a large improvement on all three counts: a **Flipsky Mini FSESC6.7 Pro
is 67 × 39 × 18.7 mm including its heatsink** — smaller than the shield it replaces — with no
header stack and screw-terminal-free phase connections.

And note the actuators buy rollover margin too: **ride height lowers CG 1:1**,
so dropping 10 mm at speed takes the roll threshold from 2.1 g to 3.1 g
(CG 32 → 22 mm on the 135 mm track).

Ballast goes **forward and low**, and on the numbers above it is not optional — see
[directional stability](#-directional-stability--the-layout-as-drawn-is-divergent-at-50-mph).
It costs almost nothing in top speed: re-run `gearing.py` with the new mass and watch how little
changes.

### Tires

**MEASURED: the donor body's wheel arches are exactly 55 mm.**

That looked like it capped tire OD, and it briefly did — chasing 70 mph on a
55 mm tire needs **10,866 wheel rpm**, which is 36% above what 1/12 pan-car
foams normally see. That was the most dangerous number in the project.

It is retired now, by gearing rather than by compromise.

#### Two tire configurations, one drivetrain

One chassis, two identical drive units, one final-drive ratio. Top speed
changes by changing wheels and nothing else:

| Profile | Tire | Rated RPM | Ceiling | μ (est.) |
|---|---|---|---|---|
| `street` | 55 × 22, cheap | 8,600 | ~50 mph | 1.05 |
| `test` | 70 × 25, cheap | 8,600 | ~63 mph | 1.10 |
| `wide` | 70 × 35, cheap | 8,600 | ~63 mph | 1.20 |
| `record` | 70 × 25, high-rated | 12,000 | ~89 mph | 1.10 |

**Tire rating is a purchasing decision, not a constant** — and the ceiling is
*computed* from it (rating × 0.90 derate), never declared. So a better tire
raises the cap automatically and a cheap one lowers it, with no code change and
no chance of the two disagreeing.

That gives you the plan directly: **burn cheap tires through the Phase 6 ladder
at a cap they can actually survive, and fit rated tires for the attempt.** The
`test` ceiling of ~63 mph is not a restriction to work around; it is the truck
refusing to destroy a tire you knowingly under-bought.

**The purchasing spec that falls out — and it is not obvious.** At 2.5:1 the
wheels turn 8,537 rpm at 70 mph, so a 10% derate needs a rating of at least
**~9,500 rpm**. An 8,600 rpm tire *cannot safely do 70 mph*. Buy the record set
to that number or above, and get it in writing.


`speed` and `wide` share a diameter, so they are identical for every speed and
cap purpose — same ceiling, same wheel RPM, same gearing math. They differ only
in grip, which is the acceleration lever. `wide` is the configuration to run
when you want a quicker launch, and it is where the Phase 4 μ measurement pays
off most.

**Wheel RPM at the cap is identical in all three.** That is the whole trick,
and it is not obvious: the motor's RPM ceiling divided by the final drive sets
wheel RPM outright. Tire diameter only changes how far the truck travels per
revolution.

So the tire never spins faster in the fast configuration than in the slow one.
Chasing top speed with a *small* tire is what drives wheel RPM into the danger
zone; chasing it with a *big* tire and taller gearing does not. 8,537 rpm is
only ~7% over typical pan-car duty, in every config.

**What holds it there is the speed cap, not the gearing.** At 2.5:1 the
drivetrain can reach 13,954 wheel rpm — 62% past the tire rating — if the cap
is not doing its job. Gearing taller for motor wear spent the mechanical
backstop that 3.5:1 provided. See
[BUILD_PHASES.md Phase 0](BUILD_PHASES.md#phase-0--numbers-before-parts).

Run `gearing.py --swap` to regenerate this table for any motor or ratio.

**Both ceilings come from the tire, not the gearing.** Each is simply the road
speed at which that tire hits its ~8,600 rpm rating — 55 mm → 55.4 mph, 70 mm →
70.5 mph. Change the final drive and these do not move, which is what frees the
ratio to be chosen on motor wear instead (see below).

#### What this costs

Two sets of wheels, and the discipline to tell the firmware which is fitted.
Tire OD feeds the speed calculation, which feeds the speed cap — so a wrong
profile makes the cap fail *open*. It is a runtime parameter bound to the cap
ceiling for that reason, and a pre-run checklist item in
[SAFETY.md](SAFETY.md#every-session-before-the-first-run).

#### Rake is aero control, not just CG control

Per-corner ride height was justified by CG management. With the aero worked out
it is doing something larger: **changing rake changes the lift distribution**,
which is a direct lever on the exact problem above.

The mechanism: nose-down rake adds front downforce and reduces front lift. That
is precisely the counter to the front-lift trap — it keeps the front axle
loaded, which preserves steering, cornering grip, *and* front drive traction on
a 4WD truck, all of which the aero is taking away at speed.

**And rake is remarkably cheap on this geometry:**

| Front | Rear | Rake | CG change | Half-shaft angle |
|---|---|---|---|---|
| −5 mm | +5 mm | **+3.01°** | **0.0 mm** | **7.0°** |
| −10 mm | +10 mm | +6.01° | 0.0 mm | 14.1° |
| −10 mm | 0 mm | +3.01° | −5.0 mm | 14.1° |

**Differential ride height costs nothing in CG** — only the *mean* height moves
the CG, so a symmetric rake trim is free on that axis. And ±5 mm buys 3° of
rake for 7° of half-shaft angle, which is **half** the 14.1° budget from
[independent uprights](#independent-uprights--settled-not-open). Three degrees
is a lot of rake for a vehicle; most cars run 1–3°.

So the actuator budget is better than the earlier framing implied. The tight
constraint is *mean* ride-height change (CG, and both shafts at angle
simultaneously); *differential* change is cheap and is where the aero authority
lives.

**Do not try to use this actively before it is measured.** The magnitude of the
lift shift per degree of rake is unknown and needs the
[IMU](BOM.md) plus Phase 6 data. The mechanism is real and the authority is
there; the gain is not yet known. Treat rake as a **static trim to find in Phase
6** first, and only consider moving it mid-run once the sign and size are
confirmed.

#### Ride height and the tire swap

Going 55 → 70 mm raises the axle 7.5 mm, and with it the CG — the one thing this
vehicle can least afford. Ride-height adjustment exists to give that back, so
both configurations sit at the same CG and the same rake.

**For the swap alone this is a static adjustment**, and shock collars or
adjustable-length links do it for a few grams with no failure modes. Powered
actuators earn their weight only for changing height *in motion* — dropping at
speed for CG, raising for launch clearance. That is a genuine capability and a
Phase 8 refinement; it is not a prerequisite for a rolling chassis, and four
lead-screw actuators is four new failure modes on a vehicle where a suspension
failure at 70 mph is a crash.

If they get built: lead screws are the right class precisely because they are
self-locking. They hold position with no power and fail in place rather than
collapsing. Verify that property on whatever is actually specified.

#### Width and compound are the grip levers — diameter is not

Worth separating, because it is easy to collapse "tire size" into one thing:

| Lever | What it buys |
|---|---|
| **Width** | Lower contact pressure → higher μ → **grip**, hence acceleration |
| **Compound** | The largest μ lever available |
| **Diameter** | Gearing, wheel rpm (tire survival), rotational inertia, CG height — **not** μ |

Rubber and foam do not obey `F = μN`. Elastomer friction carries an adhesion
term (scales with real contact area) and a hysteresis term (scales with
deformed volume), which together make μ **load-sensitive** — it falls as
contact pressure rises, roughly μ ∝ N^−0.1…−0.2. A wider tire at the same load
runs lower pressure and therefore grips harder. That is a real effect and the
reason race cars run wide rubber; it is not a myth about "more contact area."

Diameter is genuinely different: contact patch area is roughly load ÷ contact
pressure, and contact pressure is a property of the foam's stiffness — so at a
given load and width, a larger diameter changes patch *shape* (longer, narrower)
more than patch *area*.

**So go as wide as the arches and the track allow** — that is the `wide`
profile. It is the one tire choice that improves acceleration directly, and
unlike compound it costs nothing in tire life.

Be honest about the magnitude though: 22 → 35 mm is a 59% width increase and
buys perhaps 5–10% of μ from load sensitivity alone. **Compound is the bigger
lever.** The `wide` profile is specced at μ = 1.20 assuming both.

Packaging cost: **settled — no wheel offset.** A wider tire at fixed track could
push the wheels further out (more frontal area, more wheel wake) or further in
(fouling the chassis); instead both widths share one centreline and the `wide`
tire grows 5 mm each way. What that costs is a waisted front chassis plate, not
a second hub spec — see
[steering sweep](#steering-sweep--what-the-front-tires-can-hit). Just check the
grip is usable at your gearing first —
[BUILD_PHASES.md Phase 0](BUILD_PHASES.md#phase-0--numbers-before-parts) has the
matched-ratio table.

#### Sourcing

**Correction worth knowing before you order:** 1/12 pan-car foams are about
**39–43 mm**, not 55. Shopping that class gets you tires far too small. The
right classes here are:

| Need | Class to shop |
|---|---|
| ~55 mm | 1/10 pan-car / oval **front** foams (2.15–2.2 in) |
| ~70 mm | 1/10 pan-car **rear**, or small 1/8 on-road foams |

**And manufacturers largely do not publish an RPM rating.** The `rated_rpm`
field still needs a number, so derive it from the spec vendors *do* quote —
the speed the tire is sold as good for:

```
rated_rpm = proven_mph × 0.44704 ÷ (π × od_m) × 60
```

Speed-run foams (GRP, BSR and similar) are marketed by intended speed for
exactly this reason. A tire sold for 120 mph running on 1/8 gear is proving a
*surface speed*, which is what actually matters.

Because the real limit is centrifugal stress, not RPM: `a = v²/r`, so **a
smaller tire at the same road speed is under more load, not less.** At 70 mph:

| Tire OD | Centrifugal load at the tread |
|---|---|
| 55 mm | **3,630 g** |
| 70 mm | **2,850 g** |
| 80 mm | 2,500 g |

That is the physical reason the bigger tire is the safer one, and it is a 27%
reduction for going 55 → 70 mm.

Two practices from the speed-run community that are worth adopting:

- **Glue thoroughly and cure fully** — 8–12 hours, overnight, before any
  high-speed run. Early running shears an uncured bond and that is a
  delamination.
- **Reinforce the inner wall** with tape strips if you are near the limit,
  applied evenly so you do not introduce imbalance. At 2,850 g, imbalance is
  not a small force.

#### The rest of the tire rules

- **Above ~40 mph: foam, or BELTED rubber. Never unbelted rubber.** Unbelted
  rubber balloons under centrifugal load — the diameter grows, which changes
  effective gearing mid-run, and then the tire delaminates. That is the single
  most common catastrophic failure in RC speed runs and at these RPMs it is not
  a marginal call. **A nylon- or kevlar-belted tire is built to stop exactly
  that**, which is why on-road speed running above ~60 mph uses belted rubber
  routinely.

  > **⚠ CORRECTED — this rule read "Foam only above ~40 mph" and was too broad.**
  > The mechanism it describes is real but it is a property of *unbelted*
  > construction, not of rubber. Stating it as "foam only" ruled out the whole
  > belted class on a mechanism that does not apply to it.

- **DECIDED — belted rubber, not foam.** Shop **1/10 touring belted**, roughly
  63–68 mm OD and ~26 mm wide. What that swap costs, honestly:
  - **The `wide` profile is gone.** There is no 35 mm belted tire at this
    diameter. Since grip is the *only* acceleration lever on this truck
    ([gearing does nothing for 0–50](#width-and-compound-are-the-grip-levers--diameter-is-not)),
    this is the most expensive consequence of the change — more than the
    drivetrain layout costs.
  - **Measure the actual OD and set the tire profile from it.** The cap ceiling
    is *computed* from OD × rated rpm, so a 65 mm belted tire is **not** the
    70 mm `test` profile and entering it as such makes the cap fail open.
  - Rolling resistance `crr` in `accel.py` and `gearing.py` is set to 0.020 for
    **foam** on asphalt. Belted rubber differs; re-measure in Phase 4.

- **Balance the wheels.** At 8,538 rpm at the cap — and 11,600+ rpm uncapped — a
  small imbalance is a large force, and it goes into your bearings and your
  chassis resonance.
- **Glue thoroughly and cure fully**, 8–12 hours before any high-speed run.
- Fresh tires for Phase 8. Not the set you did the Phase 6 ladder on.

---

## Drive unit packaging — Phase 4a

The 2D study [Phase 4a](BUILD_PHASES.md#4a--the-packaging-study) asks for, done
at 190 mm wheelbase / 135 mm track with a 3650-class can (36 × 50 mm). It
answers *what is left in the middle*, and it moves three numbers elsewhere in
the project.

> **The motor changed but the envelope did not.** The study was drawn around a
> GoolRC-class 3650; the chosen motor is now a **QuicRun 3650 G2 13.5T at
> 36 × 52.8 mm and 186 g** — 2.8 mm longer, same diameter, same class. Nothing
> in this study moves. See [BOM.md](BOM.md#decided--quicrun-3650-g2-135t-on-4s).
>
> ⚠ **The gear pair below is *not* settled.** The 32T/75T at 2.344:1 is a worked
> example that satisfies the diff-case floor and the pitch-line limit — it is not
> a decision, and neither is the ratio, the tire diameter, or the cell count.
> Re-run the solvers against 2850 kV before cutting anything.

### The gear pair sets the motor offset — SUPERSEDED

> **⚠ This whole subsection describes the retired parallel layout and is kept
> only as the record of why it was retired.** The 90° layout has **no spur pair,
> no centre distance and no motor offset** — the motor drives the diff's bevel
> pinion directly. See
> [Drivetrain arrangement](#drivetrain-arrangement).
>
> **The 38 mm "floor" below was never a real constraint.** A gear diff ships as
> a complete assembly with its internals sized to its own case, so there is
> nothing to design against; 38 mm was a *description* of how big 1/16 diffs
> happen to be, mistakenly written as a rule. That fake floor is what forced the
> oddly large 32T pinion, the 28.3 mm offset, and the 17.7 m/s pitch-line
> problem — three constraints that dissolved with it. The real chosen ring gear
> is ~24 mm at 0.8 mod.
>
> Note the direction of the pitch-line numbers, because it is the opposite of
> what the parallel case assumed: the small bevel pinion runs **10.7 m/s at the
> cap and 17.5 m/s uncapped**, against **17.7 and 30.9** for the 40.7 mm spur
> below. Going 90° *lowered* pitch-line velocity.

The spur gear **is** the diff case, so it cannot be small — a 1/16-class gear
diff needs roughly 38 mm of ring gear. That floor forces an unusually large
pinion for a 2.34:1 single reduction, and the resulting centre distance is the
motor offset for the whole layout.

| 48-pitch, 32T pinion / 75T spur | |
|---|---|
| Ratio | 2.344:1 |
| Pinion PD / spur OD | 16.9 mm / **40.7 mm** |
| **Centre distance (= motor offset)** | **28.3 mm** |
| Motor rpm at the cap | 20,009 |
| Pitch-line velocity | **17.7 m/s** |

> **⚠ 17.7 m/s is at the top of what a plastic spur tolerates** (~15–20 m/s is
> the usual ceiling). **Steel pinion is mandatory**, and the spur wants hardened
> steel or a very good POM. Gear temperature after a run is a Phase 6 inspection
> item, alongside the printed-part checks below.
>
> Going taller makes it worse, not better: 32T/80T for 2.50:1 is 18.9 m/s.
> Going the other way, 30T/70T holds 2.33:1 at 16.6 m/s and a 26.5 mm centre
> distance — but a 37 mm spur is right at the diff-packaging floor. **If the
> spur runs hot, that is the swap**, and it costs bay length.

### Where the motor offset points — DECIDED: inboard and horizontal

The offset magnitude is fixed by the gears. Its *direction* is a real choice,
and it trades bay length against CG:

| Offset direction | Middle bay | CG | Can top |
|---|---|---|---|
| **A — inboard, horizontal** | **97 mm** | 31.9 mm | 53 mm |
| C — inboard, 45° up | 114 mm | 39.3 mm | 73 mm |
| D — straight up | 154 mm | 42.4 mm | 81 mm |
| B — outboard, horizontal | 149 mm | 31.9 mm | 53 mm |

**B is ruled out on packaging, not preference.** The can reaches 46.3 mm ahead
of the front axle and the body only gives 41.5 mm of overhang there
((273 − 190) / 2), so it comes through the bumper. It also takes the one
location the steering servo can occupy.

**C and D buy bay length by spending CG margin**, which is the one thing this
truck cannot buy back — see [the hard CG ceiling](#the-hard-cg-ceiling). D also
puts the can top at 81 mm, straight into the wheel arches.

**Status: DECIDED — A, motors inboard and horizontal.** C is the documented
fallback if the electronics genuinely cannot be decked, and it costs ~7 mm of
CG height.

### What is left

Taking A, with the front axle as the origin:

```
        41.5             97 mm bay              41.5
      |<---->|<--------------------------->|<---->|
      |  servo  [M]                   [M]  |  rx  |
  ────┴────────(O)───────────────────(O)───┴──────    ground
   bumper    front axle           rear axle   bumper
              x=0                   x=190
```

| Volume | Size | For |
|---|---|---|
| Middle bay | **97 × ~100 mm**, floor at ~10 mm, open to full height | Pack + electronics |
| Front overhang | 41.5 mm × up to 124.3 mm | **Steering servo** |
| Rear overhang | 41.5 mm × up to 124.3 mm | Receiver, antenna |
| At the axle line | ±14 mm diff cups, then 41 mm of half-shaft to ±55 mm | Drive unit |

> **The bay is 100 mm wide even though the body is 124.3 mm.** The 124.3 is at
> the fender flares, which sit over the *wheels*; between the axles the body
> necks in to its 105 mm cab/bed section, and that is the section the bay lives
> under. The wide number buys wheel clearance, not packaging volume.

> **The purchasing consequence: a standard 4S hardcase pack does not fit.**
> Those are ~105 mm long and the bay is 97 mm, in either orientation — turning
> the pack sideways does not help, because the plate is only ~100 mm across the
> bay. **It has to be a softpack**, and the binding dimension is *length*.
>
> **DECIDED — 4S 1550 mAh 100C softpack, 72 × 35 × 37 mm, XT60.** In hand.

##### ⚠ Pack current is not phase current, and an earlier version of this file confused them

An earlier version of this section sized the pack off "the 40 A launch off a
1,000 mAh pack is 40C." **There is no 40 A launch at the pack.** A motor under
FOC is a buck converter: at launch the duty cycle is near zero, so a large phase
current draws very little from the battery. The two numbers differ by an order of
magnitude at exactly the moment the old text cared about:

| Condition | I<sub>q</sub> per motor | **Pack current** | Pack power |
|---|---|---|---|
| Launch, 0 mph | 28.1 A | **3.2 A** | 46 W |
| 30 mph, still traction-limited | 28.1 A | 15.7 A | 226 W |
| 70 mph, still pulling at the cap | 28.1 A | **32.4 A** | 467 W |
| 70 mph, steady cruise against drag | ~7 A | **13.9 A** | 200 W |
| Worst case: both motors at I<sub>q</sub> 50 A at cap speed | 50 A | **~62 A** | ~890 W |

**Peak pack draw is at the top of the run, not the bottom** — the opposite of
what the old text assumed, and the reason a launch-derived C rating was the wrong
calculation. Regenerate with the pack-current block in `accel.py`'s model.

##### Capacity is sized by course running, not by the speed run

The old text justified 1,000 mAh as "nearly 5 minutes of holding 70 mph" at
cruise current. That is true and irrelevant: **cruise is not the sizing case.**
[BOM.md](BOM.md#two-vehicles-one-truck) puts course running at 3–5+ minutes, and
a course is repeated acceleration, which is where the amps are — call it 20 A
average. At 80% depth of discharge:

| | 850 mAh | 1000 mAh | 1300 mAh | **1550 mAh** | 1800 mAh |
|---|---|---|---|---|---|
| @ 14 A (cruise) | 2.9 min | 3.4 min | 4.5 min | **5.3 min** | 6.2 min |
| **@ 20 A (course)** | 2.0 min | 2.4 min | 3.1 min | **3.7 min** | 4.3 min |

**850–1000 mAh misses the 3–5 minute requirement**, which is why the
recommendation moved up. The speed run is unaffected either way — it lasts
seconds.

##### C rating: buy the internal resistance, not the label

C ratings are inflated roughly 2× industry-wide, so the label is not the
specification. What matters is whether sag holds above `VBAT_LOW_V` (12.8 V):

| Pack quality | Sag @ 32 A | Sag @ 62 A worst case |
|---|---|---|
| Cheap "50C" (8 mΩ/cell) | 1.02 V | **1.98 V → 13.2 V, too close** |
| Decent 75C (4 mΩ/cell) | 0.51 V | 0.99 V → 14.2 V ✅ |
| **Good 100C (2.5 mΩ/cell)** | **0.32 V** | **0.62 V → 14.6 V ✅** |

At 1550 mAh the real currents are only 9C cruise / 21C at the cap / 40C worst
case, so a genuine 100C pack is not being worked anywhere near its rating — which
is the point. **Prefer a brand that publishes per-cell IR.**

> **XT60 is at its ceiling in the worst case.** It is rated ~60–65 A continuous
> against the ~62 A transient above. Fine as a transient, but if Phase 6 logs
> show sustained draw up there, move to XT90.

> **Height: the packaging study was drawn around a 28 mm pack; the bought one is
> 37 mm.** That is fine for CG — the pack is the heaviest item and sits on the
> bay floor, so the extra 9 mm moves its centroid to ~28.5 mm against a
> [61 mm ceiling](#the-hard-cg-ceiling) that a first-pass build-up already
> clears by 29 mm. **What it does eat is vertical stack-up under the electronics
> deck**, which now starts ~9 mm higher. Re-check the deck clearance against the
> body in [Phase 4a](BUILD_PHASES.md#check-4--cg-height-budget).

That leaves ~25 mm of bay length beside the pack, which is **not** enough for
two controllers plus the ESP32 laid flat. **Electronics go on a deck above the
pack.** The [CG budget](#the-hard-cg-ceiling) already shows a board stack costs
only a millimetre or two of CG height, so this is effectively free.

**Two FSESC6.7 Pro boards at 67 × 39 mm sit side by side in 78 × 67 mm**, which
fits the deck comfortably — and there is no longer a separate motor-control board
to find room for, because the controllers *are* the MCUs. That is one fewer board
than the layout was drawn around.

> **Cooling wants deck space too.** Passive heatsinks on the controllers plus a
> front-inlet duct exiting under the spoiler are the plan; the outlet must be at
> least as large as the inlet. Course running, not the speed run, is what sizes
> this — see [BOM.md](BOM.md#cooling--the-numbers-before-you-over-build-it).

### Steering sweep — what the front tires can hit

**Decided: no wheel offset.** Both tire widths share one centreline at ±67.5 mm,
so the `wide` tire grows 5 mm outboard *and* 5 mm inboard — inner face 55 mm →
**50 mm** from centreline. One hub spec, one upright, and the `wide` swap is a
tire change rather than a corner rebuild.

That is affordable because the binding constraint is the **chassis plate
outline**, which is free to change, and not the drive unit, which is not.

Swept envelope of a steered front tire at ±30° lock — nothing may sit outboard
of these, measured from the vehicle centreline:

| x from front axle | `test` 70 × 25 | `wide` 70 × 35 |
|---|---|---|
| ±30 mm | 45.1 mm | 42.1 mm |
| **±22 mm** | **40.1 mm** | **34.8 mm** |
| ±15 mm | 44.1 mm | 38.4 mm |
| 0 (axle line) | 52.9 mm | 47.0 mm |

**The pinch is 22 mm fore and aft of the axle, not at the axle line.** At the
axle itself there is 47 mm of room; the tightest point is where the tire's
leading and trailing corners swing through. It is symmetric fore/aft because
steering goes both ways.

Checked against the layout above:

| | Position | Verdict |
|---|---|---|
| Front motor can | ±25 mm, x = 10.3–46.3 | **clear**, 10 mm spare |
| Chassis plate at ±37.5–40 mm | axle station | **conflicts by 3–5 mm** |

##### Plate width vs. steering lock

| Front station half-width | Max lock (`wide`) | Turning radius |
|---|---|---|
| 30 mm | 44° | — |
| 32.5 mm | 36° | — |
| **35 mm (70 mm wide)** | **30°** | **329 mm** |
| 37.5 mm | 23° | 448 mm |
| 40 mm | 18° | 585 mm |

**Waist the front axle station to 70 mm.** That gives 30° of lock and a 33 cm
turning circle — far more than a truck built for 70 mph will ever ask for, and
even 20° would give 522 mm. The middle bay stays ~100 mm because nothing sweeps
between the axles, and **the rear station needs no waist at all** — no steering
means no sweep, so it only has to clear the static 50 mm inner face and can stay
~90 mm wide.

If plate stiffness argues against narrowing the whole station, scallop just the
two corners at x = ±22 mm. That is where the pinch actually is.

> **⚠ Three things this envelope does not model. Verify in CAD before cutting.**
>
> 1. **The tire is rotated about the wheel centre here.** A real kingpin has
>    scrub radius and inclination, so the wheel centre itself translates as it
>    steers. Positive scrub pushes it outboard and makes the sweep *worse* than
>    the table.
> 2. **Ackermann means the inside wheel exceeds nominal lock.** These are
>    per-wheel numbers — apply them to the inside wheel at full lock, not to the
>    average of the pair.
> 3. **The tie rod lives in exactly this zone.** It runs from the rack to the
>    steering arm, which puts it around x = ±22 mm, and it moves with both
>    steering *and* suspension travel. With ride-height actuators it sweeps a
>    taller envelope than a fixed setup would. This is the one most likely to
>    bite.

### Two numbers this changes elsewhere

- **Half-shafts, and why the track went to 135 mm.** The motor is offset
  28.3 mm fore/aft of the axle line, so it is *not* in the half-shaft's plane
  and its 50 mm length does not constrain shaft length — only the diff's own
  width does. A 1/16 gear diff (cups at ±17) on the original 125 mm track gives
  33 mm of shaft and 17.6° at ±10 mm of ride height. Dropping to a
  **1/18-class diff** (±14) and widening to **135 mm track** gives **41 mm and
  14.1°**, which is better than the 45 mm/13° the project originally assumed.
  See [independent uprights](#independent-uprights--settled-not-open).
- **Mass goes 840 g → ~1,080 g.** The [CG budget](#the-hard-cg-ceiling) allows
  160 g for both motors; two 3650s are ~200 g **each**. CG barely moves
  (31 → 31.9 mm — they already sit at axle height), but `gearing.py` and
  `accel.py` both default to `mass_kg = 0.90` and want re-running at 1.08.
  Acceleration is traction-limited so this mostly cancels there; it is top speed
  and the rolling-resistance term that shift.

---

## Designing for cut-and-bend

You have access to a cut-and-bend shop, which makes flat blanks plus bend lines the natural
manufacturing route. **So design every structural part that way from Phase 4**, while it's still
being printed. A printed part shaped like a folded sheet costs nothing extra to print and turns
Phase 7 into a material substitution instead of a redesign.

Concretely: model the part as a flat pattern with bend lines, then fold it in CAD. Print the folded
result in ABS/ASA. When it's time for metal, you export the flat pattern you already have.

### Material

| Use | Material | Why |
|---|---|---|
| Bent brackets, uprights, motor mounts | **5052-H32** | Bends to ~1T radius without cracking. The right alloy for anything with a fold in it. |
| Flat plates, no bends | 6061-T6 | Stiffer and stronger, but cracks when bent tightly. Flat parts only. |
| Printed precursors | ABS or ASA | Survives motor-adjacent heat. **Not PLA** — it creeps under sustained load and near-motor temperatures, and it fails suddenly rather than gradually. |

Suggested thicknesses: **2.0 mm** for the main chassis plate, **1.5 mm** for brackets and uprights.
Confirm against real loads once Phase 6 tells you where things flex.

### Design rules

These are the ones that actually cause remakes:

| Rule | Value | What goes wrong |
|---|---|---|
| Minimum bend radius | ≥ 1× thickness (5052) | Tighter cracks the outside of the bend |
| Minimum flange length | ≥ 4× thickness + bend radius | Shorter and the press brake can't hold it |
| Bend relief notch | ≥ 1× thickness wide, ≥ radius + thickness deep | Without it the material tears at the bend end |
| Hole to bend line | ≥ 2.5× thickness + radius | Closer and the hole distorts into an oval |
| Bend-to-bend spacing | ≥ 6× thickness | Tooling interference on the second bend |
| K-factor (5052 air bend) | ~0.44 | Wrong K-factor means the flat pattern is the wrong length and every hole lands off-position |

### Tolerance stacking

Bend angle tolerance is about **±1°**, and it *stacks*. A part with three sequential bends can be
3° out at the far end, which across a 100 mm flange is 5 mm of position error.

Design around it rather than fighting it:

- Put mounting holes on the **same face** as the datum wherever possible, so bend variation doesn't
  move them relative to each other.
- **Slot the holes** that cross a bend, so assembly can absorb the variation.
- Never dimension a critical feature through more than one bend.
- Motor-to-motor and axle-to-axle alignment should be set by a single flat plate, not by an
  assembly of folded brackets agreeing with each other.

### Fastening into sheet

Thin sheet has almost no thread engagement. 2 mm of aluminum is roughly two threads on an M3 — not
enough for anything that sees load or gets taken apart repeatedly.

- **Nutserts / PEM inserts** for anything structural or serviceable.
- Tapped holes only where the part is thick enough to give ≥3 full threads.
- **Thread-lock everything.** Sheet metal transmits vibration differently than printed plastic, and
  Phase 7's pass criteria include "no fasteners loosened after a run" for a reason.
- Wheel nuts get thread-lock and a check after every single run. A wheel leaving at 70 mph is a
  projectile.

---

## Printed-parts phase

ABS or ASA, enclosed printer. Print orientation matters more than infill here: **layer lines are
the weak axis**, so orient parts so load runs along layers, not across them. A motor mount printed
flat will shear at the layer line; the same part printed on edge won't.

Things to watch for during Phase 6 inspections, in order of how often they show up:

1. **Hole elongation** at fastener locations — the first sign of load you didn't plan for.
2. **Creep** near motors, showing as parts that no longer sit flat. This is the PLA failure mode
   and ASA delays it rather than eliminating it.
3. **Layer separation** on parts loaded across the grain.
4. **Cracks radiating from stress concentrations** — add fillets, they're free.

Every one of these is a signal about which part goes to aluminum first. That's what Phase 6 is
actually for.
