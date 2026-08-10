/**
 * @file observer.c
 * @brief Flux-linkage observer + PLL.
 *
 * STUB. The algorithm is spelled out in full in observer.h — read that first;
 * it is the design document for this file.
 *
 * Left deliberately unwritten because the parameters it needs (R, L, pole
 * pairs) must be MEASURED off the real motor, and writing the tuning around
 * guessed values produces code that looks finished and works on nothing. Fill
 * this in during Phase 2, with the motor on the bench and the plot open.
 *
 * Implementation order that works:
 *
 *   1. Forced commutation only. Ignore the observer entirely, spin the motor
 *      open-loop at a fixed electrical frequency, confirm it follows.
 *   2. Run the flux integrator alongside, PUBLISH ITS ANGLE IN TELEMETRY, but
 *      keep using the forced angle. Compare the two on a plot. This is the
 *      single most valuable debugging step in the whole phase and it is very
 *      tempting to skip.
 *   3. Only when the observed angle tracks the forced one, switch over.
 *   4. Then build the handover, and hunt the 20-clean-starts criterion.
 *
 * The drift problem in observer.h is not hypothetical. Budget for it.
 */

#include "observer.h"

#include <math.h>

void observer_init(observer_t *o, float r, float l, uint8_t pole_pairs)
{
    o->r_phase = r;
    o->l_phase = l;
    o->pole_pairs = pole_pairs;

    o->flux_a = 0.0f;
    o->flux_b = 0.0f;

    o->theta_hat = 0.0f;
    o->omega_e = 0.0f;

    /* Starting points only. Tune in Phase 2 against a real motor: raise kp
     * until the angle tracks without lag, then ki until steady-state error
     * goes away, then back both off from where it oscillates. */
    o->kp_pll = 200.0f;
    o->ki_pll = 5000.0f;

    o->locked = false;
    o->ramp_theta = 0.0f;
    o->ramp_freq_hz = 0.0f;
    o->handover_rpm = 500.0f;
    o->track_err = 0.0f;
}

void observer_start_ramp(observer_t *o, float start_freq_hz)
{
    o->locked = false;
    o->ramp_theta = 0.0f;
    o->ramp_freq_hz = start_freq_hz;
    o->flux_a = 0.0f;
    o->flux_b = 0.0f;
    o->omega_e = 0.0f;
}

float observer_step(observer_t *o, float i_alpha, float i_beta,
                    float v_alpha, float v_beta, float dt)
{
    /* TODO Phase 2 — see observer.h for the full algorithm.
     *
     *   flux_a += (v_alpha - R*i_alpha) * dt      (with leakage, not pure!)
     *   flux_b += (v_beta  - R*i_beta ) * dt
     *   pm_a = flux_a - L*i_alpha
     *   pm_b = flux_b - L*i_beta
     *   err  = (pm_b*cos(theta_hat) - pm_a*sin(theta_hat)) / |pm|
     *   omega_e   += ki_pll * err * dt
     *   theta_hat += (omega_e + kp_pll * err) * dt
     */
    (void)i_alpha; (void)i_beta; (void)v_alpha; (void)v_beta;

    /* Until then: forced commutation, so the rest of the stack can be brought
     * up and Phase 1 can run. */
    o->ramp_theta += 2.0f * (float)M_PI * o->ramp_freq_hz * dt;
    while (o->ramp_theta > 2.0f * (float)M_PI) {
        o->ramp_theta -= 2.0f * (float)M_PI;
    }
    o->theta_hat = o->ramp_theta;
    o->omega_e = 2.0f * (float)M_PI * o->ramp_freq_hz;

    return o->theta_hat;
}

float observer_rpm(const observer_t *o)
{
    if (o->pole_pairs == 0) {
        return 0.0f;
    }
    return o->omega_e * 60.0f / (2.0f * (float)M_PI) / (float)o->pole_pairs;
}

bool observer_lost(const observer_t *o)
{
    /* TODO Phase 2: threshold on sustained track_err. Returning false while
     * the observer is a forced ramp is correct — there is nothing to lose. */
    return false;
}
