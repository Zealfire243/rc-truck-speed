/**
 * @file speed_loop.h
 * @brief Vehicle policy: velocity control, torque split, traction, launch.
 *
 * Everything above the current loop and below the RC input. This is where the
 * control code you enjoy writing lives, and it is deliberately the layer that
 * knows nothing about FETs.
 *
 * ============================================================================
 * RATES
 * ============================================================================
 *
 *   velocity loop   2 kHz    per-axle RPM control
 *   policy          400 Hz   traction, launch, split
 *
 * 400 Hz is plenty for policy: the vehicle's own dynamics — tire slip
 * transients, mass, weight transfer — live in the 10-50 ms range, and the CRSF
 * link ahead of it is 2-4 ms. Running policy faster than the thing it is
 * reacting to buys nothing.
 *
 * ============================================================================
 * TORQUE SPLIT — the differential is electronic
 * ============================================================================
 *
 * Two motors on two axles with no mechanical coupling between them means
 * `split.bias` IS the center differential. There is no mechanical fallback if
 * this code is wrong, which is worth remembering.
 *
 *     iq_front = iq_total * 0.5 * (1 - bias)
 *     iq_rear  = iq_total * 0.5 * (1 + bias)
 *
 * Bias 0 is even. Rearward bias helps launch (weight transfers back under
 * acceleration); neutral or slightly forward is more stable at top speed.
 * Making bias a function of speed is a Phase 6 experiment, not a Phase 4
 * feature.
 *
 * ============================================================================
 * TRACTION CONTROL — and why GPS is in the build
 * ============================================================================
 *
 * Slip is the difference between wheel speed and ground speed. The FOC
 * observer gives wheel speed for free, every tick. Ground speed is the hard
 * part, and 10 Hz GPS is far too slow to close a loop against.
 *
 * So GPS is not the traction control input. It is the *calibration* for one:
 *
 *   1. Phase 5: log wheel speed and GPS ground speed together over real runs.
 *   2. Establish where they diverge, and by how much, under what throttle.
 *   3. Build a fast wheel-only slip estimator — inter-axle speed difference,
 *      and angular acceleration exceeding what the vehicle's mass could
 *      plausibly produce — and calibrate it against the logged GPS truth.
 *   4. Phase 6: close the loop on the fast estimator.
 *
 * The inter-axle difference is the useful signal here and it is a direct
 * consequence of the dual-motor layout: if the front axle is turning
 * meaningfully faster than the rear, one of them is slipping, and you know
 * which one by which is accelerating harder than the vehicle is.
 *
 * A single-motor truck cannot do this at all.
 *
 * ============================================================================
 * WHAT THE DIFFERENTIALS DO TO THAT SIGNAL
 * ============================================================================
 *
 * Both axles run gear differentials, so **motor RPM tracks the FASTEST wheel
 * on its axle, not the average.** Design around that, because it cuts both
 * ways:
 *
 *   GOOD   The signal is very sensitive. One wheel breaking loose shows up
 *          immediately in that axle's motor RPM.
 *   BAD    It tells you "a wheel on this axle is slipping", never which wheel,
 *          and never how much the VEHICLE is actually slipping. Wheel-derived
 *          speed therefore always reads high during wheelspin.
 *
 * The consequence for control: an open diff sends torque to the wheel with
 * LESS grip, so one spinning wheel means that axle contributes almost no
 * thrust. Cutting torque to a slipping axle is therefore cheap — you are not
 * giving up much — and shifting it to the other axle via `split.bias` is the
 * right response. That is a real advantage of the dual-motor layout: it can
 * act as a cross-axle limited-slip that no single-motor truck has.
 *
 * Within an axle, the firmware can do nothing; that is what diff oil is for.
 * See docs/MECHANICAL.md.
 *
 * ============================================================================
 * LAUNCH CONTROL
 * ============================================================================
 *
 * `limit.accel` slews the setpoint rather than letting the stick step it. At
 * a standstill a light truck with this much torque simply lights up all four
 * tires, converts the pack into smoke, and goes nowhere. Ramping the setpoint
 * to something the tires can actually transmit is worth more elapsed-time than
 * any amount of extra power.
 *
 * Tune it against the Phase 5 slip data, not by feel.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "foc.h"
#include "pin_map.h"

typedef struct {
    pi_t  pi_vel[AXLE_COUNT];
    float ff;                 /**< feed-forward, torque per RPM of setpoint  */

    float rpm_setpoint[AXLE_COUNT];
    float rpm_target;         /**< pre-slew, from the throttle               */
    float max_rpm;            /**< full-throttle motor RPM. Set by the caller
                               *   from the active cap AND tire profile — never
                               *   from a compile-time constant. */
    float accel_limit;        /**< RPM/s. Launch control lives here.         */

    float split_bias;         /**< -1 all front .. +1 all rear               */

    float tc_gain;
    float tc_deadband;
    float slip_est;           /**< fast wheel-only estimate                  */
    float slip_gps;           /**< slow ground-truth, ESP32-injected         */

    bool  closed_loop;        /**< false in the Phase 1/4 open-loop apps     */
} speed_loop_t;

void speed_loop_init(speed_loop_t *sl);

/** Velocity loop. Call at PWM_FREQ / VELOCITY_LOOP_DIV.
 *  Writes iq_ref into each axle's foc_axle_t. */
void speed_loop_step(speed_loop_t *sl, foc_axle_t axles[AXLE_COUNT],
                     const float rpm[AXLE_COUNT], float dt);

/** Policy layer. Call at PWM_FREQ / POLICY_LOOP_DIV.
 *  Slews the setpoint, applies the split, runs traction control. */
void speed_loop_policy(speed_loop_t *sl, const float rpm[AXLE_COUNT],
                       float throttle, float dt);

/** Fast slip estimate from wheel data alone. See the header — this is what
 *  traction control actually closes on; GPS only calibrates it.
 *
 *  STUB. Needs Phase 5 correlation data before it can be written honestly. */
float speed_loop_estimate_slip(const speed_loop_t *sl,
                               const float rpm[AXLE_COUNT]);

/** Ground speed from the ESP32's GPS, for logging and calibration only.
 *  Never in the control path — it is 10 Hz and it arrives over a link the
 *  architecture explicitly allows to die. */
void speed_loop_note_gps(speed_loop_t *sl, float ground_mph);
