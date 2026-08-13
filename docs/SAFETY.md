# Safety

A 900 g truck at 70 mph carries roughly the kinetic energy of a baseball thrown by a professional
pitcher — about 440 joules — and unlike a baseball it does not stop when it hits something. It is
also travelling faster than you can react: at 31 m/s it covers **6 meters** during a 200 ms human
reaction. You are not going to steer out of anything.

Read this before Phase 6, and again the morning of any Phase 8 attempt.

---

## Bench work

**Never spin a motor with a wheel mounted, unclamped, on a bench.** This is the most likely way to
get hurt on this project, and it happens during "quick tests."

- Motor clamped, wheel off, for all of Phase 1 through 3.
- Current-limited bench supply. Start at 2 A and raise it deliberately.
- **Verify deadtime on a scope before connecting the gate driver.** Shoot-through destroys a
  half-bridge in microseconds and can fail short, which puts full pack voltage across things that
  aren't rated for it.
- Eye protection when a motor is spinning. Rotors have come apart.
- Discharged bulk capacitors before touching the inverter. They hold pack voltage after power-off.

## Hardware interlocks that must exist before Phase 4

Not software. Hardware.

- [ ] **Overcurrent comparator tied to the TIM1/TIM8 break input.** This kills the gates whether or
      not the ISR is still running. Software overcurrent protection has to survive the software
      being wrong; this doesn't.
- [ ] **Deadtime configured in the timer**, not in software.
- [ ] **A physical means of cutting power** that does not involve the transmitter or the firmware.
      An accessible pack connector counts.
- [ ] **Receiver failsafe configured and tested at the receiver**, so it holds even if the G4 is
      confused.

## Every session, before the first run

- [ ] **RC failsafe test: turn the transmitter off, confirm the truck stops.** Every session. Not
      "I tested it last week."
- [ ] Wheel nuts checked. Thread-locked, and physically checked.
- [ ] Tires inspected — no cuts, no delamination starting, no chunking.
- [ ] Battery inspected — no puffing, no damage, connectors sound.
- [ ] Speed cap set to the phase you're actually on, verified in telemetry, not assumed.
- [ ] **Tire profile matches the tires physically on the truck.** Check it in the `hello` frame,
      by eye, against the wheels in front of you. The firmware cannot detect what is bolted on —
      if it believes 55 mm while 70 mm are fitted, it computes 55 mph while you are doing 70 and
      **the speed cap fails open.** This is the one safety property in the whole system that
      depends entirely on a human being honest, so treat it like the failsafe test: every session,
      out loud, no assumptions.
- [ ] Nobody downrange. Nobody's dog downrange.

## Run site

Budget **200–250 m of smooth, closed pavement.** The arithmetic:

| | Distance |
|---|---|
| Accelerate to 31 m/s (~0.5 g average, aero-limited at the top) | ~100 m |
| Hold speed long enough for 10 Hz GPS to register it | ~30 m |
| Stop — assume you may have to coast, drag alone is only ~0.4 g | ~115 m |
| **Total** | **~245 m** |

Also required:

- **Closed.** No traffic, no pedestrians, no pets, no children. A truck that leaves the pavement at
  70 mph travels a long way.
- **Smooth.** A seam or pothole that's nothing at 20 mph launches the truck at 70.
- **Straight**, with runoff on both sides that isn't a curb, a car, or a person.
- **A spotter.** Two-person rule for anything above Phase 6. One person watches downrange while the
  other drives, because the driver is looking at a truck 150 m away and cannot see what's ahead of
  it.

## LiPo handling

- Charge in a fireproof bag or ammo can, on a non-flammable surface, never unattended.
- Never charge a pack that's puffed, damaged, or was crashed hard. A crashed pack can go into
  thermal runaway hours later — quarantine it outside and watch it.
- Never charge a warm pack straight off a run.
- Balance charge. Store at storage voltage.
- Have a plan for a fire that isn't water. Sand, or a Class D extinguisher, or a place to push it
  where it can burn out safely.

## Failure modes worth rehearsing

Know what you'll do before it happens:

| It does this | You do this |
|---|---|
| Veers off line at speed | Throttle to zero. Do not try to steer it back — you're 200 ms behind it. |
| Loses a wheel | Throttle to zero, everyone behind cover. The wheel keeps going. |
| Tire delaminates | Throttle to zero. It'll be audible and visible before it fully lets go. |
| Comes back smoking | Do not pick it up. Disconnect the pack if you can reach the connector safely, then leave it somewhere it can burn. |
| Won't respond to the transmitter | Failsafe should already have handled it. If it hasn't, that's a Phase 4 gate that didn't hold — stop the session and fix it. |

## Field weakening — the Phase 8 hazard

Field weakening (negative `foc.id_ref`) lets the motor spin past the RPM that
kV × pack voltage would normally allow, by using d-axis current to oppose the
rotor's magnet flux. It trades torque you don't need at top speed for RPM you
do, and it's a legitimate tool for the last few mph.

**It is also the one thing in this firmware that can destroy the inverter and
the pack in a single event.**

While field weakening at high RPM, the motor's back-EMF is being actively
suppressed by the current loop. If that suppression stops — inverter fault,
gate driver dropout, break input firing, current loop losing the rotor angle,
a crash disconnecting a phase — the back-EMF is suddenly unopposed, and at
those speeds it exceeds pack voltage. The motor becomes an uncontrolled
generator dumping current backwards through the FET body diodes into a pack
that has nowhere to put it.

Rules, if you use it at all:

- **Not before Phase 8.** Everything else must be proven first.
- Never field weaken beyond the point where back-EMF at that RPM exceeds the
  pack's safe absorption. Compute that number before the first attempt.
- Ramp `id_ref` in, don't step it.
- The fault response for a field-weakening state cannot be "gates off" —
  that is precisely the event described above. It has to be a controlled
  unwind of `id_ref` first, and only then a cut.
- Treat any `oc_trip` or `obs_lost` while field weakening as a hardware
  inspection, not a reset-and-retry.

If the gearing math closes without it, don't use it.

## Things this project deliberately does that look wrong

**Coast on RC loss, don't brake.** A hobby ESC brakes on failsafe. This one coasts, because
braking hard at 70 mph with no steering authority puts the truck sideways, and a truck sideways at
70 mph is a tumbling object with an unpredictable direction. Coasting looks worse and is much
safer. See [ARCHITECTURE.md](ARCHITECTURE.md#failure-modes-and-what-happens).

**Cut both axles on a desync, not the failing one.** Half the drivetrain pushing while the other
half doesn't, on a short wheelbase at speed, is a spin. Losing all torque is the good outcome.
