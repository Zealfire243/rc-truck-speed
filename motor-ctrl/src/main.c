/**
 * @file main.c
 * @brief Entry point and the control ISR skeleton.
 *
 * Two execution contexts, and the split between them is the whole design:
 *
 *   CONTROL ISR    20 kHz, highest priority, 50 us budget.
 *                  Current sense -> observer -> FOC -> inverter, twice (both
 *                  axles). Plus the safety checks. Nothing else. No printf,
 *                  no JSON, no malloc, no blocking.
 *
 *   MAIN LOOP      Everything slow. Telemetry serialization, command
 *                  dispatch, the RC drain, housekeeping.
 *
 * Anything that creeps from the second into the first shows up as `overrun` in
 * the telemetry flags, which is the canary — watch it, don't ignore it.
 *
 * Build selection: BRINGUP_APP is the default so a fresh clone runs the Phase 1
 * bench app rather than something that expects a whole vehicle. See
 * docs/BUILD_PHASES.md.
 */

#include <stdbool.h>
#include <stdint.h>

#include "current_sense.h"
#include "foc.h"
#include "inverter.h"
#include "link.h"
#include "observer.h"
#include "pin_map.h"
#include "protocol.h"
#include "rc_input.h"
#include "safety.h"
#include "speed_loop.h"

/* -------------------------------------------------------------------------
 * Global state. Static, not heap — this is a control loop.
 * ------------------------------------------------------------------------- */
static csense_t      g_csense;
static observer_t    g_obs[AXLE_COUNT];
static foc_axle_t    g_foc[AXLE_COUNT];
static speed_loop_t  g_speed;
static safety_t      g_safety;

static volatile uint32_t g_tick;
static volatile uint32_t g_ms;

#ifdef BRINGUP_APP
void bringup_init(void);
void bringup_tick(foc_axle_t axles[AXLE_COUNT], observer_t obs[AXLE_COUNT],
                  uint32_t tick);
#endif

/* -------------------------------------------------------------------------
 * The control ISR. Called on every TIM1 update, immediately after the injected
 * ADC conversions complete.
 *
 * ORDER MATTERS. Current is sampled before anything else because everything
 * else depends on it; safety runs before torque is applied, not after.
 * ------------------------------------------------------------------------- */
void control_isr(void)
{
    g_tick++;

    csense_read(&g_csense);

    float rpm[AXLE_COUNT] = {0};
    for (int a = 0; a < AXLES_POPULATED; a++) {
        rpm[a] = observer_rpm(&g_obs[a]);
    }

    float mph = rpm[AXLE_REAR] * RPM_TO_MPH;

    /* Safety before torque, every tick, no exceptions. */
    safety_action_t act = safety_check(&g_safety, rpm, g_csense.temp_c,
                                       g_csense.vbus, mph, g_ms);
    if (act == SAFETY_LATCH) {
        inverter_disable_all();
        return;
    }

#ifdef BRINGUP_APP
    bringup_tick(g_foc, g_obs, g_tick);
#else
    if ((g_tick % POLICY_LOOP_DIV) == 0) {
        const rc_state_t *rc = rc_input_get();
        speed_loop_policy(&g_speed, rpm, rc->valid ? rc->throttle : 0.0f,
                          CONTROL_DT * POLICY_LOOP_DIV);
    }
    if ((g_tick % VELOCITY_LOOP_DIV) == 0) {
        speed_loop_step(&g_speed, g_foc, rpm, CONTROL_DT * VELOCITY_LOOP_DIV);
    }
#endif

    /* Current loop, both axles. */
    for (int a = 0; a < AXLES_POPULATED; a++) {
        g_foc[a].iq_ref = safety_limit_torque(&g_safety, g_foc[a].iq_ref, mph);

        float alpha, beta;
        foc_clarke(g_csense.ia[a], g_csense.ib[a], &alpha, &beta);

        float theta = observer_step(&g_obs[a], alpha, beta,
                                    0.0f, 0.0f, CONTROL_DT);

        float da, db, dc;
        foc_step(&g_foc[a], g_csense.ia[a], g_csense.ib[a], theta,
                 g_csense.vbus, CONTROL_DT, &da, &db, &dc);

        inverter_set_duty((axle_id_t)a, da, db, dc);
    }

    /* Snapshot for the telemetry task. Cheap by design. */
    if ((g_tick % (PWM_FREQ_HZ / TLM_DEFAULT_HZ)) == 0) {
        tlm_snapshot_t snap = {
            .ts_ms = g_ms,
            .vbat  = g_csense.vbus,
            .mph   = mph,
            .flags = g_safety.flags,
            .mode  = 1,
        };
        for (int a = 0; a < AXLE_COUNT; a++) {
            snap.cmd[a]    = g_foc[a].iq_ref;
            snap.rpm[a]    = rpm[a];
            snap.iq[a]     = g_foc[a].iq_meas;
            snap.id[a]     = g_foc[a].id_meas;
            snap.temp_c[a] = g_csense.temp_c[a];
        }
        link_publish(&snap);
    }
}

int main(void)
{
    /* Order is deliberate: everything is initialized DISABLED, calibration
     * happens before anything can move, and gates are enabled last. */
    inverter_init();
    link_init(PROTO_LINK_BAUD);
    rc_input_init();
    safety_init(&g_safety);
    speed_loop_init(&g_speed);

    csense_init(&g_csense, 0.001f /* MEASURE this — see current_sense.h */);

    for (int a = 0; a < AXLE_COUNT; a++) {
        foc_init(&g_foc[a], 0.5f, 500.0f);
        observer_init(&g_obs[a], 0.05f, 0.00002f, POLE_PAIRS);
    }

    /* Gates are off. Motor must be at rest. If this fails, do not proceed —
     * an uncalibrated offset is phantom torque and unbounded observer drift. */
    if (!csense_calibrate(&g_csense)) {
        link_event("err", "current sense calibration failed");
        /* Deliberately keep running with gates disabled rather than spinning
         * on a motor whose current we cannot measure. */
    }

    link_send_hello();

#ifdef BRINGUP_APP
    bringup_init();
#endif

    for (;;) {
        link_task_step();
        /* TODO: RC drain, housekeeping, watchdog kick. */
    }
}
