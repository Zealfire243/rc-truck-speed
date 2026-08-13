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
    /*                      name       OD    width   rated     mu   */
    [TIRE_PROFILE_STREET] = { "street", 55.0f, 22.0f,  8600.0f, 1.05f },
    [TIRE_PROFILE_TEST]   = { "test",   70.0f, 25.0f,  8600.0f, 1.10f },
    [TIRE_PROFILE_WIDE]   = { "wide",   70.0f, 35.0f,  8600.0f, 1.20f },
    [TIRE_PROFILE_RECORD] = { "record", 70.0f, 25.0f, 12000.0f, 1.10f },
};

/* `wide` shares `speed`'s OD, so it shares its ceiling and its wheel RPM
 * exactly -- width does not enter the speed calculation. It is a separate
 * profile because grip differs, and grip is what the log needs to record and
 * what launch control needs to know. See pin_map.h.
 *
 * mu_nominal values are ESTIMATES. Replace each one with a Phase 4
 * measurement: full throttle from a standstill, a = dv/dt off the GPS trace,
 * mu = a/g. Note that mu is really a (tire, surface) property -- what you
 * store here is the grip of that tire on the surface you measured, which is
 * fine as long as you run the same kind of pavement. */

void safety_init(safety_t *s)
{
    s->flags = 0;
    s->estop_latched = false;
    s->fault_latched = false;
    s->iq_limit = IQ_LIMIT_A;
    s->last_rc_ms = 0;
    s->overrun_count = 0;
    s->desync_pending = false;
    s->desync_since_ms = 0;

    /* Boot into the SLOWER profile. If the human forgets to select one, the
     * failure is "the truck is slower than expected", not "the cap is set for
     * tires that aren't fitted". */
    s->profile = TIRE_PROFILE_DEFAULT;
    s->cap_mph = safety_effective_ceiling(s);
}

float safety_profile_ceiling_mph(const tire_profile_t *p)
{
    /* Computed, never declared — one source of truth, so a profile whose
     * ceiling contradicts its tire rating cannot be written. See pin_map.h. */
    float circ_m = 3.14159265f * p->tire_od_mm / 1000.0f;
    return p->rated_rpm * TIRE_RPM_DERATE / 60.0f * circ_m / 0.44704f;
}

float safety_effective_ceiling(const safety_t *s)
{
    /* Both gates must permit a speed. The phase gate says how far the TRUCK
     * has been proven; the profile says what the TIRES are good for. Neither
     * overrides the other — see safety.h. */
    float profile_ceiling =
        safety_profile_ceiling_mph(&TIRE_PROFILES[s->profile]);
    return profile_ceiling < SPEED_CAP_CEILING_MPH
               ? profile_ceiling
               : SPEED_CAP_CEILING_MPH;
}

float safety_mph_from_rpm(const safety_t *s, float motor_rpm)
{
    return motor_rpm * RPM_TO_MPH_FOR(TIRE_PROFILES[s->profile].tire_od_mm);
}

float safety_launch_accel_mps2(const safety_t *s)
{
    /* a_max = mu * g. Mass cancels: traction force is mu*m*g and a = F/m. */
    return TIRE_PROFILES[s->profile].mu_nominal * 9.80665f;
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
    s->desync_pending = false;
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
     * Both channels compared in the same tick, and BOTH cut. Never one.
     *
     * Threshold is proportional to speed with a floor, and the divergence must
     * PERSIST before it latches — because the front differential lets a single
     * lifted or slipping wheel spike front motor RPM with nothing actually
     * wrong. See pin_map.h for the reasoning and the trade. */
#if AXLES_POPULATED > 1
    float divergence = rpm[AXLE_FRONT] - rpm[AXLE_REAR];
    if (divergence < 0.0f) {
        divergence = -divergence;
    }

    float mag_f = rpm[AXLE_FRONT] < 0.0f ? -rpm[AXLE_FRONT] : rpm[AXLE_FRONT];
    float mag_r = rpm[AXLE_REAR] < 0.0f ? -rpm[AXLE_REAR] : rpm[AXLE_REAR];
    float threshold = DESYNC_FRAC * 0.5f * (mag_f + mag_r);
    if (threshold < DESYNC_RPM_FLOOR) {
        threshold = DESYNC_RPM_FLOOR;
    }

    if (divergence > threshold) {
        if (!s->desync_pending) {
            s->desync_pending = true;
            s->desync_since_ms = now_ms;
        } else if (now_ms - s->desync_since_ms >= DESYNC_PERSIST_MS) {
            s->flags |= FLAG_DESYNC;
            s->fault_latched = true;
            return SAFETY_LATCH;
        }
    } else {
        s->desync_pending = false;
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
