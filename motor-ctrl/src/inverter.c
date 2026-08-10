/**
 * @file inverter.c
 * @brief Timer, deadtime, gate, and break-input handling.
 *
 * STUB. See inverter.h — deadtime and the break input are described there and
 * both are safety-critical.
 *
 * !! Configure deadtime in TIMx->BDTR, never in software.
 * !! Scope every complementary pair before connecting a gate driver.
 */

#include "inverter.h"

void inverter_init(void)
{
    /* TODO Phase 1:
     *   - TIM1 and TIM8, center-aligned, PWM_FREQ_HZ, complementary outputs
     *   - BDTR deadtime = PWM_DEADTIME_NS, break input enabled, active low
     *   - TRGO on update -> ADC injected trigger (see current_sense.h)
     *   - Gates DISABLED on return. Enabling is always explicit. */
}

void inverter_set_duty(axle_id_t axle, float da, float db, float dc)
{
    (void)axle; (void)da; (void)db; (void)dc;
    /* TODO: scale 0..1 to CCR against ARR, write all three. */
}

bool inverter_enable(axle_id_t axle)
{
    if (inverter_break_latched(axle)) {
        return false;
    }
    /* TODO: MOE bit in BDTR. */
    return true;
}

void inverter_disable(axle_id_t axle)
{
    (void)axle;
    /* TODO: clear MOE. Outputs float, motor coasts. */
}

void inverter_disable_all(void)
{
    for (int a = 0; a < AXLE_COUNT; a++) {
        inverter_disable((axle_id_t)a);
    }
}

bool inverter_break_latched(axle_id_t axle)
{
    (void)axle;
    /* TODO: read the break interrupt flag. */
    return false;
}

bool inverter_clear_break(axle_id_t axle)
{
    (void)axle;
    /* TODO: refuse while the fault condition is still asserted. This must not
     * become a way to loop through a fault. */
    return false;
}
