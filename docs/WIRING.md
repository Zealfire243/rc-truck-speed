# Wiring

> **Status: proposed, not yet built.** Nothing here has been wired yet, though the controllers,
> motors, pack and supervisor are now in hand.
>
> **This document covers one machine at two scales.** Stages 1–2 are a single Flipsky FSESC6.7
> Pro on the bench; Stage 3 is two of them in the truck. The bench rig is the flight hardware
> with less of it, which is a change from an earlier plan that used a separate Nucleo bench rig.
> See [BOM.md](BOM.md#decision-no-stage-1-rig-straight-to-the-fsesc).

---

## Stages 1–2 — bench rig (one FSESC6.7 Pro, one motor)

**One motor, one power stage, no vehicle — on the same board that flies.** An earlier version of
this document specified a Nucleo-G474RE + X-NUCLEO-IHM08M1 bench rig here, with a pin map
dictated by the shield's copper. That rig was never bought and the stage was dropped; see
[BOM.md](BOM.md#decision-no-stage-1-rig-straight-to-the-fsesc).

> ⚠ **The bench pin map is the flight pin map, and it cannot be written yet** — for exactly the
> reasons given under [Stage 3](#-the-flight-pin-map-cannot-be-written-yet) below. Read that
> section; it now gates Phase 1 rather than Phase 2b, because there is no longer an earlier
> platform to learn on while it stays open.

**What is wired at the bench, and nothing more:**

| Connection | Notes |
|---|---|
| Bench supply → board A DC input | **Current-limited, always.** A few hundred mA for every first-time test. The limit is the safety net that the dropped Stage 1 rig used to be. |
| SWD probe → board A SWD pads | Standalone ST-Link. Continuity-check the pads against Flipsky's pinout before trusting them. |
| Board A → motor, three phase leads | **Not connected until [Phase 1a](BUILD_PHASES.md#phase-1a--the-board-before-a-motor-exists) passes in full.** |
| Motor sensor harness | Hall inputs; the QuicRun 3650 G2 is sensored. |
| Board A → host, USB or UART | Telemetry and the tuner. See [PROTOCOL.md](PROTOCOL.md). |
| Scope probes → gate outputs | For the deadtime verification in Phase 1a, taken with **no motor connected**. |

**No LiPo appears anywhere in this table.** The first pack connection of the project happens
after Phase 2b passes.

### Bring-up order — bench

Do not skip ahead. Each step is cheap; the failures they prevent are not. This mirrors
[Phase 1a](BUILD_PHASES.md#phase-1a--the-board-before-a-motor-exists) — that is the authoritative
version, with the pass criteria.

1. **SWD attached, stock firmware dumped to a file.** Before anything is overwritten.
2. **Stock VESC firmware, motor spinning.** Your known-good reference, and where the motor's R, L
   and flux linkage come from.
3. **Your firmware: gate driver SPI only, gate outputs disabled.** Registers read back sane, VDS
   overcurrent protection armed at its most sensitive setting.
4. **PWM with the gate outputs enabled, no motor connected.** Scope all six. Confirm complementary
   pairs really are complementary and **confirm deadtime across the whole duty range**, including
   the transitions at 0% and 100%.
5. **Current-limited supply at a few hundred mA, still no motor.** Nothing should draw current.
6. **Motor connected, clamped, no wheel.** Now you're at Phase 1b.

---

## Stage 3 — the vehicle (2 × FSESC6.7 Pro + ESP32)

### ⚠ The flight pin map cannot be written yet

It comes out of the **FSESC6.7 Pro schematic for your exact board revision**, which no longer
has one — and with no separate bench platform it now gates
[Phase 1](BUILD_PHASES.md#phase-1--one-motor-current-sensing-proven) rather than Phase 2b.
Most of it is recoverable by inspection and measurement rather than from a document (see the
on-arrival checklist in [BOM.md](BOM.md)), but it has to be recovered before the pin map below
means anything. You need:

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

**Board: ESP32-S3-DevKitC-1U-N16R8.** Authoritative source is
[`supervisor/main/pin_map.h`](../supervisor/main/pin_map.h) — this table mirrors it.

| Function | GPIO | Notes |
|---|---|---|
| **CAN TX** | **5** | TWAI. **Primary link — one bus, both controllers.** |
| **CAN RX** | **4** | Needs an external transceiver; see below. |
| Front controller — TX | 43 | UART0 *(fallback only)* — free only because the console is on USB |
| Front controller — RX | 44 | UART0 *(fallback only)* |
| Rear controller — TX | 15 | UART2 *(fallback only)* |
| Rear controller — RX | 16 | UART2 *(fallback only)* |
| GPS TX | 17 | UART1, to the module's RX (10 Hz, configurable) |
| GPS RX | 18 | UART1 |
| SPI2 SCLK / MOSI / MISO | 12 / 11 / 13 | Native IOMUX pins — 80 MHz, not the matrix's 40 |
| SD card CS | 9 | shares SPI2 |
| **IMU CS** | **10** | shares SPI2 |
| **IMU INT** | **14** | data-ready. Sample on it; polling aliases against the ODR. |
| Status LED | 48 | onboard addressable RGB (v1.0 boards use 38) — drive with RMT |

> **Why the S3 and not any ESP32: the UART count binds.** Two controllers, GPS and a console is
> four serial endpoints, and both chips have three UART controllers. The S3's native
> USB-Serial-JTAG takes the console off UART0 and creates the fourth. **Moving the console back
> to UART breaks the rear-axle link** — that is why `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG` is
> load-bearing in `sdkconfig.defaults`, not a preference.

> ⚠ **TWAI needs an external CAN transceiver.** The two pins above are logic-level TX/RX, not a
> line driver. The FSESC boards carry their own (VESC 6 exposes CANH/CANL directly); **the ESP32
> does not.** One SN65HVD230 / TJA1051 / MCP2562 on this end, and 120 Ω termination at each of
> the two physical bus ends only.

**S3 GPIO ground rules — these are NOT the classic-ESP32 rules the crawler used**, and the
differences cut both ways:

- **6–11 are ordinary GPIOs here.** On the classic ESP32 they were SPI flash and touching them
  bricked boot. The S3 puts flash on 26–32. The old warning was a trap carried forward.
- **35, 36, 37 are unavailable** — octal PSRAM on any `R8` part uses them.
- **19, 20 are native USB.** 43, 44 are default UART0. 0, 3, 45, 46 are strapping pins.
- **33, 34 exist on the chip but not on the DevKitC-1 headers.**

### Bring-up order — Stage 3

1. **Board A alone, fully through Phase 2b.** Keep the stock-firmware image and VESC Tool on
   hand — reflashing stock is how you tell a board fault from a firmware fault.
2. **Unseal board B and bring it up standing alone.** Re-run every Phase 1, 2 and 2b criterion on
   it, Phase 1a included; it is a different board and it has proven nothing yet.
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

On the FSESC's signal headers, bench included: **do not use bare Dupont jumpers.** The crimps walk
off 0.1 in pins under vibration. Use latching or friction-lock connectors, or solder direct with the
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
