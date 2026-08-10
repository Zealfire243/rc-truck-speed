# Wiring

> **Status: proposed, not yet built.** Nothing here has been wired, because nothing has been
> bought. Every pin below is a starting point that must be checked against the actual STM32G474
> datasheet and your board's schematic in CubeMX before you commit to a harness. Alternate-function
> conflicts on the G4 are dense — several of these pins have three plausible uses.

## Power topology

```
   4S LiPo ──┬── main switch ── bulk caps ──┬── front inverter ── front motor
             │                              └── rear inverter  ── rear motor
             │
             └── 5V BEC ──┬── STM32G4
                          ├── CRSF receiver
                          ├── steering servo
                          └── ESP32 + GPS
```

Rules that matter:

- **Bulk capacitance at each inverter, physically close.** Long leads from the pack to the FETs are
  inductance, and switching into inductance is voltage spikes across your FETs. This is the most
  common way a working bench inverter dies in a vehicle.
- **Star ground at the pack negative.** Power ground and signal ground meet at exactly one point.
  Current-sense accuracy dies to ground loops, and current-sense accuracy is Phase 1's entire
  pass criterion.
- **Separate BEC for logic**, not a tap off something that also feeds a motor.
- **Common ground between the G4 and the ESP32** on the UART link. Two boards with separate
  supplies and no shared reference do not communicate.

## STM32G474 pin map (proposed)

### Front axle — TIM1

| Function | Pin | Notes |
|---|---|---|
| PWM CH1 | PA8 | |
| PWM CH2 | PA9 | |
| PWM CH3 | PA10 | |
| PWM CH1N | PB13 | complementary |
| PWM CH2N | PB14 | complementary |
| PWM CH3N | PB15 | complementary |
| **Break in (BKIN)** | PB12 | **overcurrent → gates off in hardware** |

### Rear axle — TIM8

| Function | Pin | Notes |
|---|---|---|
| PWM CH1 | PC6 | |
| PWM CH2 | PC7 | |
| PWM CH3 | PC8 | |
| PWM CH1N | PA7 | complementary |
| PWM CH2N | PB0 | complementary |
| PWM CH3N | PB1 | complementary |
| **Break in (BKIN)** | PB6 | **overcurrent → gates off in hardware** |

### Analog

| Function | Pin | ADC |
|---|---|---|
| Front phase A current | PA0 | ADC1_IN1 |
| Front phase B current | PA1 | ADC1_IN2 |
| Front phase C current | PA2 | ADC1_IN3 |
| Rear phase A current | PA4 | ADC2_IN17 |
| Rear phase B current | PA5 | ADC2_IN13 |
| Rear phase C current | PA6 | ADC2_IN3 |
| Pack voltage sense | PA3 | ADC1_IN4, divider |
| Front inverter temp | PC0 | ADC1_IN6, thermistor |
| Rear inverter temp | PC1 | ADC1_IN7, thermistor |

**All six current channels are sampled by injected conversion triggered off the timer update
event**, so every phase is measured at the same instant in the PWM cycle. This is not a detail —
it's the difference between Phase 1 passing and Phase 1 producing plausible-looking garbage. See
`current_sense.h`.

### Digital / comms

| Function | Pin | Notes |
|---|---|---|
| CRSF receiver | PB10 / PB11 | USART3 TX/RX. Receiver goes **here**, not to the ESP32. |
| Link to ESP32 | PC10 / PC11 | UART4 TX/RX, 921600, common ground |
| Steering servo | PC9 | TIM3 CH4, 50 Hz |
| Status LED | PA5 | shared with an ADC channel on Nucleo — move it |
| SWD | PA13 / PA14 | leave alone |

### Overcurrent comparator

The G4's internal comparators can route to the timer break inputs **without leaving the chip**.
Feed each comparator from the same shunt-amplifier output the ADC reads, set the reference above
your peak operating current with margin, and route COMP → BKIN internally.

Verify this routing in CubeMX for your exact part — the comparator-to-timer matrix is specific and
not every COMP reaches every TIM.

## ESP32 pin map (proposed)

The supervisor. Nothing here can hurt anything.

| Function | GPIO | Notes |
|---|---|---|
| Link to G4 — RX | 16 | UART2 RX |
| Link to G4 — TX | 17 | UART2 TX |
| GPS RX | 4 | UART1 RX, 10 Hz module |
| GPS TX | 5 | UART1 TX, for configuration |
| SD card | 18/19/23/13 | SPI, logging |
| Status LED | 2 | strapping pin, must be low/floating at boot |

ESP32 GPIO ground rules carry over unchanged from the crawler: **6–11 are the SPI flash, never use
them**; 34/35/36/39 are input-only with no internal pull-ups; 0, 2, 12, 15 are strapping pins.

## Bring-up order

Do not skip ahead. Each step is cheap; the failures they prevent are not.

1. **G4 alone.** Blink an LED, get SWD working, get the UART printing.
2. **PWM with no gate driver connected.** Scope every one of the six outputs. Confirm complementary
   pairs are actually complementary and **confirm deadtime**.
3. **Gate driver, no power stage.** Scope the gate outputs.
4. **Power stage on a current-limited supply at 2 A, no motor.** Nothing should draw current.
5. **Motor connected, clamped, no wheel.** Now you're at Phase 1.
