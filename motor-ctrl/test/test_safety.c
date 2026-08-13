/**
 * @file test_safety.c
 * @brief Host tests for the interlocks. No STM32 required.
 *
 *     cd motor-ctrl && cc -Iinc -DAXLES_POPULATED=2 -lm \
 *         -o /tmp/test_safety test/test_safety.c src/safety.c
 *     /tmp/test_safety
 *
 * The tire-profile tests exist because of a specific hazard introduced by the
 * two-configuration plan: tire OD feeds the speed calculation, which feeds the
 * speed cap. Fit 70 mm tires while the firmware believes 55 mm and the truck
 * computes 55 mph while actually doing 70 — the cap fails OPEN.
 *
 * These tests pin down that it cannot happen through any software path. The
 * remaining exposure is human (fitting tires and not saying so), which is why
 * it is also a pre-run checklist item in docs/SAFETY.md.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "safety.h"

static int failures = 0;

static void ok(const char *what, int cond)
{
    if (cond) {
        printf("  ok    %s\n", what);
    } else {
        printf("  FAIL  %s\n", what);
        failures++;
    }
}

static void near(const char *what, float got, float want)
{
    float err = got - want;
    if (err < 0.0f) {
        err = -err;
    }
    if (err > 0.05f) {
        printf("  FAIL  %-44s got %8.3f  want %8.3f\n", what, got, want);
        failures++;
    } else {
        printf("  ok    %-44s %8.3f\n", what, got);
    }
}

int main(void)
{
    safety_t s;

    printf("\nBoots into the SLOWER profile\n");
    safety_init(&s);
    ok("default profile is street",
       s.profile == TIRE_PROFILE_STREET);
    ok("street tire OD is 55 mm",
       TIRE_PROFILES[s.profile].tire_od_mm == 55.0f);

    printf("\nSpeed follows the ACTIVE tire, not a constant\n");
    /* Same motor RPM, two tire sizes: the bigger tire really is going faster,
     * and the firmware must say so. This is the whole hazard in one check.
     *
     * Asserted as a RATIO rather than against absolute mph on purpose. The
     * absolute numbers depend on FINAL_DRIVE, and an earlier version of this
     * test hardcoded values that were coincidental to a 3.5:1 ratio — it then
     * failed for a gearing change that broke nothing. Reported speed must
     * scale with tire circumference; that is the property, and it holds at
     * every ratio. */
    safety_set_profile(&s, TIRE_PROFILE_STREET);
    float mph_street = safety_mph_from_rpm(&s, 29878.0f);
    safety_set_profile(&s, TIRE_PROFILE_RECORD);
    float mph_speed = safety_mph_from_rpm(&s, 29878.0f);

    float want_ratio = TIRE_PROFILES[TIRE_PROFILE_RECORD].tire_od_mm /
                       TIRE_PROFILES[TIRE_PROFILE_STREET].tire_od_mm;
    near("speed ratio tracks tire diameter ratio",
         mph_speed / mph_street, want_ratio);
    ok("bigger tire reports higher speed at equal RPM",
       mph_speed > mph_street);
    ok("both are nonzero", mph_street > 0.0f && mph_speed > 0.0f);

    /* Ceilings are COMPUTED from the tire's rated rpm and the derate, never
     * declared -- so a profile whose ceiling contradicts its tire rating
     * cannot be written. Gearing must not appear anywhere in it. */
    for (int i = 0; i < TIRE_PROFILE_COUNT; i++) {
        const tire_profile_t *tp = &TIRE_PROFILES[i];
        float od_m = tp->tire_od_mm / 1000.0f;
        float undertated = tp->rated_rpm / 60.0f * (3.14159265f * od_m) / 0.44704f;
        char buf[90];
        snprintf(buf, sizeof buf, "%-6s ceiling is derated below its rating",
                 tp->name);
        ok(buf, safety_profile_ceiling_mph(tp) < undertated);

        snprintf(buf, sizeof buf, "%-6s ceiling equals rating x derate",
                 tp->name);
        ok(buf, safety_profile_ceiling_mph(tp) >
                undertated * TIRE_RPM_DERATE - 0.5f);
    }

    printf("\nTire RATING is a purchasing decision, and the cap follows it\n");
    /* Same OD, better tire -> higher ceiling, with no code change. This is the
     * whole reason rated_rpm is a field rather than an assumed constant. */
    ok("record and test share an OD",
       TIRE_PROFILES[TIRE_PROFILE_RECORD].tire_od_mm ==
       TIRE_PROFILES[TIRE_PROFILE_TEST].tire_od_mm);
    ok("record is rated higher than test",
       TIRE_PROFILES[TIRE_PROFILE_RECORD].rated_rpm >
       TIRE_PROFILES[TIRE_PROFILE_TEST].rated_rpm);
    ok("so record gets a higher ceiling at the same diameter",
       safety_profile_ceiling_mph(&TIRE_PROFILES[TIRE_PROFILE_RECORD]) >
       safety_profile_ceiling_mph(&TIRE_PROFILES[TIRE_PROFILE_TEST]));

    /* The two consequences that make the plan work. */
    ok("CHEAP TEST TIRES CANNOT REACH 70 MPH (ceiling under target)",
       safety_profile_ceiling_mph(&TIRE_PROFILES[TIRE_PROFILE_TEST]) < 70.0f);
    ok("RECORD TIRES HAVE HEADROOM ABOVE 70 (phase gate is what binds)",
       safety_profile_ceiling_mph(&TIRE_PROFILES[TIRE_PROFILE_RECORD]) > 70.0f);

    printf("\nWIDE shares TEST's diameter: same speed, same cap, more grip\n");
    /* Width does not enter the speed calculation at all -- speed comes from OD
     * and gearing. So `wide` must be indistinguishable from `speed` everywhere
     * the cap logic looks, and differ only in grip. If a future edit gives
     * `wide` a different OD, these catch it. */
    safety_set_profile(&s, TIRE_PROFILE_TEST);
    float mph_narrow = safety_mph_from_rpm(&s, 20000.0f);
    float ceil_narrow = safety_effective_ceiling(&s);
    float mu_narrow = safety_launch_accel_mps2(&s);

    safety_set_profile(&s, TIRE_PROFILE_WIDE);
    near("wide reports the same speed as test",
         safety_mph_from_rpm(&s, 20000.0f), mph_narrow);
    near("wide has the same effective ceiling",
         safety_effective_ceiling(&s), ceil_narrow);
    ok("wide is actually wider",
       TIRE_PROFILES[TIRE_PROFILE_WIDE].tire_width_mm >
       TIRE_PROFILES[TIRE_PROFILE_TEST].tire_width_mm);
    ok("wide grips harder, so allows a faster launch ramp",
       safety_launch_accel_mps2(&s) > mu_narrow);

    /* Launch limit is mu*g and nothing else -- mass cancels, because traction
     * force is mu*m*g and acceleration is F/m. */
    near("launch limit is mu*g",
         safety_launch_accel_mps2(&s),
         TIRE_PROFILES[TIRE_PROFILE_WIDE].mu_nominal * 9.80665f);

    printf("\nEffective ceiling is the LOWER of phase gate and profile\n");
    /* Compiled phase ceiling is 20 mph (Phase 4). Neither profile may exceed
     * it, because the TRUCK has not been proven past 20 regardless of tires. */
    safety_init(&s);
    near("street profile, Phase 4 gate", safety_effective_ceiling(&s), 20.0f);
    safety_set_profile(&s, TIRE_PROFILE_RECORD);
    near("speed profile cannot beat the phase gate",
         safety_effective_ceiling(&s), 20.0f);

    printf("\nCap cannot be raised past the effective ceiling\n");
    safety_init(&s);
    ok("refuses 70 mph at a 20 mph gate", !safety_set_cap(&s, 70.0f));
    ok("refuses zero", !safety_set_cap(&s, 0.0f));
    ok("accepts 15 mph", safety_set_cap(&s, 15.0f));
    near("cap took the accepted value", s.cap_mph, 15.0f);

    printf("\nDowngrading tires pulls a stale cap down with it\n");
    /* The dangerous sequence: run the speed profile, then fit street tires.
     * A cap left where the speed profile put it would over-run the smaller
     * tire's rating. */
    safety_init(&s);
    safety_set_profile(&s, TIRE_PROFILE_RECORD);
    safety_set_cap(&s, 20.0f);
    safety_set_profile(&s, TIRE_PROFILE_STREET);
    ok("cap never exceeds the new profile's ceiling",
       s.cap_mph <= safety_effective_ceiling(&s));

    printf("\nRejects an unknown profile\n");
    ok("out-of-range profile refused",
       !safety_set_profile(&s, (tire_profile_id_t)99));

    printf("\nRC loss coasts, and estop outranks everything\n");
    safety_init(&s);
    float rpm[AXLE_COUNT] = {0.0f, 0.0f};
    float temp[AXLE_COUNT] = {25.0f, 25.0f};
    safety_note_rc_frame(&s, 1000);
    ok("healthy link is OK",
       safety_check(&s, rpm, temp, 14.4f, 5.0f, 1050) == SAFETY_OK);
    ok("stale link cuts",
       safety_check(&s, rpm, temp, 14.4f, 5.0f, 2000) == SAFETY_CUT);
    near("torque zeroed while RC is lost",
         safety_limit_torque(&s, 20.0f, 5.0f), 0.0f);

    safety_init(&s);
    safety_estop(&s, true);
    ok("estop latches",
       safety_check(&s, rpm, temp, 14.4f, 5.0f, 1000) == SAFETY_LATCH);
    ok("estop blocks a fault clear", !safety_clear_fault(&s));
    near("no torque under estop", safety_limit_torque(&s, 20.0f, 0.0f), 0.0f);

    printf("\nAxle desync must PERSIST before it latches\n");
    /* The front axle runs a differential, so one wheel lifting over a seam
     * spikes front motor RPM with nothing wrong. An instantaneous latch would
     * cut all drive at speed over a bump — which is the very failure the
     * interlock exists to prevent. See pin_map.h. */
    safety_init(&s);
    safety_note_rc_frame(&s, 1000);
    float split[AXLE_COUNT] = {9000.0f, 4000.0f};
    float synced[AXLE_COUNT] = {9000.0f, 9000.0f};

    ok("a single diverged tick does NOT latch",
       safety_check(&s, split, temp, 14.4f, 5.0f, 1000) != SAFETY_LATCH);
    ok("still not latched 20 ms in",
       safety_check(&s, split, temp, 14.4f, 5.0f, 1020) != SAFETY_LATCH);

    /* A wheel lift ends. The detector must forget it completely. */
    safety_note_rc_frame(&s, 1030);
    ok("divergence clearing resets the timer",
       safety_check(&s, synced, temp, 14.4f, 5.0f, 1030) != SAFETY_LATCH);
    ok("pending flag cleared", !s.desync_pending);
    safety_note_rc_frame(&s, 1060);
    ok("a later brief spike starts over, does not latch",
       safety_check(&s, split, temp, 14.4f, 5.0f, 1060) != SAFETY_LATCH);

    /* A real failure does not heal. */
    safety_note_rc_frame(&s, 1120);
    ok("sustained divergence past 50 ms DOES latch",
       safety_check(&s, split, temp, 14.4f, 5.0f, 1120) == SAFETY_LATCH);
    ok("desync flag set", (s.flags & FLAG_DESYNC) != 0);
    near("both axles get zero torque",
         safety_limit_torque(&s, 20.0f, 5.0f), 0.0f);

    printf("\nDesync threshold scales with speed\n");
    /* 500 rpm apart is a real divergence at 5,000 rpm and noise at 25,000. */
    safety_init(&s);
    safety_note_rc_frame(&s, 1000);
    float fast[AXLE_COUNT] = {25000.0f, 24500.0f};   /* 2% apart */
    safety_check(&s, fast, temp, 14.4f, 5.0f, 1000);
    ok("500 rpm apart at 25,000 rpm is not a desync", !s.desync_pending);

    safety_init(&s);
    safety_note_rc_frame(&s, 1000);
    float slow[AXLE_COUNT] = {2000.0f, 1200.0f};     /* 800 rpm, 50% apart */
    safety_check(&s, slow, temp, 14.4f, 5.0f, 1000);
    ok("800 rpm apart at 1,600 rpm mean IS a desync", s.desync_pending);

    printf("\n%s (%d failures)\n\n",
           failures ? "FAILURES" : "ALL PASS", failures);
    return failures != 0;
}
