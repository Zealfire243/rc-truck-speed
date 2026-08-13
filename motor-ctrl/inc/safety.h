/**
 * @file safety.h
 * @brief Every interlock, in one place, owned by the G4.
 *
 * The ESP32 cannot command torque, cannot raise the speed cap, and cannot clear
 * a fault latch. Everything that makes this vehicle safe lives here, on the
 * chip that owns the gates. See docs/ARCHITECTURE.md.
 *
 * ============================================================================
 * THE LAYERS, FASTEST FIRST
 * ============================================================================
 *
 *   1. HARDWARE   comparator -> TIM1/TIM8 break input. Kills the gates with no
 *                 software involved, which is the point: software protection
 *                 has to survive the software being wrong.
 *   2. ISR        this module, every control tick. Overcurrent margin,
 *                 overtemp, desync, observer loss, speed cap.
 *   3. POLICY     RC timeout, pack voltage, thermal fold-back.
 *
 * Layer 1 is the only one that works when layers 2 and 3 have crashed. Build it
 * first, and test it deliberately — a protection path you have never seen fire
 * is a protection path you do not have.
 *
 * ============================================================================
 * TWO DECISIONS THAT LOOK WRONG AND ARE NOT
 * ============================================================================
 *
 * COAST ON RC LOSS, DON'T BRAKE. A hobby ESC brakes on failsafe because it
 * assumes a car doing 20 mph with a driver watching. At 70 mph on a 190 mm
 * wheelbase, hard braking with no steering authority puts the truck sideways,
 * and sideways at 70 mph is a tumbling object going somewhere unpredictable.
 * Coasting looks undignified and is much safer.
 *
 * CUT BOTH AXLES ON DESYNC, NOT THE FAILING ONE. Half the drivetrain pushing
 * while the other half doesn't, on a short wheelbase at speed, is a spin.
 * Losing all torque is the good outcome. This is why the desync check compares
 * both channels in the same tick rather than each watching itself.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pin_map.h"

/** Condition flags. These are the strings in `tlm.flags` — see
 *  docs/PROTOCOL.md §5. Keep the two lists identical. */
typedef enum {
    FLAG_ESTOP     = 1u << 0,
    FLAG_RC_LOST   = 1u << 1,
    FLAG_DESYNC    = 1u << 2,
    FLAG_OBS_LOST  = 1u << 3,
    FLAG_OC_TRIP   = 1u << 4,
    FLAG_OVERTEMP  = 1u << 5,
    FLAG_CAP       = 1u << 6,
    FLAG_IQ_CLAMP  = 1u << 7,
    FLAG_VBAT_LOW  = 1u << 8,
    FLAG_OVERRUN   = 1u << 9,
} safety_flag_t;

/** What the caller must do about it. Ordered by severity — a check that
 *  returns a higher value wins over one that returns a lower. */
typedef enum {
    SAFETY_OK = 0,
    SAFETY_FOLDBACK,   /**< reduce torque limit, keep running   */
    SAFETY_CUT,        /**< torque to zero, gates stay enabled  */
    SAFETY_LATCH,      /**< gates off, latched until cleared    */
} safety_action_t;

typedef struct {
    uint32_t flags;
    bool     estop_latched;
    bool     fault_latched;

    float    iq_limit;        /**< current ceiling, folded back on heat    */
    float    cap_mph;         /**< runtime cap, clamped to the ceiling     */

    /** Which tires are fitted. Sets the speed calculation AND the cap
     *  ceiling, together, so the two cannot be mismatched. The firmware
     *  cannot detect this — it is a human step. See pin_map.h. */
    tire_profile_id_t profile;

    uint32_t last_rc_ms;
    uint32_t overrun_count;
} safety_t;

void safety_init(safety_t *s);

/** Run every control tick, before torque is applied.
 *
 *  @param rpm       per-axle motor RPM from the observers
 *  @param temp_c    per-axle inverter temperature
 *  @param vbat      pack voltage under load
 *  @param mph       wheel-derived speed
 *  @param now_ms    monotonic milliseconds
 */
safety_action_t safety_check(safety_t *s,
                             const float rpm[AXLE_COUNT],
                             const float temp_c[AXLE_COUNT],
                             float vbat, float mph, uint32_t now_ms);

/** Clamp a torque request against the cap and every active limit. Returns the
 *  value that is actually safe to command. */
float safety_limit_torque(safety_t *s, float iq_req, float mph);

/** Set the runtime cap. Refuses anything above the effective ceiling.
 *  Returns false if refused. */
bool safety_set_cap(safety_t *s, float mph);

/** Select the fitted tire profile. Rebinds both the speed calculation and the
 *  cap ceiling. Lowers `cap_mph` if the new profile's ceiling is below it, so
 *  switching from speed tires to street tires can never leave a stale high cap
 *  in place. Returns false on an unknown profile. */
bool safety_set_profile(safety_t *s, tire_profile_id_t profile);

/** The cap actually in force: the LOWER of the profile's ceiling and the
 *  compiled phase-gate ceiling. Both must permit a speed for it to be allowed.
 *
 *  This is the whole safety argument for profiles. The phase gate says "this
 *  truck has only been proven to 20 mph"; the profile says "these tires are
 *  only good for 55". Neither can override the other. */
float safety_effective_ceiling(const safety_t *s);

/** Convert motor RPM to mph using the ACTIVE profile's tire diameter.
 *  Never use a hardcoded constant for this — see pin_map.h. */
float safety_mph_from_rpm(const safety_t *s, float motor_rpm);

/** Maximum useful acceleration for the fitted tires, m/s^2 — simply mu*g.
 *
 *  Launch control should slew the setpoint no faster than this. Ramping harder
 *  does not accelerate the truck; it just spins the tires, which is slower AND
 *  destroys the tires you are trying to run a record on. Feed this into
 *  `speed_loop.accel_limit` (converted to motor RPM/s) rather than tuning a
 *  slew rate by feel.
 *
 *  Only as good as the profile's `mu_nominal` — an estimate until Phase 4
 *  measures it. */
float safety_launch_accel_mps2(const safety_t *s);

void safety_note_rc_frame(safety_t *s, uint32_t now_ms);
void safety_estop(safety_t *s, bool engaged);
bool safety_clear_fault(safety_t *s);
