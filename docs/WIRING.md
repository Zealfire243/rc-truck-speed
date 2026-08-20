# Wiring

> **Status: proposed, not yet built.** Nothing here has been wired, because nothing has been
> bought.
>
> **This document covers two different machines.** Stage 1 is a Nucleo-G474RE + X-NUCLEO-IHM08M1
> bench rig; Stage 3 is two Flipsky FSESC6.7 Pro controllers in the truck. They share almost no
> wiring. See [BOM.md](BOM.md#the-three-stage-hardware-plan).

---

## Stage 1 — bench rig (Nucleo-G474RE + IHM08M1)

**One motor, one power stage, no vehicle.** The pin map below is largely *dictated* by the
IHM08M1 shield rather than chosen — the shield hardwires its gate drive and current sense to
specific morpho pins.

> ⚠ **Check every pin against UM1996's morpho tables (CN7 and CN10) and the G474 datasheet in
> CubeMX before wiring.** Alternate-function conflicts on the G4 are dense — several of these
> pins have three plausible uses — and the shield's assignments win, because they are copper.

### TIM1 — the only axle at Stage 1

| Function | Pin | Notes |
|---|---|---|
| PWM CH1 | PA8 | IHM08M1 UH, CN10-23 |
| PWM CH2 | PA9 | IHM08M1 VH, CN10-21 |
| PWM CH3 | PA10 | IHM08M1 WH, CN10-33 |
| PWM CH1N | PB13 | UL, CN10-15 — complementary |
| PWM CH2N | PB14 | VL, CN7-34 — complementary |
| PWM CH3N | PB15 | WL, CN10-24 — complementary |
| **Break in (BKIN)** | PB12 | **overcurrent → gates off in hardware.** Shield BKIN on CN10-13/14 |

> **There is no TIM8 axle.** An earlier version of this document mapped a rear axle onto TIM8 on
> the same chip. That plan is gone — the second motor now has its own controller. Stage 1 is
> deliberately one motor, and Phase 3's second channel is a second board, not a second timer.

### Analog

| Function | Pin | ADC | Shield pin |
|---|---|---|---|
| Phase A current | PA0 | ADC1_IN1 | Curr_fdbk_PhA, CN7-28 |
| Phase B current | PA1 | ADC1_IN2 | Curr_fdbk_PhB, CN7-36 |
| Phase C current | PA2 | ADC1_IN3 | Curr_fdbk_PhC, CN7-38 |
| Pack voltage sense | PA3 | ADC1_IN4 | VBUS_sensing, CN7-30 |
| Inverter temp | PC0 | ADC1_IN6 | Temperature feedback, CN7-35 |

**All three current channels are sampled by injected conversion triggered off the timer update
event**, so every phase is measured at the same instant in the PWM cycle. This is not a detail —
it's the difference between Phase 1 passing and Phase 1 producing plausible-looking garbage. See
`current_sense.h`.

> ⚠ **The IHM08M1's honest measurement ceiling is ~12 A**, set by amplifier saturation, not heat.
> `IQ_LIMIT_A` stays at 10 A for this entire stage. Set the three-shunt jumpers **J5/J6 to 3-Sh**
> and note that **JP2 modifies the op-amp gain** — measure your actual gain before computing
> `amps_per_count`. Full reasoning in [BOM.md](BOM.md#-the-ihm08m1-tops-out-near-12-a-and-no-amount-of-cooling-changes-that).

### Digital / comms

| Function | Pin | Notes |
|---|---|---|
| Host USB serial | PA2 / PA3 | via ST-Link VCP, 921600 — bench telemetry before the ESP32 exists |
| Hall sensors | CN7-17, CN10-31, CN10-25 | H1/H2/H3, shield JP3 closed for pull-ups |
| SWD | PA13 / PA14 | leave alone |

### Overcurrent comparator

The G4's internal comparators can route to the timer break inputs **without leaving the chip**.
Feed a comparator from the same shunt-amplifier output the ADC reads, set the reference above your
peak operating current with margin, and route COMP → BKIN internally.

Verify this routing in CubeMX for your exact part — the comparator-to-timer matrix is specific and
not every COMP reaches every TIM.

> The shield also has its own OCP, trip point set by **CURRENT REF on CN10-27, a DAC output.** That
> threshold is firmware, not silicon — but it protects the shield, not your algorithm, and it does
> not raise the 12 A measurement ceiling.

### Bring-up order — Stage 1

Do not skip ahead. Each step is cheap; the failures they prevent are not.

1. **G474 alone.** Blink an LED, get SWD working, get the UART printing.
2. **PWM with no gate driver connected.** Scope every one of the six outputs. Confirm complementary
   pairs are actually complementary and **confirm deadtime**.
3. **Gate driver, no power stage.** Scope the gate outputs.
4. **Power stage on a current-limited supply at 2 A, no motor.** Nothing should draw current.
5. **Motor connected, clamped, no wheel.** Now you're at Phase 1.

---

## Stage 3 — the vehicle (2 × FSESC6.7 Pro + ESP32)

### ⚠ The flight pin map cannot be written yet

It comes out of the **FSESC6.7 Pro schematic for your exact board revision**, which is a
[Phase 2b gate](BUILD_PHASES.md#phase-2b--port-to-the-flight-power-stage) precisely because
nothing below it can be specified without it. You need:

- F405 port assignments for TIM1 complementary outputs and BKIN
- ADC channels and the current-sense **shunt value and amplifier gain** — `amps_per_count` comes
  straight out of this
- The gate driver part number (DRV8301 and DRV8323 variants both exist, different SPI registers)
- Hall/encoder input pins, and which pins are free for the peer link and fault line

**If no schematic is obtainable, buy hardware where one is.** Reverse-engineering a 70 A inverter
is not a reasonable use of your time and the current-sense scaling is not guessable.

### Power topology

```
   4S LiPo ──┬── main switch ── anti-spark ──┬── FRONT FSESC ── front motor
             │                               └── REAR  FSESC ── rear motor
             │
             └── 5V BEC ──┬── CRSF receiver
                          ├── steering servo
                          └── ESP32 + GPS
```

Rules that matter:

- **Each FSESC has its own bulk capacitance on-board, but check the lead length from the pack.**
  Long leads to the FETs are inductance, and switching into inductance is voltage spikes across
  your FETs. This is the most common way a working bench inverter dies in a vehicle.
- **Star ground at the pack negative.** Power ground and signal ground meet at exactly one point.
- **Separate BEC for logic.** The FSESC's on-board regulator can feed its own logic; do not chain
  the servo off it.
- **Common ground between both controllers and the ESP32.** Three boards with separate supplies
  and no shared reference do not communicate.
- **12 AWG for phase and battery runs.** At 50 A peak this is not the place to save grams.

### The three links that make the architecture work

| Link | From → to | Carries | Why it exists |
|---|---|---|---|
| **CRSF split** | receiver TX → *both* controllers' RX | Throttle, steering, failsafe | Both decode independently; neither depends on the other. ⚠ **Only one controller may drive the receiver's RX line** — CRSF telemetry is a shared bus and two transmitters collide. |
| **Peer link** | controller ↔ controller | RPM, current, heartbeat | The desync interlock's *data*. CAN preferred (one bus, also carries the ESP32); UART point-to-point acceptable. |
| **Fault lines** | each controller's fault out → the other's **TIM1 BKIN** | Nothing — it is a wire | The desync interlock's *cut*. Hardware, no software, no link. Same path as an overcurrent trip. |

See [ARCHITECTURE.md](ARCHITECTURE.md#the-desync-interlock-across-two-controllers) for why the
cut must be hardware and why the ESP32 cannot mediate any of this.

### ESP32 pin map (proposed)

The supervisor. Nothing here can hurt anything.

| Function | GPIO | Notes |
|---|---|---|
| Front controller — RX | 16 | UART2 RX |
| Front controller — TX | 17 | UART2 TX |
| Rear controller — RX | 25 | UART1 RX *(or drop both for a single CAN bus)* |
| Rear controller — TX | 26 | UART1 TX |
| GPS RX | 4 | 10 Hz module |
| GPS TX | 5 | for configuration |
| SD card | 18/19/23/13 | SPI, logging |
| Status LED | 2 | strapping pin, must be low/floating at boot |

> **Two controllers means two UART links, or one CAN bus.** CAN is the better answer here and
> [ARCHITECTURE.md](ARCHITECTURE.md#the-desync-interlock-across-two-controllers) now prefers it —
> one bus serves both controllers and the ESP32, the F405 has bxCAN natively, and the ESP32's TWAI
> peripheral speaks classic CAN. The UART rows above are the fallback.

ESP32 GPIO ground rules carry over unchanged from the crawler: **6–11 are the SPI flash, never use
them**; 34/35/36/39 are input-only with no internal pull-ups; 0, 2, 12, 15 are strapping pins.

### Bring-up order — Stage 3

1. **One controller, on the bench, with the Nucleo rig still assembled beside it.** That is Phase
   2b, and the old rig is your reference when the new board misbehaves.
2. **Second controller standing alone.** Re-run every Phase 1, 2 and 2b criterion on it.
3. **Peer link only**, both controllers powered, motors disconnected. Confirm heartbeats both ways
   and confirm a heartbeat timeout faults both.
4. **Fault lines.** Assert each controller's fault output by hand and confirm the *other* board's
   gates go off. Scope it.
5. **CRSF split.** Confirm both controllers decode the same frames and both fail safe on link loss.
6. **Motors connected, clamped, no wheels.** Now you're at Phase 3.

---

## Strain relief and harness routing

**Strain relief means making sure mechanical load is carried by something other
than the solder joint.**

Solder is an electrical connection, not a structural one. When a wire is pulled
or vibrated, the stress concentrates exactly where it exits the joint — solder
wicks a few millimetres up into the strands, so there is an abrupt hard-to-soft
transition right there. The copper work-hardens at that point and fatigue-cracks
after some number of cycles.

That failure mode is what makes it worth caring about: **it is not a joint that
fails when you build it, it is a joint that passes every bench test and breaks
in the vehicle.** A truck at speed on pavement is continuous high-frequency
vibration, which is precisely the fatigue regime.

Five things, in rough order of how much they buy you:

1. **Anchor the cable within 10–20 mm of the joint.** Zip tie to an adhesive
   pad or a chassis tab, so any pull on the harness is taken by the anchor and
   the joint sees nothing. This is the single biggest one.
2. **Leave a service loop.** A taut wire transmits every load to its ends. Slack
   lets the wire absorb movement in its middle, where it is designed to.
3. **Adhesive-lined heatshrink over each joint, extending onto the insulation.**
   It spreads the bend radius instead of letting it concentrate at the solder
   edge, and the adhesive seals out dirt and moisture.
4. **Keep joints short.** The less solder wicks up the strands, the less stiff
   the transition.
5. **Route so flexing happens mid-span**, not at a termination.

On the Nucleo morpho headers at Stage 1: **do not use bare Dupont jumpers.** The crimps walk off
0.1 in pins under vibration. Use latching or friction-lock connectors, or solder direct with the
strain relief above.

In the vehicle the equivalent risk moved to the **JST connectors on the FSESC boards** — hall
inputs, comms, and the fault lines. A fault line going open at speed silently removes the
cross-controller interlock, and nothing will tell you. **Test the fault lines as part of the
pre-run checklist**, not just once at Phase 3.

### Keep the signal wiring away from the phase leads

Routing matters electrically here, not just mechanically.

> **This got much better.** The old plan ran a second power stage on flying leads from the morpho
> headers — six gate-drive lines and three analog current-sense lines in one harness, which is
> about the worst thing you can build. **With integrated controllers, gate drive and current sense
> never leave the PCB.** The harness now carries only motor phase leads, hall cables, and comms.

What is left still needs care. The three phase leads carry hard switching edges at 50 A, and they
run alongside **hall sensor cables** and the **peer link**:

- **Hall cables and phase leads must not share a bundle.** A corrupted hall signal is a rotor
  angle error, and the sensored fallback exists precisely so you have something to trust when the
  observer does not.
- Signal runs on their own twisted pair or shielded run, shield grounded **at one end only** (the
  controller end) so it cannot become a ground loop.
- Cross phase leads at right angles rather than running parallel if they must meet.
- **Twist the three phase leads together.** Their fields largely cancel when twisted, which cuts
  what they radiate into everything else.
- Keep signal runs as short as the layout allows.

The same reasoning still applies inside Phase 1's pass criterion — phase current measurement is
[what Phase 1 is for](BUILD_PHASES.md#phase-1--one-motor-current-sensing-proven), and a harness
that looks tidy and reads garbage is a genuinely miserable thing to debug, because nothing about
it looks wrong.

## Bring-up order

Do not skip ahead. Each step is cheap; the failures they prevent are not.

1. **G4 alone.** Blink an LED, get SWD working, get the UART printing.
2. **PWM with no gate driver connected.** Scope every one of the six outputs. Confirm complementary
   pairs are actually complementary and **confirm deadtime**.
3. **Gate driver, no power stage.** Scope the gate outputs.
4. **Power stage on a current-limited supply at 2 A, no motor.** Nothing should draw current.
5. **Motor connected, clamped, no wheel.** Now you're at Phase 1.
