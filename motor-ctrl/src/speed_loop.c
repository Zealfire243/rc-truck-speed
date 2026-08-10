/**
 * @file speed_loop.c
 * @brief Velocity control, torque split, traction, launch.
 *
 * Partially implemented: the velocity loop and the torque split are real, the
 * slip estimator is a stub because it cannot be written honestly before Phase 5
 * produces the GPS correlation data it must be calibrated against. See
 * speed_loop.h.
 */

#include "speed_loop.h"

#include "pin_map.h"

void speed_loop_init(speed_loop_t *sl)
{
    for (int a = 0; a < AXLE_COUNT; a++) {
        sl->pi_vel[a].kp = 0.002f;
        sl->pi_vel[a].ki = 0.01f;
        sl->pi_vel[a].out_min = -IQ_LIMIT_A;
        sl->pi_vel[a].out_max = IQ_LIMIT_A;
        pi_reset(&sl->pi_vel[a]);
        sl->rpm_setpoint[a] = 0.0f;
    }
    sl->ff = 0.0f;
    sl->rpm_target = 0.0f;
    sl->accel_limit = 20000.0f;   /* RPM/s. Lower it for launch control. */
    sl->split_bias = 0.0f;
    sl->tc_gain = 0.0f;           /* traction control OFF until Phase 6 */
    sl->tc_deadband = 0.05f;
    sl->slip_est = 0.0f;
    sl->slip_gps = 0.0f;
    sl->closed_loop = false;
}

void speed_loop_step(speed_loop_t *sl, foc_axle_t axles[AXLE_COUNT],
                     const float rpm[AXLE_COUNT], float dt)
{
    if (!sl->closed_loop) {
        return;   /* open-loop apps write iq_ref directly */
    }
    for (int a = 0; a < AXLES_POPULATED; a++) {
        float err = sl->rpm_setpoint[a] - rpm[a];
        float iq = pi_update(&sl->pi_vel[a], err, dt);
        iq += sl->ff * sl->rpm_setpoint[a];
        axles[a].iq_ref = iq;
    }
}

void speed_loop_policy(speed_loop_t *sl, const float rpm[AXLE_COUNT],
                       float throttle, float dt)
{
    /* Slew the target. This IS launch control — see speed_loop.h. */
    float want = throttle * (float)(SPEED_CAP_CEILING_MPH / RPM_TO_MPH);
    float max_step = sl->accel_limit * dt;
    float delta = want - sl->rpm_target;
    if (delta > max_step) {
        delta = max_step;
    } else if (delta < -max_step) {
        delta = -max_step;
    }
    sl->rpm_target += delta;

    sl->slip_est = speed_loop_estimate_slip(sl, rpm);

    /* Traction control. Disabled until Phase 6 (tc_gain starts at 0). */
    float scale = 1.0f;
    if (sl->tc_gain > 0.0f && sl->slip_est > sl->tc_deadband) {
        scale = 1.0f - sl->tc_gain * (sl->slip_est - sl->tc_deadband);
        if (scale < 0.0f) {
            scale = 0.0f;
        }
    }

    /* Torque split. With no mechanical coupling between the axles, this is
     * the differential. */
    float base = sl->rpm_target * scale;
    sl->rpm_setpoint[AXLE_FRONT] = base * 0.5f * (1.0f - sl->split_bias);
    sl->rpm_setpoint[AXLE_REAR]  = base * 0.5f * (1.0f + sl->split_bias);
}

float speed_loop_estimate_slip(const speed_loop_t *sl,
                               const float rpm[AXLE_COUNT])
{
    (void)sl; (void)rpm;
    /* TODO Phase 6, calibrated against Phase 5 GPS logs.
     *
     * The signal to build on is the inter-axle speed difference — a direct
     * consequence of the dual-motor layout, and something a single-motor truck
     * simply cannot measure. Combine with angular acceleration exceeding what
     * the vehicle's mass could plausibly produce. */
    return 0.0f;
}

void speed_loop_note_gps(speed_loop_t *sl, float ground_mph)
{
    /* Logging and calibration only. NEVER in the control path: 10 Hz, and it
     * arrives over a link the architecture explicitly permits to die. */
    sl->slip_gps = ground_mph;
}
