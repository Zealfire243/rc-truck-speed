/**
 * @file test_foc.c
 * @brief Host tests for the FOC transforms. No STM32 required.
 *
 *     cd motor-ctrl && cc -Iinc -lm -o /tmp/test_foc test/test_foc.c src/foc.c
 *     /tmp/test_foc
 *
 * Run these before Phase 1, and again any time you touch foc.c. The transforms
 * are pure math with no hardware dependency, which means every bug in them is
 * findable here in a second — and findable nowhere else once a motor is
 * spinning and the currents look "sort of wrong."
 *
 * This is the cheapest debugging in the entire project. Use it.
 */

#include <math.h>
#include <stdio.h>

#include "foc.h"

#define SQRT3_T 1.7320508075688772f

static int failures = 0;

static void check(const char *what, float got, float want, float tol)
{
    float err = fabsf(got - want);
    if (err > tol) {
        printf("  FAIL  %-38s got %9.5f  want %9.5f\n", what, got, want);
        failures++;
    } else {
        printf("  ok    %-38s %9.5f\n", what, got);
    }
}

/* Balanced three-phase currents at a given electrical angle. */
static void phases(float theta, float amp, float *ia, float *ib, float *ic)
{
    const float T = 2.0f * (float)M_PI / 3.0f;
    *ia = amp * cosf(theta);
    *ib = amp * cosf(theta - T);
    *ic = amp * cosf(theta + T);
}

int main(void)
{
    printf("\nClarke: balanced set must sum to zero and preserve amplitude\n");
    {
        float ia, ib, ic;
        phases(0.7f, 10.0f, &ia, &ib, &ic);
        check("ia+ib+ic", ia + ib + ic, 0.0f, 1e-4f);

        float alpha, beta;
        foc_clarke(ia, ib, &alpha, &beta);
        check("|(alpha,beta)|", sqrtf(alpha * alpha + beta * beta), 10.0f, 1e-3f);
    }

    printf("\nPark at the rotor angle: all current lands on d, none on q\n");
    {
        /* Currents aligned with the rotor should read as pure flux, zero
         * torque. If this fails, the transform pair has a sign error and the
         * motor will produce torque it was not asked for. */
        float theta = 1.1f;
        float ia, ib, ic;
        phases(theta, 10.0f, &ia, &ib, &ic);

        float alpha, beta, s, c, d, q;
        foc_clarke(ia, ib, &alpha, &beta);
        foc_sincos(theta, &s, &c);
        foc_park(alpha, beta, s, c, &d, &q);

        check("d", d, 10.0f, 1e-3f);
        check("q", q, 0.0f, 1e-3f);
    }

    printf("\nPark 90 deg ahead: all current lands on q — pure torque\n");
    {
        float theta = 1.1f;
        float ia, ib, ic;
        phases(theta + (float)M_PI / 2.0f, 10.0f, &ia, &ib, &ic);

        float alpha, beta, s, c, d, q;
        foc_clarke(ia, ib, &alpha, &beta);
        foc_sincos(theta, &s, &c);
        foc_park(alpha, beta, s, c, &d, &q);

        check("d", d, 0.0f, 1e-3f);
        check("q", q, 10.0f, 1e-3f);
    }

    printf("\nPark round trip across a full electrical revolution\n");
    {
        float worst = 0.0f;
        for (int i = 0; i < 360; i++) {
            float theta = (float)i * (float)M_PI / 180.0f;
            float s, c;
            foc_sincos(theta, &s, &c);

            float d0 = 3.0f, q0 = -7.0f;
            float alpha, beta, d1, q1;
            foc_inv_park(d0, q0, s, c, &alpha, &beta);
            foc_park(alpha, beta, s, c, &d1, &q1);

            float e = fmaxf(fabsf(d1 - d0), fabsf(q1 - q0));
            if (e > worst) {
                worst = e;
            }
        }
        check("worst round-trip error over 360 deg", worst, 0.0f, 1e-4f);
    }

    printf("\nSVPWM: duties in range, centered, and correct line-to-line\n");
    {
        float worst_lo = 1.0f, worst_hi = 0.0f, worst_ll = 0.0f;
        const float vbus = 14.4f;

        for (int i = 0; i < 360; i++) {
            float theta = (float)i * (float)M_PI / 180.0f;
            /* Request a vector at 90% of the bus — inside what SVPWM can do,
             * which is 2/sqrt(3) better than naive sinusoidal PWM. */
            float mag = 0.9f * vbus / SQRT3_T;
            float va = mag * cosf(theta);
            float vb = mag * sinf(theta);

            float da, db, dc;
            foc_svpwm(va, vb, vbus, &da, &db, &dc);

            float lo = fminf(da, fminf(db, dc));
            float hi = fmaxf(da, fmaxf(db, dc));
            if (lo < worst_lo) worst_lo = lo;
            if (hi > worst_hi) worst_hi = hi;

            /* The motor only sees line-to-line voltage. Common-mode injection
             * shifts all three duties equally, so it must cancel here — this
             * is the check that the injection is actually common-mode. */
            float vab_want = va - (-0.5f * va + 0.8660254f * vb);
            float vab_got = (da - db) * vbus;
            float e = fabsf(vab_got - vab_want);
            if (e > worst_ll) worst_ll = e;
        }

        check("min duty >= 0", worst_lo >= 0.0f ? 1.0f : 0.0f, 1.0f, 0.5f);
        check("max duty <= 1", worst_hi <= 1.0f ? 1.0f : 0.0f, 1.0f, 0.5f);
        check("worst line-to-line error (V)", worst_ll, 0.0f, 1e-3f);
    }

    printf("\nPI anti-windup: integrator must not run away while saturated\n");
    {
        pi_t pi = {.kp = 1.0f, .ki = 100.0f, .out_min = -1.0f, .out_max = 1.0f};
        pi_reset(&pi);

        /* Hold a large error for a long time — output pinned the whole way. */
        for (int i = 0; i < 1000; i++) {
            (void)pi_update(&pi, 10.0f, 0.001f);
        }
        check("clamped flag set", pi.clamped ? 1.0f : 0.0f, 1.0f, 0.5f);

        /* Now reverse the error. A wound-up integrator would take many steps
         * to come off the rail; a properly clamped one responds immediately. */
        float out = pi_update(&pi, -10.0f, 0.001f);
        check("recovers immediately on sign flip", out, -1.0f, 1e-3f);
    }

    printf("\n%s (%d failure%s)\n\n",
           failures == 0 ? "ALL PASS" : "FAILURES",
           failures, failures == 1 ? "" : "s");
    return failures != 0;
}
