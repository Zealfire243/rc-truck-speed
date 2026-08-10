# hardware

CAD (`cad/`) and print-ready (`print/`) files, populated as parts are designed.

## The one rule

**Every structural part is modelled as a flat pattern plus bend lines, then
folded in CAD** — including the ones that will only ever be printed in Phase 4.

A printed part shaped like a folded sheet costs nothing extra to print, and it
turns [Phase 7](../docs/BUILD_PHASES.md#phase-7--aluminum-chassis) into a
material substitution instead of a redesign. The flat pattern you export for
the cut-and-bend shop is the same one the printed part came from.

Design rules — bend radii, reliefs, K-factor, hole-to-bend distance, tolerance
stacking — are in [../docs/MECHANICAL.md](../docs/MECHANICAL.md#designing-for-cut-and-bend).

## Naming

    <part>-<rev>-<variant>.<ext>

    chassis-plate-r1-flat.dxf     flat pattern, for cutting
    chassis-plate-r1.step         folded, for assembly
    chassis-plate-r1.3mf          printed precursor
    upright-front-r2.step

Keep the flat pattern and the folded model in sync. When they diverge you will
find out at the press brake.
