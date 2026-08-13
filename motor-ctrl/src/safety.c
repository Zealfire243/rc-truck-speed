/**
 * @file safety.c
 * @brief Interlock implementation. See safety.h for the reasoning.
 *
 * Fully implemented, on purpose. This is the module that most deserves to
 * exist before the hardware does — it is pure logic, it is testable on a host,
 * and it is the one place where "I'll finish it later" is not acceptable.
 */

#include "safety.h"

#include "pin_map.h"

/* The fitted-tire table.
 *
 * Each ceiling is the road speed at which that tire reaches its ~8,600 rpm
 * rating -- 55 mm -> 55.4 mph, 70 mm -> 70.5 mph. They are derived from the
 * TIRE, so changing FINAL_DRIVE does not move them. See pin_map.h. */
const tire_profile_t TIRE_PROFILES[TIRE_PROFILE_COUNT] = {
    [TIRE_PROFILE_STREET] = { "street", 55.0f, 55.0f },
    [TIRE_PROFILE_SPEED]  = { "speed",  70.0f, 70.0f },
};

void safety_init(safety_t *s)
{
    s->flags = 0;
    s->estop_latched = false;
    s->fault_latched = false;
    s->iq_limit = IQ_LIMIT_A;
    s->last_rc_ms = 0;
    s->overrun_count = 0;

    /* Boot into the SLOWER profile. If the human forgets to select one, the
     * failure is "the truck is slower than expected", not "the cap is set for
     * tires that aren't fitted". */
    s->profile = TIRE_PROFILE_DEFAULT;
    s->cap_mph = safety_effective_ceiling(s);
}

float safety_effective_ceiling(const safety_t *s)
{
    /* Both gates must permit a speed. The phase gate says how far the TRUCK
     * has been proven; the profile says what the TIRES are good for. Neither
     * overrides the other — see safety.h. */
    float profile_ceiling = TIRE_PROFILES[s->profile].cap_ceiling_mph;
    return profile_ceiling < SPEED_CAP_CEILING_MPH
               ? profile_ceiling
               : SPEED_CAP_CEILING_MPH;
}

float safety_mph_from_rpm(const safety_t *s, float motor_rpm)
{
    return motor_rpm * RPM_TO_MPH_FOR(TIRE_PROFILES[s->profile].tire_od_mm);
}

bool safety_set_profile(safety_t *s, tire_profile_id_t profile)
{
    if (profile < 0 || profile >= TIRE_PROFILE_COUNT) {
        return false;
    }
    s->profile = profile;

    /* Pull the cap down if the new profile is more restrictive. Going from
     * speed tires to street tires must never leave a 70 mph cap in place over
     * tires rated for 55. Going the other way does NOT auto-raise the cap —
     * that stays an explicit act. */
    float ceiling = safety_effective_ceiling(s);
    if (s->cap_mph > ceiling) {
        s->cap_mph = ceiling;
    }
    return true;
}

bool safety_set_cap(safety_t *s, float mph)
{
    /* The whole job of this function is to say no. The compiled ceiling moves
     * one phase gate at a time, in a commit — not from a tuning session and
     * not from a host-side bug. The profile ceiling moves only by fitting
     * different tires. See pin_map.h. */
    if (mph <= 0.0f || mph > safety_effective_ceiling(s)) {
        return false;
    }
    s->cap_mph = mph;
    return true;
}

void safety_note_rc_frame(safety_t *s, uint32_t now_ms)
{
    s->last_rc_ms = now_ms;
}

void safety_estop(safety_t *s, bool engaged)
{
    s->estop_latched = engaged;
    if (engaged) {
        s->flags |= FLAG_ESTOP;
    } else {
        s->flags &= ~(uint32_t)FLAG_ESTOP;
    }
}

bool safety_clear_fault(safety_t *s)
{
    if (s->estop_latched) {
        return false;   /* estop outranks a clear request */
    }
    s->fault_latched = false;
    s->flags &= ~(uint32_t)(FLAG_DESYNC | FLAG_OBS_LOST | FLAG_OC_TRIP);
    return true;
}

static safety_action_t worse(safety_action_t a, safety_action_t b)
{
    return a > b ? a : b;
}

safety_action_t safety_check(safety_t *s,
                             const float rpm[AXLE_COUNT],
                             const float temp_c[AXLE_COUNT],
                             float vbat, float mph, uint32_t now_ms)
{
    safety_action_t act = SAFETY_OK;

    if (s->estop_latched) {
        return SAFETY_LATCH;
    }
    if (s->fault_latched) {
        return SAFETY_LATCH;
    }

    /* --- RC link ---------------------------------------------------------
     * Coast, don't brake. See the header. */
    if (now_ms - s->last_rc_ms > RC_TIMEOUT_MS) {
        s->flags |= FLAG_RC_LOST;
        act = worse(act, SAFETY_CUT);
    } else {
        s->flags &= ~(uint32_t)FLAG_RC_LOST;
    }

    /* --- Axle desync -----------------------------------------------------
     * Both channels compared in the same tick, and BOTH cut. Never one. */
#if AXLES_POPULATED > 1
    float divergence = rpm[AXLE_FRONT] - rpm[AXLE_REAR];
    if (divergence < 0.0f) {
        divergence = -divergence;
    }
    if (divergence > DESYNC_RPM) {
        s->flags |= FLAG_DESYNC;
        s->fault_latched = true;
        return SAFETY_LATCH;
    }
#else
    (void)rpm;
#endif

    /* --- Thermal ---------------------------------------------------------
     * Fold back before cutting. A run that quietly loses power is recoverable
     * and shows up in the log; a run that cuts out at speed is a crash. */
    bool hot = false, too_hot = false;
    for (int i = 0; i < AXLES_POPULATED; i++) {
        if (temp_c[i] >= OVERTEMP_CUT_C) {
            too_hot = true;
        } else if (temp_c[i] >= OVERTEMP_FOLDBACK_C) {
            hot = true;
        }
    }
    if (too_hot) {
        s->flags |= FLAG_OVERTEMP;
        s->fault_latched = true;
        return SAFETY_LATCH;
    }
    if (hot) {
        s->flags |= FLAG_OVERTEMP;
        /* Linear fold-back across the band between the two thresholds. */
        float hottest = temp_c[0];
        for (int i = 1; i < AXLES_POPULATED; i++) {
            if (temp_c[i] > hottest) {
                hottest = temp_c[i];
            }
        }
        float span = OVERTEMP_CUT_C - OVERTEMP_FOLDBACK_C;
        float frac = 1.0f - (hottest - OVERTEMP_FOLDBACK_C) / span;
        s->iq_limit = IQ_LIMIT_A * (frac < 0.1f ? 0.1f : frac);
        act = worse(act, SAFETY_FOLDBACK);
    } else {
        s->flags &= ~(uint32_t)FLAG_OVERTEMP;
        s->iq_limit = IQ_LIMIT_A;
    }

    /* --- Pack ------------------------------------------------------------
     * Advisory. Cutting power at speed because the pack sagged under the very
     * load you asked for would be worse than the sag. */
    if (vbat < VBAT_LOW_V) {
        s->flags |= FLAG_VBAT_LOW;
    } else {
        s->flags &= ~(uint32_t)FLAG_VBAT_LOW;
    }

    /* --- Speed cap -------------------------------------------------------
     * Not a fault. Expected to be active for most of every phase run. */
    if (mph >= s->cap_mph) {
        s->flags |= FLAG_CAP;
    } else {
        s->flags &= ~(uint32_t)FLAG_CAP;
    }

    return act;
}

float safety_limit_torque(safety_t *s, float iq_req, float mph)
{
    if (s->estop_latched || s->fault_latched) {
        return 0.0f;
    }
    if (s->flags & FLAG_RC_LOST) {
        return 0.0f;   /* coast */
    }

    /* Speed cap. Taper into it rather than hitting a wall — a hard cut at the
     * cap unsettles the truck right at the top of a run, which is the worst
     * possible moment. Taper starts 2 mph below. */
    if (mph > s->cap_mph - 2.0f) {
        float over = (mph - (s->cap_mph - 2.0f)) / 2.0f;
        if (over > 1.0f) {
            over = 1.0f;
        }
        iq_req *= (1.0f - over);
    }

    if (iq_req > s->iq_limit) {
        iq_req = s->iq_limit;
        s->flags |= FLAG_IQ_CLAMP;
    } else if (iq_req < -s->iq_limit) {
        iq_req = -s->iq_limit;
        s->flags |= FLAG_IQ_CLAMP;
    } else {
        s->flags &= ~(uint32_t)FLAG_IQ_CLAMP;
    }

    return iq_req;
}
