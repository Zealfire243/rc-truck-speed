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

void safety_init(safety_t *s)
{
    s->flags = 0;
    s->estop_latched = false;
    s->fault_latched = false;
    s->iq_limit = IQ_LIMIT_A;
    s->cap_mph = SPEED_CAP_CEILING_MPH;
    s->last_rc_ms = 0;
    s->overrun_count = 0;
}

bool safety_set_cap(safety_t *s, float mph)
{
    /* The whole job of this function is to say no. The compiled ceiling moves
     * one phase gate at a time, in a commit — not from a tuning session and
     * not from a host-side bug. See pin_map.h. */
    if (mph <= 0.0f || mph > SPEED_CAP_CEILING_MPH) {
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
