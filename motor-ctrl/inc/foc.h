/**
 * @file foc.h
 * @brief Field-oriented control core — the transforms, the current loops, and
 *        space-vector modulation. One instance per axle.
 *
 * This file is deliberately hardware-free. It takes three phase currents and a
 * rotor angle, and it produces three duty cycles. Where the currents come from
 * (`current_sense.h`), where the angle comes from (`observer.h`), and where the
 * duties go (`inverter.h`) are all somebody else's problem.
 *
 * That separation is the whole reason this project can survive being wrong
 * about the hardware. If a hardware change forces THIS interface to change, the
 * abstraction was wrong — same rule the crawler applied to `motor_driver.h`.
 *
 * ============================================================================
 * WHAT FOC ACTUALLY DOES  (the two-sentence version)
 * ============================================================================
 *
 * A three-phase motor's currents are three sinusoids that shift as the rotor
 * turns, which is miserable to control. FOC rotates the reference frame to spin
 * WITH the rotor, and in that frame the currents become two DC values: `id`
 * (flux, wasted) and `iq` (torque, useful). Then you PI-control two DC values,
 * which is easy, and rotate the answer back.
 *
 * So: hold `id` at zero, command `iq` to whatever torque you want.
 *
 * ============================================================================
 * THE PIPELINE, ONCE PER PWM CYCLE
 * ============================================================================
 *
 *   ia, ib, ic  ──Clarke──►  ialpha, ibeta  ──Park(θ)──►  id, iq
 *                                                            │
 *                                             ┌──────────────┴───────────┐
 *                                             │  PI(id → 0)              │
 *                                             │  PI(iq → iq_ref)         │
 *                                             └──────────────┬───────────┘
 *                                                            ▼
 *   duty_a,b,c  ◄──SVPWM──  valpha, vbeta  ◄──invPark(θ)──  vd, vq
 *
 * Every step is a handful of multiply-adds. The G4 has a single-precision FPU,
 * so floats are fine here — do NOT reach for fixed point until profiling says
 * to. It also has a CORDIC accelerator that computes sin/cos in a few cycles;
 * `foc_sincos()` is the seam where that optimization goes when it's needed.
 *
 * ============================================================================
 * TIMING BUDGET
 * ============================================================================
 *
 *   PWM / current loop      16-20 kHz   ->  50-62 us per cycle
 *   Two axles per ISR                   ->  both pipelines in that window
 *
 * Phase 3's pass criteria include measuring this, not assuming it. Instrument
 * with a GPIO toggle and a scope; `overrun` in the telemetry flags is the
 * runtime canary.
 *
 * ============================================================================
 * SIGN CONVENTION
 * ============================================================================
 *
 * Positive iq = forward torque, on both axles, regardless of how the phases are
 * physically wired. Correct a backwards motor in `pin_map.h` (MOTOR_INVERT_*),
 * never by swapping wires and never in the control loop. See docs/PROTOCOL.md.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * PI controller with clamping anti-windup.
 *
 * Anti-windup matters more here than in most loops: the current loop saturates
 * constantly during acceleration (that's what "full throttle" means), and an
 * integrator that keeps accumulating while the output is pinned will overshoot
 * badly when it finally comes off the rail.
 * ------------------------------------------------------------------------- */
typedef struct {
    float kp;
    float ki;      /**< per second; multiplied by dt internally */
    float integ;   /**< integrator state, in output units       */
    float out_min;
    float out_max;
    bool  clamped; /**< true when the last update hit a rail     */
} pi_t;

void  pi_reset(pi_t *pi);
float pi_update(pi_t *pi, float error, float dt);

/* -------------------------------------------------------------------------
 * Transforms. Free functions so they can be unit-tested on a host without an
 * STM32 anywhere in sight — see host-tools/ for the reference vectors.
 * ------------------------------------------------------------------------- */

/** Clarke: three phase currents -> two-axis stationary frame.
 *
 *  Assumes ia + ib + ic == 0, which is Kirchhoff and not an approximation.
 *  Phase 1's pass criteria check this on real hardware precisely because
 *  everything downstream silently assumes it. */
void foc_clarke(float ia, float ib, float *alpha, float *beta);

/** Park: stationary frame -> rotor frame. `theta` is ELECTRICAL radians. */
void foc_park(float alpha, float beta, float sin_t, float cos_t,
              float *d, float *q);

/** Inverse Park: rotor frame -> stationary frame. */
void foc_inv_park(float d, float q, float sin_t, float cos_t,
                  float *alpha, float *beta);

/** Space-vector modulation via min/max common-mode injection.
 *
 *  Produces the same result as classical sector-based SVPWM with far less
 *  branching: compute the three phase voltages, subtract the midpoint of their
 *  min and max, and normalize. The common-mode term is what buys SVPWM its
 *  ~15% higher bus utilization over naive sinusoidal PWM.
 *
 *  Outputs are duties in 0.0 .. 1.0. */
void foc_svpwm(float valpha, float vbeta, float vbus,
               float *duty_a, float *duty_b, float *duty_c);

/** sin and cos of an electrical angle, together.
 *
 *  Both are always needed at once, and computing them together is where the
 *  G4's CORDIC accelerator drops in. Currently plain libm. */
void foc_sincos(float theta, float *sin_t, float *cos_t);

/* -------------------------------------------------------------------------
 * One axle's FOC state.
 * ------------------------------------------------------------------------- */
typedef struct {
    pi_t  pi_d;
    pi_t  pi_id_q;      /**< named to avoid colliding with the `id` field   */

    float iq_ref;       /**< torque command, amps                          */

    /** Flux command, amps. Zero below base speed.
     *
     *  Negative is FIELD WEAKENING: it opposes the rotor magnet flux, cutting
     *  the effective back-EMF constant so the motor can spin past kV * vbus.
     *  Trades torque (which you don't need at top speed) for RPM (which is
     *  what binds you). Genuinely useful for the last few mph.
     *
     *  !! It is also the one thing here that can destroy the inverter and the
     *  !! pack in a single event. While field weakening at high RPM, back-EMF
     *  !! is being actively suppressed by this loop. Lose that suppression —
     *  !! fault, gate dropout, break input, lost rotor angle — and the
     *  !! unopposed back-EMF exceeds pack voltage and drives current backwards
     *  !! through the body diodes into a pack with nowhere to put it.
     *  !!
     *  !! The fault response while field weakening CANNOT be "gates off";
     *  !! that IS the failure. Unwind id_ref first, then cut.
     *  !!
     *  !! Phase 8 only, and only if the gearing doesn't close without it.
     *  !! See docs/SAFETY.md#field-weakening--the-phase-8-hazard. */
    float id_ref;

    /* Last measured values, for telemetry. */
    float id_meas;
    float iq_meas;
    float vbus;

    bool  enabled;
} foc_axle_t;

void foc_init(foc_axle_t *ax, float kp_i, float ki_i);

/** Run one full current-loop iteration.
 *
 *  @param ia,ib      two measured phase currents (ic is inferred)
 *  @param theta_e    electrical rotor angle, radians, from the observer
 *  @param vbus       measured pack voltage — NOT a constant. Using a nominal
 *                    voltage here means your duty cycles are wrong by however
 *                    much the pack has sagged, which at full throttle is a lot.
 *  @param duty_a/b/c outputs, 0.0 .. 1.0
 */
void foc_step(foc_axle_t *ax, float ia, float ib, float theta_e, float vbus,
              float dt, float *duty_a, float *duty_b, float *duty_c);
