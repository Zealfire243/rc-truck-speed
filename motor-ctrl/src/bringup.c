/**
 * @file bringup.c
 * @brief The Phase 1 bench app. This is the default build.
 *
 * One motor, clamped, NO WHEEL. Forced commutation at a low, settable
 * electrical frequency, with everything published to telemetry so you can look
 * at the current waveforms on a plot.
 *
 * The point of this app is NOT to make the motor spin nicely. It is to let you
 * verify that the current measurements are true, which is Phase 1's entire
 * pass criterion:
 *
 *   1. Currents read zero at rest, after calibration, no drift over a minute.
 *   2. ia + ib + ic ~= 0 at every instant while spinning. Kirchhoff, not an
 *      approximation. `csense_residual()` publishes this.
 *   3. Currents are sinusoidal and smooth, not spiky or clipped.
 *   4. Deadtime verified on a scope on every complementary pair.
 *
 * See docs/BUILD_PHASES.md#phase-1--one-motor-current-sensing-proven.
 *
 * Drive it from the host:
 *
 *     {"t":"set","id":1,"p":"bringup.freq","v":5.0}
 *     {"t":"set","id":2,"p":"bringup.duty","v":0.05}
 *
 * START AT 0.05 DUTY. The current limit on the bench supply is your friend,
 * and a motor that is not turning is a short circuit with an inductor in it.
 */

#include <stdint.h>

#include "foc.h"
#include "inverter.h"
#include "observer.h"
#include "pin_map.h"

/* Settable over the protocol — docs/PROTOCOL.md §4.4. */
float g_bringup_duty = 0.0f;   /* -1 .. 1, still subject to limit.iq */
float g_bringup_freq = 0.0f;   /* forced commutation electrical Hz    */

void bringup_init(void)
{
    /* Deliberately does NOT enable the gates. Enabling happens when you send a
     * nonzero duty, so a board that boots with a motor attached sits still. */
    g_bringup_duty = 0.0f;
    g_bringup_freq = 0.0f;
}

void bringup_tick(foc_axle_t axles[AXLE_COUNT], observer_t obs[AXLE_COUNT],
                  uint32_t tick)
{
    (void)tick;

    /* Phase 1 is one axle only. */
    observer_start_ramp(&obs[AXLE_FRONT], g_bringup_freq);

    if (g_bringup_duty == 0.0f) {
        axles[AXLE_FRONT].enabled = false;
        inverter_disable(AXLE_FRONT);
        return;
    }

    if (!axles[AXLE_FRONT].enabled) {
        if (!inverter_enable(AXLE_FRONT)) {
            return;   /* break latched — do not force it */
        }
        axles[AXLE_FRONT].enabled = true;
    }

    /* Open loop: command a torque current proportional to the requested duty.
     * The current loop still runs, so limit.iq still protects you — which is
     * exactly why this app commands iq rather than writing duties directly. */
    axles[AXLE_FRONT].iq_ref = g_bringup_duty * IQ_LIMIT_A;
    axles[AXLE_FRONT].id_ref = 0.0f;
}
