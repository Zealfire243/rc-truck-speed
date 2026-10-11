# Pin continuity checklist — FSESC6.7 Pro, board A

This checklist turns the **candidate** values in
[`motor-ctrl/inc/pin_map.h`](../motor-ctrl/inc/pin_map.h) into **verified** ones. Each candidate
came from three sources that agree: the stock firmware's live register state (read with
[`motor-ctrl/tools/gpio_dump.cfg`](../motor-ctrl/tools/gpio_dump.cfg)), the original VESC 6.0
hwconf, and the ST and TI datasheets. None of those proves where the copper goes. These checks
do.

**Only the PWM checks (P1–P6, X1) guard against shoot-through.** Do those first, and do them
most carefully. The rest guard against a gate driver that won't answer, a fault that is never
seen, or a current loop that reads the wrong phase.

Pin numbers come from **TI SLOS719F** (DRV8301, 56-pin HTSSOP "DCA") and **ST's
STM32F405RGTx pin database** (LQFP64). Check that your chips match those packages before you
start (step 0.3).

---

## 0. Before you touch a probe

**0.1 Power everything off, completely.**
- Unplug the ST-Link from USB **and** from the SWD connector. A powered ST-Link back-feeds 3V3
  into the board and makes every reading to 3V3 or ground meaningless.
- Disconnect USB, the bench supply, and anything on COMM, CAN or SENSE.
- If the board has *ever* been on the bench supply, measure across the main power input (the
  BATTERY leads) in DC volts. Wait until it reads under 0.1 V. The bulk capacitors hold charge.

**0.2 Set up the meter.**
- Use **resistance (Ω) mode** for every check and write the number down. Don't use beep mode
  alone: many meters beep at anything under 30–50 Ω, which hides a series resistor you need to
  know about, and some don't beep through 100 Ω at all.
- Short the probes together and note the reading, typically 0.1–0.5 Ω. That's your zero.
- The pins are on a **0.5 mm pitch** on both chips. Standard probe tips are wider than one pin.
  Use fine-tip probes, or tape a sewing needle to each probe. Good light and a magnifier help.
  Press on the **foot of the pin where it meets the pad**, not the top of the lead. You will
  slip onto the neighbouring pin otherwise.
- Where a pin is too hard to hit, probe the **nearest via, resistor or capacitor pad on the
  same trace** instead. Make sure it really is the same net: follow the trace by eye.

**0.3 Identify the chips and find pin 1.**
- **STM32F405RGT6**: the marking should read `STM32F405RGT6` (or similar), with 64 pins and 16
  per side. Pin 1 is next to the dot in one corner. Viewed from the top, pins count
  **counter-clockwise**:

  | Side (pin 1 at top-left) | Pins | Counting direction |
  |---|---|---|
  | Left | 1–16 | top → bottom |
  | Bottom | 17–32 | left → right |
  | Right | 33–48 | bottom → top |
  | Top | 49–64 | right → left |

- **DRV8301**: a long narrow 56-pin chip next to the FETs, with 28 pins per long side. Pin 1
  is next to the dot or notch. Again counter-clockwise from the top: **pins 1–28 run down one
  long side and pins 29–56 come back up the other**, so pin 28 is directly across from
  pin 29.

**0.4 Count from the nearest corner, never from pin 1.** Counting 40 pins at 0.5 mm goes
wrong. Every check below gives the pin's position counted from the start of its side, e.g.
"right side, 9th from bottom".

**0.5 Find reference points for GND and 3V3.**
- **GND**: the battery-negative pad, or the DRV8301's exposed pad. Confirm with the MCU's
  VSS pin 18 (bottom side, 2nd from left) reading ~0 Ω to it.
- **3V3**: the `3.3V` pin on the SWD or COMM header. Confirm with the MCU's VDD pin 19 (bottom
  side, 3rd from left) reading ~0 Ω to it.

---

## 1. How to read the results

| Reading | Meaning |
|---|---|
| Under 1 Ω (above your probe zero) | A direct trace. **Pass.** |
| 10 Ω to a few kΩ, steady | Probably a **series resistor** on the trace. Common on gate inputs (tens of Ω) and on ADC lines (RC filter, hundreds of Ω to a few kΩ). Find the resistor and confirm both of its ends are on the two pins. Then it's a pass, and you record the value. |
| Slowly drifting value | You're charging a capacitor. Not a direct connection. Look for the resistor. |
| Open / OL | No connection. **Fail**, unless the check expects open. |

When a check fails: **stop, don't promote that pin,** photograph the area, and write down what
you measured. A failure on P1–P6 or X1 means the PWM map is wrong, and nothing gets flashed to
drive the gates until it's understood.

---

## 2. PWM — the shoot-through checks (do these first)

Each MCU timer channel and its complementary output (CHx and CHxN) must land on the **same
half-bridge** of the DRV8301: one on that bridge's high input and the other on its low input.
If PA8 went to INH_A but PB13 went to INL_B, the timer's deadtime would protect nothing.

| ID | MCU pin | Where on the MCU | DRV8301 pin | Where on the DRV8301 | Expect | Reading | Pass |
|---|---|---|---|---|---|---|---|
| P1 | PA8 (41), TIM1_CH1 | right side, 9th from bottom | INH_A (17) | pin-1 side, 17th from pin 1 | < 1 Ω or series R | | |
| P2 | PB13 (34), TIM1_CH1N | right side, 2nd from bottom | INL_A (18) | pin-1 side, 18th from pin 1 | < 1 Ω or series R | | |
| P3 | PA9 (42), TIM1_CH2 | right side, 10th from bottom | INH_B (19) | pin-1 side, 19th | < 1 Ω or series R | | |
| P4 | PB14 (35), TIM1_CH2N | right side, 3rd from bottom | INL_B (20) | pin-1 side, 20th | < 1 Ω or series R | | |
| P5 | PA10 (43), TIM1_CH3 | right side, 11th from bottom | INH_C (21) | pin-1 side, 21st | < 1 Ω or series R | | |
| P6 | PB15 (36), TIM1_CH3N | right side, 4th from bottom | INL_C (22) | pin-1 side, 22nd | < 1 Ω or series R | | |

**X1 — no cross-connections between the six PWM lines.** Measure each of the six DRV8301 input
pins (17–22) against every other one. That's 15 pairs, and all should read **open / OL**. Then
measure each of the six against GND and against 3V3, which should also be open. The DRV8301
has internal 100 kΩ pull-downs on these inputs, but only while EN_GATE is high. Unpowered you
may see a high resistance to GND through protection structures. That's fine; a short isn't.
Adjacent pins at 0.5 mm are exactly where a solder bridge hides.

| Pair | Reading | | Pair | Reading | | Pair | Reading |
|---|---|---|---|---|---|---|---|
| 17–18 | | | 18–19 | | | 19–21 | |
| 17–19 | | | 18–20 | | | 19–22 | |
| 17–20 | | | 18–21 | | | 20–21 | |
| 17–21 | | | 18–22 | | | 20–22 | |
| 17–22 | | | 19–20 | | | 21–22 | |

**P7 — which half-bridge is which motor wire.** This doesn't affect shoot-through, but it sets
phase labels for the rest of the project. Find the three thick motor output wires or pads, and
measure each against the DRV8301's high-side source pins: **SH_A (46)**, **SH_B (41)**, **SH_C
(36)**. Counted from the bottom of the pins 29–56 side, those are the 18th, 13th and 8th pins.
Each motor output should read < 1 Ω to exactly one of them.

| Motor output (label or position) | SH_A (46) | SH_B (41) | SH_C (36) |
|---|---|---|---|
| | | | |
| | | | |
| | | | |

---

## 3. Gate driver control and SPI

The DRV8301's SPI pins are named from the **driver's** point of view, so the MCU's MOSI goes
to the driver's **SDI**, and the driver's **SDO** comes back to the MCU's MISO.

| ID | MCU pin | Where on the MCU | DRV8301 pin | Where on the DRV8301 | Expect | Reading | Pass |
|---|---|---|---|---|---|---|---|
| S1 | PC9 (40), chip select | right side, 8th from bottom | nSCS (8) | pin-1 side, 8th | < 1 Ω or series R | | |
| S2 | PC10 (51), clock | top side, 3rd from right | SCLK (11) | pin-1 side, 11th | < 1 Ω or series R | | |
| S3 | PC12 (53), MOSI | top side, 5th from right | SDI (9) | pin-1 side, 9th | < 1 Ω or series R | | |
| S4 | PC11 (52), MISO | top side, 4th from right | SDO (10) | pin-1 side, 10th | < 1 Ω or series R | | |
| S5 | VDD_SPI (49) on the DRV8301 | 21st from bottom on the 29–56 side | 3V3 reference | — | < 1 Ω (confirms 3.3 V SPI logic) | | |
| E1 | PB5 (57), gate enable | top side, 9th from right | EN_GATE (16) | pin-1 side, 16th | < 1 Ω or series R | | |

**If S2–S4 fail but read through to PB3/PB4 instead,** this board is on the MK3–MK6 pin set
after all and the stock-firmware evidence was misleading. Stop, record what you found, and
don't promote any SPI pin.

---

## 4. Fault lines

| ID | From | To | Expect | Reading | Pass |
|---|---|---|---|---|---|
| F1 | MCU PB7 (59), top side, 11th from right | DRV8301 nFAULT (6), pin-1 side, 6th | < 1 Ω or series R | | |
| F2 | DRV8301 nFAULT (6) | 3V3 reference | A pull-up resistor, typically 1–10 kΩ. The DRV8301's fault output is open-drain and needs one. Record the value. | | |
| F3 | DRV8301 nFAULT (6) | MCU **PB12** (33), right side, 1st from bottom (corner) | **Probably open.** If it connects, the hardware break path exists. See below. | | |
| F3b | DRV8301 nFAULT (6) | MCU **PA6** (22), bottom side, 6th from left | **Probably open.** Same reason. | | |
| F4 | DRV8301 nOCTW (5), pin-1 side, 5th | MCU PB12 (33), PA15 (50), PB6 (58), and 3V3 | Probably not routed to the MCU. Record anything that connects. | | |

**Why F3 matters.** The plan in `pin_map.h` and [SAFETY.md](SAFETY.md) was to feed the gate
driver's fault output into TIM1's hardware **break input**, so the timer itself kills the PWM
in nanoseconds. On the F405, TIM1's break input only exists on **PB12 or PA6**, and nFAULT's
candidate pin, PB7, is neither. The stock firmware polls PB7 as a GPIO and leaves the break
input off. If F3 and F3b both read open, there is no hardware break path on this board.
That's a design decision to revisit, not a reason to stop. The DRV8301's own latched
overcurrent shutdown still kills the gates in hardware; the timer just won't know about it.

---

## 5. Current sense and bus voltage

These lines usually run through an RC filter, so **expect a resistor (often 100 Ω–few kΩ),
not a dead short.** Use resistance mode and follow the trace to find it.

The DRV8301 has only **two** current amplifiers (SO1, SO2), but this board has three shunts.
The third goes through a separate amplifier somewhere near the shunts, a small 5- to 8-pin
chip. Finding it is part of A3.

| ID | Check | Expect | Reading | Pass |
|---|---|---|---|---|
| A1 | DRV8301 SO1 (25, pin-1 side, 25th) → which of MCU PC0 (8), PC1 (9), PC2 (10)? Those are left side, 8th, 9th and 10th from top. | One of the three, through a resistor | | |
| A2 | DRV8301 SO2 (26, pin-1 side, 26th) → which of PC0 / PC1 / PC2? | A different one of the three | | |
| A3 | The third amplifier's output → the remaining one of PC0 / PC1 / PC2 | The last one | | |
| A4 | Which shunt each amplifier reads. For the DRV8301: SN1 (33) and SN2 (31) are the shunt-side inputs, 5th and 3rd from bottom on the 29–56 side. Trace each to a shunt, then trace that shunt to a low-side FET whose source ties to SL_A (44), SL_B (39) or SL_C (34). | A complete map: ADC pin → amplifier → shunt → phase | | |
| A5 | Bus voltage divider: MCU PC3 (11, left side, 11th from top) to the battery-positive input, and PC3 to GND | Roughly the top and bottom divider resistors (VESC 6 uses 39 kΩ / 2.2 kΩ). Read the resistor markings too, because in-circuit readings are distorted by parallel paths. | | |
| A6 | Board temperature: MCU PA3 (17, bottom side, 1st from left) to the thermistor near the FETs | Through a resistor or the thermistor itself | | |

**A4 is the one that matters for torque control.** A current loop that reads phase B's shunt
and calls it phase A will produce torque that's wrong in direction and magnitude, and the
host-side tests can't catch it. Record the full chain for each phase.

---

## 6. Unexplained pins

| ID | Check | Reading / what it connects to |
|---|---|---|
| M1 | MCU PA15 (50), top side, 2nd from right. The stock firmware drives it as an open-drain output with a pull-up. Trace it. | |
| M2 | MCU PB12 (33). Plain input in the stock firmware. Already covered by F3 and F4; note anything else it connects to. | |

---

## 7. Sign-off

When a check passes, change the matching `PIN_UNVERIFIED` in `pin_map.h` to the real pin. The
commit message should say **how** each was established, for example "continuity, 0.3 Ω, check
P1". Define `PIN_MAP_VERIFIED` only when:

- [ ] P1–P6 pass, X1 is open on all 15 pairs, and nothing reads shorted to GND or 3V3.
- [ ] P7 maps every motor output to exactly one half-bridge.
- [ ] S1–S4 and E1 pass, and S5 confirms 3.3 V SPI logic.
- [ ] F1 and F2 pass. F3/F3b recorded either way, and if open, the break-input question is
      written up in SAFETY.md.
- [ ] A1–A4 give a complete ADC pin → amplifier → shunt → phase map. A5 and A6 recorded.
- [ ] Results are recorded in this file with date and board ID.

After that come the powered checks, which can't be done with a meter: with the bench supply on
at a low current limit, PB7 (nFAULT) should read **high** once PVDD is present. That confirms
the "fault asserted" reading from the ST-Link-only dump was just undervoltage. Then the DRV8301
register readback (Phase 1a step 4) and the gate-drive scope check (step 5).
