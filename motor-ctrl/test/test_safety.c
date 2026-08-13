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
     * and the firmware must say so. This is the whole hazard in one check. */
    safety_set_profile(&s, TIRE_PROFILE_STREET);
    float mph_street = safety_mph_from_rpm(&s, 29878.0f);
    safety_set_profile(&s, TIRE_PROFILE_SPEED);
    float mph_speed = safety_mph_from_rpm(&s, 29878.0f);
    near("55 mm tire at motor ceiling", mph_street, 55.0f);
    near("70 mm tire at motor ceiling", mph_speed, 70.0f);
    ok("bigger tire reports higher speed at equal RPM",
       mph_speed > mph_street);

    printf("\nEffective ceiling is the LOWER of phase gate and profile\n");
    /* Compiled phase ceiling is 20 mph (Phase 4). Neither profile may exceed
     * it, because the TRUCK has not been proven past 20 regardless of tires. */
    safety_init(&s);
    near("street profile, Phase 4 gate", safety_effective_ceiling(&s), 20.0f);
    safety_set_profile(&s, TIRE_PROFILE_SPEED);
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
    safety_set_profile(&s, TIRE_PROFILE_SPEED);
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

    printf("\nAxle desync cuts BOTH channels\n");
    safety_init(&s);
    safety_note_rc_frame(&s, 1000);
    float split[AXLE_COUNT] = {9000.0f, 4000.0f};
    ok("divergence past threshold latches",
       safety_check(&s, split, temp, 14.4f, 5.0f, 1000) == SAFETY_LATCH);
    ok("desync flag set", (s.flags & FLAG_DESYNC) != 0);
    near("both axles get zero torque",
         safety_limit_torque(&s, 20.0f, 5.0f), 0.0f);

    printf("\n%s (%d failures)\n\n",
           failures ? "FAILURES" : "ALL PASS", failures);
    return failures != 0;
}
