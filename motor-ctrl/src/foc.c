/**
 * @file foc.c
 * @brief FOC transforms and current loops. See foc.h for the why.
 *
 * Fully implemented — this is pure math with no hardware dependency, so there
 * is no reason for it to be a stub. It is also the part that is easiest to get
 * subtly wrong and hardest to debug on a spinning motor, so test it on the
 * host first: feed known vectors through Clarke -> Park -> inverse Park ->
 * Clarke and confirm you get back what you put in.
 */

#include "foc.h"

#include <math.h>

#define SQRT3        1.7320508075688772f
#define ONE_OVER_S3  0.5773502691896258f
#define SQRT3_OVER_2 0.8660254037844386f

/* -------------------------------------------------------------------------
 * PI with clamping anti-windup
 * ------------------------------------------------------------------------- */

void pi_reset(pi_t *pi)
{
    pi->integ = 0.0f;
    pi->clamped = false;
}

float pi_update(pi_t *pi, float error, float dt)
{
    float unclamped = pi->kp * error + pi->integ + pi->ki * error * dt;

    float out = unclamped;
    if (out > pi->out_max) {
        out = pi->out_max;
    } else if (out < pi->out_min) {
        out = pi->out_min;
    }

    /* Only integrate when we are not pinned against a rail, or when the error
     * would drive us back off it. This is conditional integration: cheaper
     * than back-calculation and, for a current loop that saturates on every
     * hard acceleration, entirely adequate. */
    bool saturated = (out != unclamped);
    bool unwinding = (out >= pi->out_max && error < 0.0f) ||
                     (out <= pi->out_min && error > 0.0f);

    if (!saturated || unwinding) {
        pi->integ += pi->ki * error * dt;
    }
    pi->clamped = saturated;

    return out;
}

/* -------------------------------------------------------------------------
 * Transforms
 * ------------------------------------------------------------------------- */

void foc_clarke(float ia, float ib, float *alpha, float *beta)
{
    /* Amplitude-invariant Clarke. ic is not passed because ia + ib + ic == 0,
     * so it carries no information the other two don't already have. Measuring
     * all three anyway is still worth doing — that redundancy IS the Phase 1
     * sanity check (see docs/BUILD_PHASES.md). */
    *alpha = ia;
    *beta  = (ia + 2.0f * ib) * ONE_OVER_S3;
}

void foc_park(float alpha, float beta, float sin_t, float cos_t,
              float *d, float *q)
{
    *d =  alpha * cos_t + beta * sin_t;
    *q = -alpha * sin_t + beta * cos_t;
}

void foc_inv_park(float d, float q, float sin_t, float cos_t,
                  float *alpha, float *beta)
{
    *alpha = d * cos_t - q * sin_t;
    *beta  = d * sin_t + q * cos_t;
}

void foc_svpwm(float valpha, float vbeta, float vbus,
               float *duty_a, float *duty_b, float *duty_c)
{
    if (vbus < 1.0f) {
        /* No bus voltage measurement, or the pack is gone. Coast, don't
         * divide by something near zero. */
        *duty_a = *duty_b = *duty_c = 0.5f;
        return;
    }

    /* Inverse Clarke to three phase voltages. */
    float va = valpha;
    float vb = -0.5f * valpha + SQRT3_OVER_2 * vbeta;
    float vc = -0.5f * valpha - SQRT3_OVER_2 * vbeta;

    /* Common-mode injection: shift all three by the midpoint of their extremes.
     * This is what makes it space-vector rather than plain sinusoidal PWM, and
     * it is why you get ~15% more usable bus voltage for free. The shift is
     * common-mode, so it cancels in the line-to-line voltages the motor
     * actually sees. */
    float vmax = va > vb ? (va > vc ? va : vc) : (vb > vc ? vb : vc);
    float vmin = va < vb ? (va < vc ? va : vc) : (vb < vc ? vb : vc);
    float vcom = 0.5f * (vmax + vmin);

    float inv_bus = 1.0f / vbus;
    float da = (va - vcom) * inv_bus + 0.5f;
    float db = (vb - vcom) * inv_bus + 0.5f;
    float dc = (vc - vcom) * inv_bus + 0.5f;

    /* Clamp. Overmodulation is a real technique but it distorts the current
     * waveform and confuses the observer, so it is not something to fall into
     * accidentally. */
    *duty_a = da < 0.0f ? 0.0f : (da > 1.0f ? 1.0f : da);
    *duty_b = db < 0.0f ? 0.0f : (db > 1.0f ? 1.0f : db);
    *duty_c = dc < 0.0f ? 0.0f : (dc > 1.0f ? 1.0f : dc);
}

void foc_sincos(float theta, float *sin_t, float *cos_t)
{
    /* Drop-in point for the G4's CORDIC accelerator. Until profiling says the
     * ISR is tight, libm is clearer and correct. */
    *sin_t = sinf(theta);
    *cos_t = cosf(theta);
}

/* -------------------------------------------------------------------------
 * Per-axle pipeline
 * ------------------------------------------------------------------------- */

void foc_init(foc_axle_t *ax, float kp_i, float ki_i)
{
    ax->pi_d.kp = kp_i;
    ax->pi_d.ki = ki_i;
    ax->pi_d.out_min = -1.0f;
    ax->pi_d.out_max = 1.0f;
    pi_reset(&ax->pi_d);

    ax->pi_id_q.kp = kp_i;
    ax->pi_id_q.ki = ki_i;
    ax->pi_id_q.out_min = -1.0f;
    ax->pi_id_q.out_max = 1.0f;
    pi_reset(&ax->pi_id_q);

    ax->iq_ref = 0.0f;
    ax->id_ref = 0.0f;
    ax->id_meas = 0.0f;
    ax->iq_meas = 0.0f;
    ax->vbus = 0.0f;
    ax->enabled = false;
}

void foc_step(foc_axle_t *ax, float ia, float ib, float theta_e, float vbus,
              float dt, float *duty_a, float *duty_b, float *duty_c)
{
    if (!ax->enabled) {
        /* 0.5 on all three is zero differential voltage — the motor sees
         * nothing. Not the same as gates off; `inverter_disable()` does that. */
        *duty_a = *duty_b = *duty_c = 0.5f;
        pi_reset(&ax->pi_d);
        pi_reset(&ax->pi_id_q);
        return;
    }

    float sin_t, cos_t;
    foc_sincos(theta_e, &sin_t, &cos_t);

    float alpha, beta;
    foc_clarke(ia, ib, &alpha, &beta);

    float id, iq;
    foc_park(alpha, beta, sin_t, cos_t, &id, &iq);

    ax->id_meas = id;
    ax->iq_meas = iq;
    ax->vbus = vbus;

    /* Current loops. Outputs are normalized voltages, -1 .. 1, scaled to the
     * bus in svpwm. Note both loops run every cycle even though id_ref is
     * usually zero — holding id AT zero is active work, not the absence of it,
     * and a nonzero measured id is the clearest single sign that the observer
     * angle is wrong. Watch it in telemetry. */
    float vd_n = pi_update(&ax->pi_d,    ax->id_ref - id, dt);
    float vq_n = pi_update(&ax->pi_id_q, ax->iq_ref - iq, dt);

    /* Circle limit: the two axes share one voltage budget, and torque loses.
     * Without this you can command a total vector longer than the bus can
     * produce, and svpwm's clamp distorts it in a way the current loops then
     * fight. Preserving vd over vq is deliberate — losing flux control is
     * worse than losing a little torque. */
    float mag_sq = vd_n * vd_n + vq_n * vq_n;
    if (mag_sq > 1.0f) {
        float vd_sq = vd_n * vd_n;
        if (vd_sq >= 1.0f) {
            vd_n = vd_n > 0.0f ? 1.0f : -1.0f;
            vq_n = 0.0f;
        } else {
            float vq_max = sqrtf(1.0f - vd_sq);
            vq_n = vq_n > 0.0f ? vq_max : -vq_max;
        }
    }

    float valpha, vbeta;
    foc_inv_park(vd_n * vbus, vq_n * vbus, sin_t, cos_t, &valpha, &vbeta);

    foc_svpwm(valpha, vbeta, vbus, duty_a, duty_b, duty_c);
}
