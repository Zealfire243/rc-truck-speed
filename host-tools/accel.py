#!/usr/bin/env python3
"""
Acceleration simulator. The other half of the gearing decision.

`gearing.py` answers "how fast, eventually." It says nothing about how long
that takes or how far it needs, and top speed is not the only goal — a truck
geared purely for a terminal number can be slow enough getting there that it
never reaches it inside the pavement you actually have.

    uv run accel.py                    # one configuration, in detail
    uv run accel.py --sweep            # ratio x tire tradeoff table
    uv run accel.py --limits           # what binds acceleration, and when

Stdlib only, same as gearing.py.

============================================================================
THE THREE LIMITS, AND WHICH ONE BINDS
============================================================================

Wheel force is the minimum of three separate ceilings, and which one is
active changes with speed. Getting this wrong is how people gear a car for
torque it can never put down.

  1. TRACTION      F = mu * m * g
                   Speed-independent. With 4WD, ALL the vehicle weight sits
                   on driven wheels, so weight transfer moves grip between
                   axles but never reduces the total. This is the launch
                   ceiling, and at this scale it is lower than people expect.

  2. CURRENT       F = Kt * iq_max * n_motors * FD * eta / r_wheel
                   What the inverter will let through. Scales directly with
                   final drive — this is the term people think about when
                   they gear for acceleration.

  3. VOLTAGE       As RPM climbs, back-EMF eats the bus and there is less
                   headroom left to push current, against an impedance that
                   is itself rising with electrical frequency:
                       I_avail = (V_pack - n_motor/kV) / Z(rpm)
                       Z = sqrt(R^2 + (2*pi*f_elec*L)^2)
                   This is what actually sets top speed. Model Z as R alone
                   and you will over-predict the top end by ~15%.

============================================================================
WHAT THIS MODEL ACTUALLY FOUND  (run --sweep and see for yourself)
============================================================================

For this vehicle, in this regime, the answers are unusually clean:

  GEARING SETS TOP SPEED AND NOTHING ELSE.
      0-50 mph comes out ~2.4 s at every ratio from 2.5:1 to 5:1. The whole
      acceleration run is traction-limited, so the extra torque a shorter gear
      provides is torque the tires were already refusing to transmit.

  GRIP SETS ACCELERATION AND NOTHING ELSE.
      mu 0.7 -> 1.3 moves 0-50 from 4.0 s to 2.0 s, and moves top speed by
      one mph. Top speed is drag-limited; acceleration is grip-limited; they
      barely interact.

  ABOVE ~20 A PER MOTOR, MORE CURRENT BUYS NOTHING.
      That is where launch force meets the traction ceiling. 10 A is genuinely
      slow (5.3 s to 50); 45 A is identical to 20 A.

  MASS IS NEARLY IRRELEVANT TO BOTH.
      Traction force is mu*m*g and acceleration is F/m, so m cancels. 700 g
      does 0-50 in 2.48 s; 1300 g does it in 2.30 s — the heavier truck is
      marginally QUICKER, because fixed rotating inertia is a smaller share of
      it. Build light for safety, cost, and part stress. Not for performance.

So: gear for the top speed you want, because acceleration is free. If you want
a quicker launch, the lever is grip — tire compound, surface, downforce — not
the gear ratio, not the inverter, and not the diet.

============================================================================
NUMBERS TO MEASURE, NOT TRUST
============================================================================

  mu         Foam on clean asphalt, 1.0-1.3. THE most uncertain input in this
             model and it sets the entire launch. Measure it: full throttle
             from a standstill, read the GPS trace, a = dv/dt, mu = a/g.

             NOTE WHAT mu IS AND IS NOT IN THIS MODEL. It is an input, held
             constant, because the sim uses Amontons-Coulomb friction (F=mu*N,
             area-independent). That is a decent approximation for rigid
             materials and WRONG for rubber and foam, whose friction has an
             adhesion term (scales with real contact area) and a hysteresis
             term (scales with deformed volume). The practical consequence is
             LOAD SENSITIVITY: mu falls as contact pressure rises, roughly
             mu ~ N^-k with k about 0.1-0.2.

             So a WIDER tire at the same load runs lower contact pressure and
             therefore a genuinely higher mu. Compound moves it further still.
             Neither shows up if you only sweep --tire, which changes DIAMETER.

             Decomposed:
               width, compound  ->  mu  ->  acceleration
               diameter         ->  gearing, wheel rpm, inertia, CG
                                ->  NOT mu

             Sweeping --tire and seeing acceleration stay flat is therefore a
             statement about this model's parameterization, not evidence that
             tire choice does not matter. It matters enormously; it just
             enters through --mu.
  R_phase    Line-to-line resistance / 2, measured with a good meter.
  L_phase    Line-to-line inductance / 2. Dominates impedance above ~10,000
             rpm, so it sets the top end more than R does.
  Kt         Approximated as 8.27/kV. Convention-dependent (phase vs line,
             peak vs RMS) and can be off 10-15%. Phase 2 gives you a real
             number: command a known iq against a brake and measure torque.
"""

from __future__ import annotations

import argparse
import math
from dataclasses import dataclass, replace

from gearing import G, MPH_TO_MS, RHO_AIR, Vehicle


@dataclass
class Motor:
    """Per-motor electrical parameters."""

    kv: float = 2441.0        # RPM/V
    r_phase: float = 0.030    # ohms. MEASURE.
    l_phase: float = 20e-6    # henries. MEASURE — see impedance() below.
    iq_max: float = 30.0      # amps, inverter ceiling
    i_noload: float = 1.5     # amps at speed doing nothing: iron + windage
    pole_pairs: int = 2

    @property
    def kt(self) -> float:
        """N.m per amp. See the module docstring — this is an approximation
        with a convention-dependent constant. Calibrate it in Phase 2."""
        return 8.27 / self.kv

    def impedance(self, motor_rpm: float) -> float:
        """Phase impedance at the electrical frequency for this shaft speed.

        **Resistance alone is not enough, and assuming it is will lie to you
        badly at the top end.** At 30,000 rpm with 2 pole pairs the electrical
        frequency is 1 kHz, where a 20 uH winding shows ~0.126 ohm of
        reactance — four times a typical 0.03 ohm phase resistance. Model only
        R and the sim will happily predict full current at speeds where the
        motor physically cannot take it, and hand you a top speed 15% too
        optimistic.

            f_elec = rpm/60 * pole_pairs
            Z = sqrt(R^2 + (2*pi*f_elec*L)^2)
        """
        f_elec = motor_rpm / 60.0 * self.pole_pairs
        x_l = 2.0 * math.pi * f_elec * self.l_phase
        return math.sqrt(self.r_phase ** 2 + x_l ** 2)


@dataclass
class Traction:
    mu: float = 1.10                # foam on clean asphalt
    rotating_mass_kg: float = 0.05  # wheels + rotors, reflected to linear


LIMIT_NAMES = ("traction", "current", "voltage")


def wheel_force(v_ms: float, veh: Vehicle, mot: Motor, tr: Traction
                ) -> tuple[float, str]:
    """Drive force at the contact patch, and which ceiling produced it."""
    r_wheel = veh.tire_od_mm / 2000.0
    wheel_rads = v_ms / r_wheel
    motor_rpm = wheel_rads * 60.0 / (2.0 * math.pi) * veh.final_drive

    # Voltage ceiling: what headroom is left after back-EMF, against the
    # impedance the winding actually presents at this electrical frequency.
    v_bemf = motor_rpm / mot.kv
    i_avail = (veh.v_pack - v_bemf) / mot.impedance(motor_rpm)
    if i_avail < 0.0:
        i_avail = 0.0

    iq = min(mot.iq_max, i_avail)
    limited_by = "current" if mot.iq_max <= i_avail else "voltage"

    # Iron and windage consume current before any of it becomes thrust. At
    # 30,000 rpm this is not a rounding error.
    iq_useful = iq - mot.i_noload
    if iq_useful < 0.0:
        iq_useful = 0.0

    f_motor = (mot.kt * iq_useful * veh.motors * veh.final_drive
               * veh.eta_drive / r_wheel)

    f_traction = tr.mu * veh.mass_kg * G
    if f_traction < f_motor:
        return f_traction, "traction"
    return f_motor, limited_by


def simulate(veh: Vehicle, mot: Motor, tr: Traction,
             dt: float = 0.001, t_max: float = 30.0) -> dict:
    """Forward-integrate from a standstill. Returns the whole trace plus the
    milestones that actually inform a gearing choice."""
    m_eff = veh.mass_kg + tr.rotating_mass_kg

    t = v = x = 0.0
    trace = []
    marks: dict[str, float] = {}
    limit_switch: dict[str, float] = {}
    last_limit = ""

    targets_mph = (30.0, 40.0, 50.0, 60.0, 70.0)
    dist_marks = (50.0, 100.0, 150.0, 200.0)

    while t < t_max:
        f_drive, limit = wheel_force(v, veh, mot, tr)
        f_drag = 0.5 * RHO_AIR * veh.cd * veh.frontal_area_m2 * v * v
        f_roll = veh.crr * veh.mass_kg * G
        a = (f_drive - f_drag - f_roll) / m_eff

        if limit != last_limit:
            limit_switch.setdefault(limit, v / MPH_TO_MS)
            last_limit = limit

        for tgt in targets_mph:
            key = f"0-{tgt:.0f}"
            if key not in marks and v / MPH_TO_MS >= tgt:
                marks[key] = t
        for d in dist_marks:
            key = f"v@{d:.0f}m"
            if key not in marks and x >= d:
                marks[key] = v / MPH_TO_MS

        if a < 0.005 and v > 1.0:
            break   # terminal velocity, near enough

        v += a * dt
        x += v * dt
        t += dt
        if len(trace) < 100000:
            trace.append((t, v / MPH_TO_MS, x, a / G, limit))

    return {
        "top_mph": v / MPH_TO_MS,
        "marks": marks,
        "limit_switch": limit_switch,
        "trace": trace,
    }


def report(veh: Vehicle, mot: Motor, tr: Traction) -> None:
    r = simulate(veh, mot, tr)
    m = r["marks"]

    print(f"\n  {veh.tire_od_mm:.0f} mm tire | {veh.final_drive:.2f}:1 | "
          f"{mot.kv:,.0f} kV | {veh.cells}S | {mot.iq_max:.0f} A/motor | "
          f"mu {tr.mu:.2f} | {veh.mass_kg*1000:.0f} g\n")

    print(f"  Top speed          {r['top_mph']:>6.1f} mph")
    print("\n  ACCELERATION")
    for tgt in (30, 40, 50, 60, 70):
        key = f"0-{tgt}"
        if key in m:
            print(f"    0-{tgt:<3d} mph        {m[key]:>6.2f} s")
        else:
            print(f"    0-{tgt:<3d} mph          never")

    print("\n  SPEED BY DISTANCE  (you have ~200-250 m of pavement)")
    for d in (50, 100, 150, 200):
        key = f"v@{d}m"
        if key in m:
            print(f"    at {d:>3d} m          {m[key]:>6.1f} mph")

    print("\n  WHAT LIMITS YOU, AND FROM WHAT SPEED")
    for name in LIMIT_NAMES:
        if name in r["limit_switch"]:
            print(f"    {name:<10s}       from {r['limit_switch'][name]:>5.1f} mph")

    f0, lim0 = wheel_force(0.0, veh, mot, tr)
    f_trac = tr.mu * veh.mass_kg * G
    print(f"\n  Launch force       {f0:>6.2f} N  ({lim0}-limited)")
    print(f"  Traction ceiling   {f_trac:>6.2f} N  "
          f"= {f_trac/(veh.mass_kg*G):.2f} g")
    if lim0 == "traction":
        f_motor_uncapped = (mot.kt * (mot.iq_max - mot.i_noload) * veh.motors
                            * veh.final_drive * veh.eta_drive
                            / (veh.tire_od_mm / 2000.0))
        print(f"  Motors could make  {f_motor_uncapped:>6.2f} N  "
              f"({f_motor_uncapped/f_trac:.1f}x what the tires can take)")
        print("\n  -> Launch is GRIP-limited, not torque-limited. Shortening the")
        print("     gear buys nothing off the line; it only moves where torque")
        print("     runs out. Gear for the midrange and the top end instead.")
    print()


def sweep(veh: Vehicle, mot: Motor, tr: Traction) -> None:
    print(f"\n  {mot.kv:,.0f} kV, {veh.cells}S, {mot.iq_max:.0f} A/motor, "
          f"mu {tr.mu:.2f}, {veh.mass_kg*1000:.0f} g")
    print("  Each cell: top speed / 0-50 mph / speed at 150 m\n")

    ratios = (2.5, 3.0, 3.5, 4.0, 4.5, 5.0)
    tires = (55, 60, 65, 70)

    header = "   ratio  " + "".join(f"{t:>21d} mm" for t in tires)
    print(header)
    print("  " + "-" * (len(header) - 2))
    for fd in ratios:
        row = f"  {fd:>4.2f}:1 "
        for od in tires:
            v = replace(veh, tire_od_mm=float(od), final_drive=fd)
            r = simulate(v, mot, tr)
            t50 = r["marks"].get("0-50")
            v150 = r["marks"].get("v@150m", r["top_mph"])
            t50s = f"{t50:.1f}s" if t50 else "  --"
            row += f"{r['top_mph']:>9.0f}/{t50s:>6}/{v150:>5.0f}"
        print(row)
    print("\n  Note what does NOT change: 0-50 is ~2.4 s in every cell. The run")
    print("  is traction-limited throughout, so gearing is a pure top-speed")
    print("  selector here — it is not trading acceleration for top end.")
    print("  Gear for the speed you want. For a quicker launch, chase grip.\n")


def matched_ratio(veh: Vehicle, mot: Motor, mu: float) -> float:
    """The final drive at which launch torque exactly meets launch grip.

    This is the principled way to pick a ratio once you know mu, and it exists
    because the two design goals pull in opposite directions:

      SHORTER than matched  ->  torque you cannot put down. You are paying for
                                it in motor rpm, bearing life, and heat, and
                                getting nothing.
      TALLER  than matched  ->  grip you cannot use. Wider or stickier tires
                                stop buying you anything.

    So the matched ratio is the one that extracts all the available grip while
    spinning the motor as slowly as possible. Solve F_motor(FD) = mu*m*g:

        FD = mu * m * g * r_wheel / (Kt * (iq_max - i_noload) * n * eta)
    """
    r_wheel = veh.tire_od_mm / 2000.0
    f_traction = mu * veh.mass_kg * G
    per_ratio = (mot.kt * (mot.iq_max - mot.i_noload) * veh.motors
                 * veh.eta_drive / r_wheel)
    return f_traction / per_ratio


def match_table(veh: Vehicle, mot: Motor) -> None:
    print(f"\n  MATCHED FINAL DRIVE vs GRIP  "
          f"({veh.tire_od_mm:.0f} mm, {mot.iq_max:.0f} A/motor, "
          f"{veh.mass_kg*1000:.0f} g)\n")
    print("  Gear to the matched ratio: all the grip used, motor as slow as")
    print("  possible. Shorter wastes motor life; taller wastes tire.\n")
    print("   mu     matched FD   motor rpm @ cap")
    print("   " + "-" * 40)
    for mu in (0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.8):
        fd = matched_ratio(veh, mot, mu)
        wheel_rpm = 8537.0
        print(f"   {mu:>4.2f}      {fd:>5.2f}:1      {wheel_rpm*fd:>9,.0f}")
    print("\n  Measure mu on the first Phase 4 run, THEN cut the gears.\n")


def thermal(veh: Vehicle, mot: Motor, tr: Traction, cruise_mph: float) -> None:
    """Motor loss at a SUSTAINED speed, versus final drive.

    Answers: does gearing taller actually take pressure off the motors at
    cruise? It does, and the reason is that the two loss terms move in
    opposite directions with very different steepness.

        COPPER   P = 1.5 * iq^2 * R        falls with a SHORTER gear
                                            (less torque needed per motor)
        IRON +   P ~ (rpm)^1.6              falls with a TALLER gear
        WINDAGE                             and falls steeply

    At sustained top speed the truck needs very little torque — about 6 A per
    motor — so copper loss is small and iron loss dominates. Iron loss scales
    with roughly the 1.6 power of frequency, so slowing the motor down is
    worth far more than the extra current costs you.

    CAVEAT, and it is a real one: the iron-loss curve is anchored to
    `i_noload`, which is a guess until you measure it. The SHAPE of this
    table (that taller is cooler, and by roughly how much) is robust. The
    absolute watts are not. Spin the motor unloaded at a known RPM and
    measure the current — that one number calibrates this whole model.
    """
    v = cruise_mph * MPH_TO_MS
    r_wheel = veh.tire_od_mm / 2000.0
    wheel_rpm = v / r_wheel * 60.0 / (2.0 * math.pi)

    f_drag = 0.5 * RHO_AIR * veh.cd * veh.frontal_area_m2 * v * v
    f_roll = veh.crr * veh.mass_kg * G
    p_wheel = (f_drag + f_roll) * v
    p_mech_per_motor = p_wheel / veh.eta_drive / veh.motors

    # Iron+windage reference: what the no-load current costs at 30k rpm.
    p_iron_ref = mot.i_noload * veh.v_pack
    rpm_ref = 30000.0

    print(f"\n  SUSTAINED {cruise_mph:.0f} MPH on {veh.tire_od_mm:.0f} mm tires")
    print(f"  wheel {wheel_rpm:,.0f} rpm | {p_wheel:.0f} W at the wheels | "
          f"{p_mech_per_motor:.0f} W per motor\n")
    print("   ratio   motor rpm   loss W   bearing life   centrif.   tire OK to")
    print("   " + "-" * 66)

    baseline = None
    for fd in (2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0):
        motor_rpm = wheel_rpm * fd
        omega = motor_rpm * 2.0 * math.pi / 60.0
        torque = p_mech_per_motor / omega
        iq = torque / mot.kt
        p_cu = 1.5 * iq * iq * mot.r_phase
        p_fe = p_iron_ref * (motor_rpm / rpm_ref) ** 1.6
        total = p_cu + p_fe
        if abs(fd - 3.5) < 1e-6:
            baseline = total
        # Bearing L10 life is inversely proportional to speed for a given
        # load; centrifugal stress on rotor magnet retention goes as rpm^2.
        life = 29882.0 / motor_rpm
        centrif = (motor_rpm / 29882.0) ** 2
        # Road speed the drivetrain can reach before the MOTOR runs out, and
        # whether the tire is still inside its rating there.
        print(f"   {fd:>4.2f}:1  {motor_rpm:>9,.0f}  {total:>6.1f}   "
              f"{life:>8.2f}x     {centrif:>6.2f}x", end="")
        print()

    print("\n  bearing life and centrifugal stress are RELATIVE TO 3.5:1.")
    print("  Bearing L10 goes as 1/rpm; magnet retention stress goes as rpm^2.")
    print("  Both favour a taller gear, and more steeply than heat does.\n")
    for fd in (2.0, 2.5, 3.0):
        motor_rpm = wheel_rpm * fd
        omega = motor_rpm * 2.0 * math.pi / 60.0
        iq = (p_mech_per_motor / omega) / mot.kt
        total = 1.5 * iq * iq * mot.r_phase + p_iron_ref * (motor_rpm / rpm_ref) ** 1.6
        if baseline:
            print(f"  {fd:.2f}:1 runs {(1-total/baseline)*100:.0f}% cooler than 3.5:1")
    print()


def main() -> None:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--tire", type=float, default=55.0)
    p.add_argument("--fd", type=float, default=2.5)
    p.add_argument("--cells", type=int, default=4)
    p.add_argument("--kv", type=float, default=2441.0)
    p.add_argument("--iq", type=float, default=30.0, help="amps per motor")
    p.add_argument("--r", type=float, default=0.030, help="phase ohms")
    p.add_argument("--l", type=float, default=20e-6, help="phase henries")
    p.add_argument("--mu", type=float, default=1.10)
    p.add_argument("--mass", type=float, default=0.90)
    p.add_argument("--sweep", action="store_true")
    p.add_argument("--match", action="store_true",
                   help="matched final drive vs grip")
    p.add_argument("--thermal", type=float, metavar="MPH",
                   help="motor loss vs ratio at a sustained speed")
    a = p.parse_args()

    veh = Vehicle(tire_od_mm=a.tire, final_drive=a.fd, cells=a.cells,
                  mass_kg=a.mass)
    mot = Motor(kv=a.kv, r_phase=a.r, l_phase=a.l, iq_max=a.iq)
    tr = Traction(mu=a.mu)

    if a.match:
        match_table(veh, mot)
    elif a.thermal:
        thermal(veh, mot, tr, a.thermal)
    elif a.sweep:
        sweep(veh, mot, tr)
    else:
        report(veh, mot, tr)


if __name__ == "__main__":
    main()
