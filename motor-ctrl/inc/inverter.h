/**
 * @file inverter.h
 * @brief The hardware seam. Duties in, gates driven.
 *
 * This interface is to this project what `motor_driver.h` was to the crawler:
 * the one place that knows about timers, gate drivers, and FETs, and the
 * boundary that everything above it is written against.
 *
 * **If a hardware change forces this interface to change, the abstraction was
 * wrong.** That rule is what makes the AM32-ESC off-ramp in docs/BOM.md real
 * rather than aspirational — swapping a hand-built inverter for a commercial
 * ESC driven over DShot should be a new implementation of this header and
 * nothing else.
 *
 * ============================================================================
 * DEADTIME
 * ============================================================================
 *
 * The high and low FETs of a half-bridge must never conduct at once. They
 * switch in nanoseconds and the gate charge takes time to drain, so commanding
 * one off and the other on simultaneously shorts the pack through both. That
 * is shoot-through, it destroys the bridge in microseconds, and it often fails
 * SHORT — putting full pack voltage across whatever is downstream.
 *
 * Deadtime is a hardware feature of TIM1/TIM8 (the BDTR register). Configure
 * it there. Do NOT attempt it in software; the ISR is not deterministic enough
 * and a missed deadline becomes a dead bridge.
 *
 * **Scope every complementary pair before connecting the gate driver.** This is
 * a Phase 1 pass criterion and a docs/SAFETY.md item.
 *
 * ============================================================================
 * THE BREAK INPUT
 * ============================================================================
 *
 * BKIN is a hardware path from an overcurrent comparator straight to the
 * timer's output-disable. When it fires, the gates go off in the same clock
 * cycle, with no interrupt, no ISR, and no dependency on the software being
 * correct or even running.
 *
 * This is the only protection layer that survives a firmware bug, so it must be
 * wired and tested before the truck moves. Test it deliberately: force the
 * comparator input over threshold and confirm the gates actually go dead.
 * A protection path you have never seen fire is a protection path you do not
 * have.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pin_map.h"

/** Configure timers, deadtime, complementary outputs, break input, and the
 *  ADC trigger. Gates stay DISABLED on return — enabling is always explicit. */
void inverter_init(void);

/** Apply duties, 0.0 .. 1.0, for one axle. Called from the control ISR once
 *  per PWM cycle, immediately after foc_step(). */
void inverter_set_duty(axle_id_t axle, float da, float db, float dc);

/** Enable gates. Refuses while a break condition is latched. */
bool inverter_enable(axle_id_t axle);

/** Gates off — outputs float, motor coasts. Distinct from commanding zero
 *  voltage (all duties 0.5), which keeps the bridge active. */
void inverter_disable(axle_id_t axle);

/** Both axles off, immediately. The path every fault takes. */
void inverter_disable_all(void);

/** True if the hardware break input has latched on this axle. */
bool inverter_break_latched(axle_id_t axle);

/** Clear a latched break. Fails if the fault condition is still present —
 *  which is the point; this must not become a way to loop through a fault. */
bool inverter_clear_break(axle_id_t axle);
