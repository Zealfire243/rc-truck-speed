# Mechanical

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

**MEASURED: the donor body's wheel arches are exactly 55 mm.** That sets tire OD,
and it makes the tire the binding constraint of the entire project.

At 55 mm and 70 mph the wheel turns **10,866 rpm**, and *no choice of gearing
changes that number* — it falls out of tire diameter and target speed alone.
Gearing moves the load between motor kV and motor RPM; it cannot help the tire.

#### What the arch measurement cost

`gearing.py` at 4S, everything else held constant:

| Tire OD | Wheel RPM | Motor RPM @ 2.75:1 | Required kV |
|---|---|---|---|
| **55 mm (fits stock)** | **10,866** | **29,882** | **2,441** |
| 65 mm (needs trimming) | 9,195 | 25,286 | 2,065 |
| 70 mm (needs trimming) | 8,538 | 23,480 | 1,918 |

Bigger tires win on every axis at once — lower wheel RPM so the tire survives,
lower motor RPM so the bearings survive, lower kV so the motor runs cooler. The
only cost is 2.5 mm of axle rise per 5 mm of diameter, and CG height is the one
thing this vehicle can least afford.

**The arch is a soft constraint.** Trimming wheel arches is routine on speed
builds, and it is the correct fix if Phase 6 starts destroying tires — going to
65 mm cuts wheel RPM 15%. Do not lower the speed target before spending this
lever.

#### Sourcing at 55 mm

55 mm is 1/12-scale pan-car foam territory: purpose-built for sustained high
RPM, glued to rims, actually spec'd. Shop there rather than for "1/18 tires."

But note the duty gap — 1/12 pan cars run 40–50 mph, so their tires normally see
7,000–8,000 rpm. You would be running **35–50% above their usual duty.** Verify
the manufacturer's RPM rating before buying and treat this as the live risk item
straight through Phase 6.

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
