# Mechanical

## Drivetrain arrangement

**Two motors, one per axle, each mounted parallel to the axle it drives** —
motor, reduction, and axle as one drive unit, front and rear identical. Settled
early, and everything else is downstream of it: the [dual-channel FOC on one
G4](ARCHITECTURE.md#why-one-g4-instead-of-two), the electronic torque split,
the inter-axle slip signal traction control uses, and the desync interlock that
cuts both channels rather than one.

**Motor parallel to axle means a single spur reduction and no bevel gears.**
That is worth more than it sounds. A 90° drive costs an extra mesh at ~93%
efficiency and brings thrust loads, shimming, and a housing that has to locate
two perpendicular axes precisely. A motor lying alongside its axle needs one
spur pair — around 96% — with both shafts on parallel axes, which a folded
sheet-metal housing locates far more easily than a bevel set.

Concretely: motor pinion meshes a spur gear on the axle line at 2.5:1, and that
is the entire transmission. Drivetrain efficiency in `gearing.py` and
`accel.py` is set to **0.94** for that reason — if a bevel or 90° drive is ever
added, drop it to ~0.88 and re-run both.

There is no propshaft and no centre differential. The two axles are coupled
only through the ground and through `split.bias`, which means **the electronic
torque split IS the centre diff** — with no mechanical fallback if that code is
wrong.

> **Open item — live axle or independent.** Undecided, and separate from the
> spool/diff question. A live axle (one shaft, wheels rigidly attached) is the
> simplest thing the parallel-motor layout allows and puts the least between
> the spur gear and the ground. It also puts the axle and part of the drive unit
> into unsprung mass, which on a light truck crossing pavement seams at 70 mph
> is the worst place for it. Independent uprights with CVDs cost parts and
> complexity and keep unsprung mass low. Decide alongside the spool/diff call in
> [Phase 4a](BUILD_PHASES.md#4a--the-packaging-study).

> **Open item — left/right within each drive unit.** Spool or differential has
> not been decided, and it is a housing-geometry input, so it must be settled
> before the drive unit is drawn.
>
> The lean is **spool the rear** (straight-line duty, simplest, strongest) and
> decide the front on how much cornering you actually want: spooled fronts
> fight the steering badly on a 190 mm wheelbase.
>
> The argument specific to this truck: speed is derived from motor RPM and the
> desync interlock compares the two axles, so an open diff lets a single
> slipping wheel spike one axle's motor RPM and **false-trip the interlock,
> cutting both channels at speed.** Spools keep that signal unambiguous.
>
> Full decision table in
> [BUILD_PHASES.md § 4a](BUILD_PHASES.md#decision-1--spool-or-differential-in-each-drive-unit).
> **Record the decision and its reasoning here once made.**

## What size is this thing?

Worth pinning down, because **the class is where you shop** — bearings, hubs,
driveshafts, servos, and motor cans all come in class-shaped sizes.

| | This build | 1/18 | 1/16 | 1/10 |
|---|---|---|---|---|
| Wheelbase | **190 mm** | ~160 | ~198 | 255–290 |
| Track | **125 mm** | ~120 | ~150 | 190–200 |
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
  wants roughly 28–30 mm diameter cans (380 / 28xx size). A 1/10 540-can is
  overkill, heavy, and will not fit two-across in a 105 mm body.
- **Tires cross classes on purpose.** 55 mm is 1/12 pan-car; 70 mm is 1/16
  truck or 1/10 touring. The `record` config runs 1/10-touring-sized rubber on
  a 1/16 chassis, which is where the "large wheeled" look comes from and is a
  legitimate speed-run choice — bigger tires are lower wheel RPM at a given
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
| Width | 105 mm |

The chassis under it is deliberately not those numbers:

| | Body says | Chassis is | Why |
|---|---|---|---|
| Wheelbase | 165 mm | **~190 mm** | Straight-line yaw stability scales hard with wheelbase. This is the cheapest stability in the project. |
| Track | ≤105 mm | **~125 mm** | Roll stability and a wider stability triangle against yaw upset. |
| Wheels vs. wells | tucked in | slightly proud | The cost of the above. Accept it. |

At 31.3 m/s a truck travels its own wheelbase in **6 milliseconds**. Any yaw disturbance — a seam
in the pavement, a gust, a momentary traction difference between axles — has to be resisted by
geometry, because nothing else is fast enough to help. Stretching the wheelbase 15% is free
stability; every other fix costs weight or money.

### CG and the spoiler

Two rules, and they interact:

1. **CG as low as possible.** Battery flat on the floor of the chassis, motors low, nothing tall.
2. **CG forward of the aerodynamic center of pressure.** This is the arrow-fletching principle: if
   the aero center is behind the mass center, a yaw disturbance produces a restoring moment and the
   truck self-straightens. If it's ahead, the disturbance amplifies and the truck swaps ends.

The spoiler is doing real work on rule 2 — it adds rear side-area and pushes the center of pressure
back. It came with the body and it happens to be exactly what a speed truck wants. Don't remove it
for weight.

Ballast, if needed, goes **forward and low**. Adding 50 g at the front bumper is a legitimate
stability fix and costs you almost nothing in top speed — go back and re-run `gearing.py` with the
new mass and watch how little changes.

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
drivetrain can reach 11,951 wheel rpm — 39% past the tire rating — if the cap
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

Packaging cost: a wider tire at fixed track either pushes the wheels further
out (more frontal area, more wheel wake) or further in (fouling the chassis).
Decide which in the [Phase 4 packaging study](BUILD_PHASES.md#4a--the-packaging-study), not at the track. Just check the grip is usable at your gearing first —
[BUILD_PHASES.md Phase 0](BUILD_PHASES.md#phase-0--numbers-before-parts) has the
matched-ratio table.

#### Sourcing

Both sizes sit in pan-car foam territory — purpose-built for sustained high RPM,
glued to rims, actually spec'd. Shop there rather than for "1/18 tires," and get
a manufacturer RPM number in writing for both sets before ordering.

#### The rest of the tire rules

- **Foam only above ~40 mph.** Rubber tires balloon under centrifugal load — the diameter grows,
  which changes your effective gearing mid-run, and then the tire delaminates. This is the single
  most common catastrophic failure in RC speed runs, and at 10,866 rpm it is not a marginal call.
- **Balance the wheels.** At nearly 11,000 rpm a small imbalance is a large force, and it goes
  into your bearings and your chassis resonance.
- Fresh tires for Phase 8. Not the set you did the Phase 6 ladder on.

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
