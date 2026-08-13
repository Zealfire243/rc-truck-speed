#!/usr/bin/env python3
"""
Gearing, kV, and pack solver for the speed truck.

This tool exists because the pack voltage was deliberately NOT assumed. Every
downstream decision — motor kV, final drive, inverter FET ratings, wheel
choice — falls out of the numbers this prints, so run it before buying
anything. See docs/BUILD_PHASES.md Phase 0.

Three modes:

    uv run gearing.py                  # solve required kV at the target speed
    uv run gearing.py --solve fd       # solve final drive for a kV you have
    uv run gearing.py --sweep          # tire x final-drive table across 3S/4S/6S
    uv run gearing.py --swap           # top speed per tire at fixed gearing

Stdlib only, on purpose — this has to run on a phone-tethered laptop at a
parking lot with no network.

THE PHYSICS, BRIEFLY
--------------------
Top speed is set by RPM, not power. Two separate questions, and conflating
them is the classic speed-run mistake:

  1. Can it SPIN fast enough?   motor_rpm = kV * V_loaded * rpm_load_frac
                                wheel_rpm = motor_rpm / final_drive
     This is the binding constraint. If the gearing is wrong, no amount of
     power helps — the motor is already at its unloaded RPM ceiling.

  2. Can it PUSH hard enough?   P = (F_aero + F_roll) * v
     At this scale the answer is almost always yes with margin. Drag on a
     ~0.011 m^2 frontal area is small; see the printed power figure.

So: gear for RPM, then confirm the power number isn't embarrassing. If the
required kV comes out absurd (>3500), the fix is a bigger tire or a shorter
final drive, not a hotter motor.

Sanity check on the numbers this prints: the tire is usually the real limit.
A 55 mm tire at 70 mph is turning ~10,900 rpm -- which is why this
build gears for 70 mph on a 70 mm tire instead (see --swap). Rubber balloons and lets go
somewhere well below that. See docs/SAFETY.md.
"""

from __future__ import annotations

import argparse
import math
from dataclasses import dataclass

MPH_TO_MS = 0.44704
G = 9.80665
RHO_AIR = 1.225  # kg/m^3, sea level, 15 C


@dataclass
class Vehicle:
    """Everything the solver needs. Defaults describe the stretched printed
    mule from Phase 4 — update them as real parts get weighed and measured."""

    # --- Target -----------------------------------------------------------
    target_mph: float = 70.0

    # --- Rolling ----------------------------------------------------------
    tire_od_mm: float = 55.0
    final_drive: float = 2.50  # motor turns per wheel turn

    # --- Electrical -------------------------------------------------------
    cells: int = 4
    v_per_cell_loaded: float = 3.6  # under load, not the 4.2 V resting charge
    rpm_load_frac: float = 0.85  # fraction of no-load RPM reached at top speed

    # --- Mass and aero ----------------------------------------------------
    mass_kg: float = 0.90
    cd: float = 0.55  # blunt truck body with a spoiler; optimistic if anything
    frontal_area_m2: float = 0.011  # 105 mm body + wheels proud of the wells
    crr: float = 0.020  # foam tires on asphalt

    # --- Efficiency -------------------------------------------------------
    eta_drive: float = 0.85  # belt/gear final drive
    eta_motor: float = 0.80  # motor + inverter, combined
    motors: int = 2

    # --- Derived ----------------------------------------------------------
    @property
    def v_pack(self) -> float:
        return self.cells * self.v_per_cell_loaded

    @property
    def tire_circumference_m(self) -> float:
        return math.pi * self.tire_od_mm / 1000.0

    @property
    def target_ms(self) -> float:
        return self.target_mph * MPH_TO_MS


@dataclass
class Solution:
    wheel_rpm: float
    motor_rpm: float
    required_kv: float
    f_aero: float
    f_roll: float
    p_wheel: float
    p_elec: float
    i_pack: float
    i_per_motor: float
    tire_surface_ms: float


def solve(v: Vehicle) -> Solution:
    wheel_rpm = 60.0 * v.target_ms / v.tire_circumference_m
    motor_rpm = wheel_rpm * v.final_drive
    required_kv = motor_rpm / (v.v_pack * v.rpm_load_frac)

    f_aero = 0.5 * RHO_AIR * v.cd * v.frontal_area_m2 * v.target_ms**2
    f_roll = v.crr * v.mass_kg * G
    p_wheel = (f_aero + f_roll) * v.target_ms
    p_elec = p_wheel / (v.eta_drive * v.eta_motor)
    i_pack = p_elec / v.v_pack

    return Solution(
        wheel_rpm=wheel_rpm,
        motor_rpm=motor_rpm,
        required_kv=required_kv,
        f_aero=f_aero,
        f_roll=f_roll,
        p_wheel=p_wheel,
        p_elec=p_elec,
        i_pack=i_pack,
        i_per_motor=i_pack / v.motors,
        tire_surface_ms=v.target_ms,
    )


def solve_final_drive(v: Vehicle, kv: float) -> float:
    """Inverse: given a motor you already own, what final drive hits target?"""
    wheel_rpm = 60.0 * v.target_ms / v.tire_circumference_m
    motor_rpm = kv * v.v_pack * v.rpm_load_frac
    return motor_rpm / wheel_rpm


def report(v: Vehicle) -> None:
    s = solve(v)

    print(f"\n  TARGET  {v.target_mph:.0f} mph  ({v.target_ms:.1f} m/s)")
    print(f"  {v.tire_od_mm:.0f} mm tire | {v.final_drive:.2f}:1 final drive | "
          f"{v.cells}S @ {v.v_pack:.1f} V loaded | {v.mass_kg*1000:.0f} g\n")

    print("  SPEED  (the binding constraint)")
    print(f"    wheel                {s.wheel_rpm:>9,.0f} rpm")
    print(f"    motor                {s.motor_rpm:>9,.0f} rpm")
    print(f"    required motor kV    {s.required_kv:>9,.0f} kV")

    print("\n  POWER  (confirm, don't design to)")
    print(f"    aero drag            {s.f_aero:>9.2f} N")
    print(f"    rolling drag         {s.f_roll:>9.2f} N")
    print(f"    at the wheels        {s.p_wheel:>9.0f} W")
    print(f"    electrical           {s.p_elec:>9.0f} W")
    print(f"    pack current         {s.i_pack:>9.1f} A   "
          f"({s.i_per_motor:.1f} A per motor, steady state)")

    print("\n  FLAGS")
    for line in flags(v, s):
        print(f"    {line}")
    print()


def flags(v: Vehicle, s: Solution) -> list[str]:
    """The parts that bite. Ordered by how expensive the lesson is."""
    out: list[str] = []

    if s.wheel_rpm > 7000:
        out.append(
            f"TIRE: {s.wheel_rpm:,.0f} rpm on a {v.tire_od_mm:.0f} mm tire. "
            "Foam only — rubber balloons and delaminates. Balance the wheels."
        )
    if s.required_kv > 3500:
        out.append(
            f"kV: {s.required_kv:,.0f} kV is very high. High-kV motors run hot "
            "and inefficient. Prefer a larger tire or shorter final drive."
        )
    elif s.required_kv < 1200:
        out.append(
            f"kV: {s.required_kv:,.0f} kV is low — you have RPM headroom. "
            "Consider a taller final drive or a smaller tire for efficiency."
        )
    if s.motor_rpm > 50000:
        out.append(
            f"BEARINGS: {s.motor_rpm:,.0f} rpm exceeds most hobby motor "
            "ratings. Check the manufacturer's max RPM before ordering."
        )
    if s.i_per_motor > 40:
        out.append(
            f"INVERTER: {s.i_per_motor:.0f} A per motor steady state. "
            "Acceleration current is several times this — size FETs for peak."
        )

    # Peak current during acceleration is what actually sizes the hardware.
    peak = s.i_per_motor * 5
    out.append(f"SIZE FOR PEAK: budget ~{peak:.0f} A per inverter, not "
               f"{s.i_per_motor:.0f} A. Steady state never blows a FET.")

    if not out:
        out.append("nothing alarming")
    return out


def sweep(v: Vehicle) -> None:
    tires = [55, 60, 65, 70, 75, 80]
    drives = [2.5, 3.0, 3.5, 4.0, 4.5]

    print(f"\n  REQUIRED MOTOR kV TO HIT {v.target_mph:.0f} MPH")
    print("  (loaded pack voltage, "
          f"{v.rpm_load_frac:.0%} of no-load RPM at top speed)\n")

    for cells in (3, 4, 6):
        vv = Vehicle(**{**v.__dict__, "cells": cells})
        print(f"  {cells}S — {vv.v_pack:.1f} V loaded")
        header = "   tire  " + "".join(f"{d:>9.1f}:1" for d in drives)
        print(header)
        print("  " + "-" * (len(header) - 2))
        for t in tires:
            row = f"  {t:>4.0f}mm  "
            for d in drives:
                vt = Vehicle(**{**vv.__dict__, "tire_od_mm": t, "final_drive": d})
                kv = solve(vt).required_kv
                mark = " " if 1200 <= kv <= 3500 else "*"
                row += f"{kv:>9,.0f}{mark}"
            print(row)
        print()

    print("  * outside the comfortable 1200-3500 kV band\n")


def swap_table(v: Vehicle, kv: float, tire_rating_rpm: float = 8600.0) -> None:
    """Top speed per tire size at FIXED gearing and a FIXED motor.

    This is the two-configuration plan: one chassis, two identical transaxles,
    one final-drive ratio, and you change top speed by changing wheels.

    The point that makes it work — and it is not obvious — is that **wheel RPM
    is constant across every tire size.** The motor's RPM ceiling divided by
    the final drive sets wheel RPM, full stop. Tire diameter only changes how
    far the car travels per revolution.

    !! WHAT THIS TABLE WILL NOT DO IS TELL YOU THE TIRE IS SAFE.
    !!
    !! At a tall gear the drivetrain can spin the wheels well past what the
    !! tires are rated for, and the only thing standing between you and that
    !! is the firmware speed cap. An earlier version of this function printed
    !! "the tire never sees more than N rpm" as if gearing guaranteed it —
    !! true at 3.5:1, dangerously false at 2.5:1. It now prints the margin and
    !! names what is actually protecting you.
    """
    motor_rpm = kv * v.v_pack * v.rpm_load_frac
    wheel_rpm_uncapped = motor_rpm / v.final_drive

    print(f"\n  FIXED: {kv:,.0f} kV, {v.cells}S @ {v.v_pack:.1f} V, "
          f"{v.final_drive:.2f}:1 final drive")
    print(f"  Motor ceiling {motor_rpm:,.0f} rpm -> wheel "
          f"{wheel_rpm_uncapped:,.0f} rpm UNCAPPED, in every configuration")
    print(f"  Tire rating assumed {tire_rating_rpm:,.0f} rpm\n")

    print("   tire    uncapped top   safe top   cap holds    UNCAPPED margin")
    print("   " + "-" * 62)
    worst = 0.0
    for od in (55, 60, 65, 70, 75, 80):
        circ = math.pi * od / 1000.0
        mph_uncapped = (wheel_rpm_uncapped / 60.0 * circ) / MPH_TO_MS
        mph_safe = (tire_rating_rpm / 60.0 * circ) / MPH_TO_MS
        over = wheel_rpm_uncapped / tire_rating_rpm - 1.0
        worst = max(worst, over)
        print(f"   {od:>3.0f} mm    {mph_uncapped:>7.1f} mph   "
              f"{mph_safe:>5.1f} mph   {mph_safe:>5.1f} mph    "
              f"{over:>+6.0%} over rating")

    print()
    if worst > 0.02:
        print(f"  ** The drivetrain can overspeed the tires by {worst:.0%}. **")
        print("  Gearing does NOT protect the tire at this ratio — the firmware")
        print("  speed cap does, and it is therefore load-bearing safety rather")
        print("  than a convenience. The cap ceilings are tire-derived for this")
        print("  reason (safety.c TIRE_PROFILES), and selecting the profile that")
        print("  matches the fitted tires is a pre-run checklist item.")
        print("\n  Gearing shorter shrinks this margin; gearing taller grows it.")
        print("  See docs/BUILD_PHASES.md Phase 0 for the three-way tradeoff")
        print("  between wear, grip utilisation, and this margin.\n")
    else:
        print("  At this ratio the motor cannot meaningfully overspeed the")
        print("  tires — gearing is itself a backstop behind the cap.\n")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--mph", type=float, default=70.0)
    p.add_argument("--tire", type=float, default=55.0, help="tire OD, mm")
    p.add_argument("--fd", type=float, default=2.50, help="final drive ratio")
    p.add_argument("--cells", type=int, default=4)
    p.add_argument("--mass", type=float, default=0.90, help="kg")
    p.add_argument("--solve", choices=["kv", "fd"], default="kv")
    p.add_argument("--kv", type=float, default=2441.0,
                   help="motor kV, for --solve fd")
    p.add_argument("--sweep", action="store_true")
    p.add_argument("--swap", action="store_true",
                   help="top speed per tire at fixed gearing")
    a = p.parse_args()

    v = Vehicle(target_mph=a.mph, tire_od_mm=a.tire, final_drive=a.fd,
                cells=a.cells, mass_kg=a.mass)

    if a.swap:
        swap_table(v, a.kv)
    elif a.sweep:
        sweep(v)
    elif a.solve == "fd":
        fd = solve_final_drive(v, a.kv)
        print(f"\n  {a.kv:,.0f} kV on {v.cells}S ({v.v_pack:.1f} V loaded), "
              f"{v.tire_od_mm:.0f} mm tire, {v.target_mph:.0f} mph")
        print(f"  -> final drive {fd:.2f}:1\n")
        report(Vehicle(**{**v.__dict__, "final_drive": fd}))
    else:
        report(v)


if __name__ == "__main__":
    main()
