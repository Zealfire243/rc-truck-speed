/**
 * @file current_sense.h
 * @brief Phase current measurement. Phase 1's entire pass criterion.
 *
 * ============================================================================
 * THE ONE THING THAT MATTERS: *WHEN* YOU SAMPLE
 * ============================================================================
 *
 * Phase current is a triangle wave riding on the PWM. Its average is what FOC
 * wants, and the average happens at exactly one instant per cycle: the center
 * of the PWM period, when the low-side FETs are all on and the current through
 * the shunts is the real phase current.
 *
 * Sample anywhere else and you get:
 *   - during a switching edge  -> ringing, garbage, wildly wrong values
 *   - at an arbitrary time     -> a sample of the ripple, not the average
 *
 * So the ADC MUST be triggered by the timer, not by software and not by a
 * periodic task:
 *
 *     TIM1/TIM8 update event  ->  ADC injected conversion  ->  ISR
 *
 * "Injected" is the STM32 term for a hardware-triggered, high-priority
 * conversion group that pre-empts regular conversions. Configure it in CubeMX
 * as JEXTTRIG from the timer's TRGO. This is not a detail — it is the
 * difference between Phase 1 passing and Phase 1 producing plausible-looking
 * garbage that poisons every later phase.
 *
 * ============================================================================
 * OFFSET CALIBRATION
 * ============================================================================
 *
 * The shunt amplifiers sit at a mid-rail bias so they can read bidirectional
 * current, and that bias is never exactly where the datasheet says. An
 * uncalibrated offset reads as a constant phantom current, which the observer
 * integrates into unbounded angle drift (see observer.h) and the current loop
 * treats as real torque.
 *
 * So: at boot, gates disabled, motor at rest, average several hundred samples
 * per channel and store the result. `csense_calibrate()`.
 *
 * Re-run it if the board gets meaningfully warmer or colder. The offset drifts
 * with temperature.
 *
 * ============================================================================
 * THE THREE-SHUNT REDUNDANCY
 * ============================================================================
 *
 * Two shunts are mathematically sufficient (ia + ib + ic == 0). This design
 * measures all three anyway, because that redundancy is the only way to CHECK
 * the measurement without a second instrument:
 *
 *     |ia + ib + ic|  should be ~0 at every instant.
 *
 * That sum is a live, free, continuous self-test of the entire analog chain.
 * It is Phase 1's second pass criterion and it is worth the third channel.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pin_map.h"

typedef struct {
    /* Calibration, in raw ADC counts. */
    uint16_t offset[AXLE_COUNT][3];
    bool     calibrated;

    /* Scale: amps per ADC count.
     *   amps_per_count = vref / (adc_full_scale * gain * shunt_ohms)
     * MEASURE this against a known load rather than trusting the arithmetic —
     * shunt tolerance and amplifier gain error both land here, and a scale
     * error is invisible until it becomes a torque loop that lies to you. */
    float    amps_per_count;

    /* Most recent readings, amps. */
    float    ia[AXLE_COUNT];
    float    ib[AXLE_COUNT];
    float    ic[AXLE_COUNT];

    float    vbus;
    float    temp_c[AXLE_COUNT];
} csense_t;

void csense_init(csense_t *cs, float amps_per_count);

/** Average several hundred samples per channel with the gates disabled and the
 *  motor at rest. Blocking; call once at boot before enabling anything. */
bool csense_calibrate(csense_t *cs);

/** Read the injected conversion results into the struct. Call at the top of
 *  the control ISR, before the observer and before FOC. */
void csense_read(csense_t *cs);

/** |ia + ib + ic| for one axle, amps. Should sit near zero at all times.
 *  Phase 1 pass criterion; a useful runtime health signal afterwards. */
float csense_residual(const csense_t *cs, axle_id_t axle);
