/**
 * @file observer.h
 * @brief Sensorless rotor angle and speed estimation.
 *
 * ============================================================================
 * THIS IS THE HARD PART OF THE PROJECT
 * ============================================================================
 *
 * FOC needs the electrical rotor angle every single PWM cycle. There is no
 * sensor on this vehicle to provide it, so it has to be inferred from the phase
 * currents and the voltages we commanded. That inference works well at speed
 * and not at all at zero, and bridging that gap is the fiddliest mechanism in
 * the build — the direct analogue of the crawler's dog clutch.
 *
 * Budget real time for Phase 2. It is not a weekend.
 *
 * ============================================================================
 * WHY IT CANNOT WORK AT ZERO SPEED
 * ============================================================================
 *
 * Every sensorless method ultimately observes back-EMF, and back-EMF is
 * proportional to speed. At zero speed there is no signal, only noise. So:
 *
 *   0 .. threshold RPM     forced commutation (open loop, we PICK the angle
 *                          and the rotor follows if there's enough current)
 *   threshold .. max       observer (closed loop, we MEASURE the angle)
 *
 * The handover between those two is where startup failures live. Get it wrong
 * and the motor cogs, stalls, or spins backwards. Phase 2's pass criteria
 * demand 20 clean starts in a row for exactly this reason.
 *
 * (High-frequency injection can find the angle at true zero speed by exploiting
 * rotor saliency. It is a much larger project and this motor may not even be
 * salient enough. Not now.)
 *
 * ============================================================================
 * THE ALGORITHM — flux-linkage observer
 * ============================================================================
 *
 * Preferred over a plain back-EMF observer: it degrades more gracefully at low
 * speed and needs less filtering, and the filtering is what wrecks phase
 * margin at 30,000 rpm.
 *
 * Per axis (alpha and beta), integrate the stator voltage equation:
 *
 *     flux_a = INTEGRAL( v_alpha - R * i_alpha ) dt
 *     flux_b = INTEGRAL( v_beta  - R * i_beta  ) dt
 *
 * Subtract the part of that flux that the stator inductance accounts for; what
 * remains is the permanent magnet's contribution, which points where the rotor
 * points:
 *
 *     pm_a = flux_a - L * i_alpha
 *     pm_b = flux_b - L * i_beta
 *
 *     theta_e = atan2(pm_b, pm_a)
 *
 * Then feed theta_e into a PLL to get a smooth angle and a speed estimate
 * together, rather than differentiating a noisy atan2 (which you must never do
 * at 30,000 rpm):
 *
 *     err    = sin(theta_e - theta_hat)      // small-angle: use the cross
 *                                            // product, no trig needed
 *     omega += ki_pll * err * dt
 *     theta_hat += (omega + kp_pll * err) * dt
 *
 * `omega` out of that PLL is your electrical speed, and it is far cleaner than
 * anything you'd get by differencing angles.
 *
 * ============================================================================
 * THE PART THAT WILL BITE YOU: PURE INTEGRATOR DRIFT
 * ============================================================================
 *
 * That integral is a pure integrator. Any DC offset in the current measurement
 * — and there is always some, which is why `current_sense.h` calibrates —
 * accumulates without bound and walks the estimated angle away from the truth
 * over seconds. It works beautifully on the bench for thirty seconds and then
 * drifts during a real run.
 *
 * Standard fix: replace the pure integrator with a high-pass-limited one,
 * bleeding the state toward zero with a time constant well below the lowest
 * electrical frequency you operate at. You lose a little low-speed accuracy and
 * gain an observer that runs all day.
 *
 * ============================================================================
 * PARAMETERS YOU MUST MEASURE, NOT GUESS
 * ============================================================================
 *
 *   R    phase resistance, ohms. Measure line-to-line with a good meter and
 *        halve it. Datasheet values on hobby motors are frequently fiction.
 *   L    phase inductance, henries. Same.
 *   pp   pole pairs. Count magnets and divide by two, or spin the motor one
 *        mechanical turn by hand and count electrical cycles.
 *
 * R and L wrong by 30% will still spin the motor and will still be wrong. The
 * symptom is a nonzero steady-state `id` in telemetry — see foc.h.
 *
 * Mechanical RPM = omega_electrical * 60 / (2*pi) / pole_pairs
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    /* Motor parameters — MEASURED, not guessed. See the header comment. */
    float r_phase;      /**< ohms                                        */
    float l_phase;      /**< henries                                     */
    uint8_t pole_pairs;

    /* Observer state */
    float flux_a, flux_b;

    /* PLL state */
    float theta_hat;    /**< electrical radians, 0 .. 2pi                */
    float omega_e;      /**< electrical rad/s                            */
    float kp_pll, ki_pll;

    /* Startup */
    bool  locked;          /**< true once the observer has taken over    */
    float ramp_theta;      /**< forced-commutation angle during startup  */
    float ramp_freq_hz;
    float handover_rpm;    /**< mechanical RPM at which to hand over     */

    /* Health */
    float track_err;       /**< PLL error magnitude; the `obs_lost` canary */
} observer_t;

void observer_init(observer_t *o, float r, float l, uint8_t pole_pairs);

/** Run one iteration. Call every PWM cycle, immediately before foc_step().
 *
 *  @param v_alpha,v_beta  the voltages COMMANDED last cycle, not measured.
 *                         Using commanded voltage is standard and fine, but it
 *                         means inverter nonlinearity (deadtime, FET drops)
 *                         shows up as observer error at low duty. If low-speed
 *                         tracking is poor, deadtime compensation is the fix.
 *  @return electrical angle in radians.
 */
float observer_step(observer_t *o, float i_alpha, float i_beta,
                    float v_alpha, float v_beta, float dt);

/** Mechanical RPM from the PLL's electrical speed. */
float observer_rpm(const observer_t *o);

/** True when tracking error has exceeded the threshold for long enough to call
 *  it lost. Wired to the `obs_lost` flag, which cuts that channel — which trips
 *  desync, which cuts both. See safety.h. */
bool observer_lost(const observer_t *o);

void observer_start_ramp(observer_t *o, float start_freq_hz);
