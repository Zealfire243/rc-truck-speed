/**
 * @file current_sense.c
 * @brief Phase current acquisition. See current_sense.h — that header is the
 *        design document, and its "when you sample" section is Phase 1's
 *        entire pass criterion.
 *
 * STUB. Hardware-dependent: the injected-conversion setup has to match the
 * real ADC/timer wiring, which does not exist yet. Write this alongside the
 * board, not before it.
 */

#include "current_sense.h"

void csense_init(csense_t *cs, float amps_per_count)
{
    cs->amps_per_count = amps_per_count;
    cs->calibrated = false;
    cs->vbus = 0.0f;
    for (int a = 0; a < AXLE_COUNT; a++) {
        cs->ia[a] = cs->ib[a] = cs->ic[a] = 0.0f;
        cs->temp_c[a] = 25.0f;
        for (int p = 0; p < 3; p++) {
            cs->offset[a][p] = 0;
        }
    }
}

bool csense_calibrate(csense_t *cs)
{
    /* TODO Phase 1: gates disabled, motor at rest, average >=256 samples per
     * channel into cs->offset. Blocking. Reject the result if the spread
     * across samples is wide — that means the motor is moving or the analog
     * chain is noisy, and calibrating against either bakes in an error. */
    cs->calibrated = false;
    return false;
}

void csense_read(csense_t *cs)
{
    /* TODO Phase 1: read the injected data registers, subtract offset, scale
     * by amps_per_count, apply MOTOR_INVERT_* sign correction. */
    (void)cs;
}

float csense_residual(const csense_t *cs, axle_id_t axle)
{
    float sum = cs->ia[axle] + cs->ib[axle] + cs->ic[axle];
    return sum < 0.0f ? -sum : sum;
}
