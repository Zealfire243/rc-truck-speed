/**
 * @file test_foc_sim.c
 * @brief Closed-loop host test: the real foc_step() driving a simulated motor.
 *
 *     cd motor-ctrl && cc -Iinc -o /tmp/test_foc_sim test/test_foc_sim.c src/foc.c -lm
 *     /tmp/test_foc_sim
 *
 * test_foc.c proves each transform on its own. This proves they work TOGETHER
 * around a plant that can disagree with them: a sign error between Park and
 * inverse Park, a sector error in SVPWM, or a reversed phase order will all
 * pass the per-function tests and then fail here, because the motor model does
 * not share code or conventions with foc.c.
 *
 * What makes the plant independent:
 *   - It works in PHASE coordinates (three currents, three back-EMFs), not the
 *     rotor frame, so nothing in it can quietly agree with a Park bug.
 *   - Its id/iq and torque are computed from the phase currents with their own
 *     sums, not via foc_clarke()/foc_park().
 *   - It sees duty cycles through a pole-voltage model, so SVPWM's common-mode
 *     injection has to cancel for real.
 *
 * What it does NOT prove: the motor constants below are ILLUSTRATIVE. They are
 * the right order of magnitude for a 3650-class motor, not measurements. The
 * real R, L and flux linkage come from VESC Tool in Phase 1a step 3 and from
 * the bench after that. This test is about the controller's structure, not
 * about this motor.
 *
 * Loop timing is realistic: currents are sampled at the start of a PWM period,
 * and the duties computed from them take effect one period later, held flat
 * (zero-order hold). That one-cycle delay is real on the hardware.
 */

#include <math.h>
#include <stdio.h>

#include "foc.h"

#define PI_D 3.14159265358979323846

/* Illustrative motor: 1 pole pair (2-pole 3650), 4S pack. */
#define M_R        0.012    /* phase resistance, ohms            */
#define M_L        10e-6    /* phase inductance, henries         */
#define M_LAMBDA   0.0019   /* PM flux linkage, phase peak, Wb   */
#define M_POLES    1        /* pole pairs                        */

#define PWM_HZ     20000
#define CTRL_DT    (1.0 / PWM_HZ)
#define SUBSTEPS   10       /* plant integration steps per PWM period */

static int failures = 0;

static void check(const char *what, double got, double want, double tol)
{
    double err = fabs(got - want);
    if (err > tol) {
        printf("  FAIL  %-46s got %10.5f  want %10.5f\n", what, got, want);
        failures++;
    } else {
        printf("  ok    %-46s %10.5f\n", what, got);
    }
}

static void check_true(const char *what, int cond)
{
    if (!cond) {
        printf("  FAIL  %s\n", what);
        failures++;
    } else {
        printf("  ok    %s\n", what);
    }
}

/* ------------------------------------------------------------------------
 * Plant: wye-connected surface PM motor, no neutral wire.
 *
 * Phase b lags a by 120 degrees and c leads, matching the sign convention
 * test_foc.c uses for "positive iq is forward torque".
 * ------------------------------------------------------------------------ */
typedef struct {
    double i[3];
    double theta;      /**< true electrical angle, rad  */
    double omega;      /**< electrical rad/s, imposed   */
    double phase[3];   /**< phase offsets of the three windings */
} plant_t;

static void plant_init(plant_t *p, double theta, double omega, int mirrored)
{
    p->i[0] = p->i[1] = p->i[2] = 0.0;
    p->theta = theta;
    p->omega = omega;
    p->phase[0] = 0.0;
    /* mirrored swaps b and c: a reversed phase sequence, the classic
     * mis-wiring. Used as a negative control. */
    p->phase[1] = mirrored ? -2.0 * PI_D / 3.0 :  2.0 * PI_D / 3.0;
    p->phase[2] = mirrored ?  2.0 * PI_D / 3.0 : -2.0 * PI_D / 3.0;
}

static void plant_step(plant_t *p, const double duty[3], double vbus, double dt)
{
    double u[3], un = 0.0;
    for (int x = 0; x < 3; x++) {
        double e = -M_LAMBDA * p->omega * sin(p->theta - p->phase[x]);
        u[x] = duty[x] * vbus - e;       /* pole voltage minus back-EMF */
        un += u[x];
    }
    un /= 3.0;                           /* floating neutral */
    for (int x = 0; x < 3; x++) {
        p->i[x] += dt * ((u[x] - un) - M_R * p->i[x]) / M_L;
    }
    p->theta += p->omega * dt;
}

/* id, iq and torque from the phase currents, by the plant's own sums. */
static void plant_dq(const plant_t *p, double *id, double *iq, double *torque)
{
    double d = 0.0, q = 0.0, t = 0.0;
    for (int x = 0; x < 3; x++) {
        double a = p->theta - p->phase[x];
        d += p->i[x] * cos(a);
        q += p->i[x] * -sin(a);
        t += p->i[x] * -M_LAMBDA * sin(a);   /* i * dpsi/dtheta */
    }
    *id = d * 2.0 / 3.0;
    *iq = q * 2.0 / 3.0;
    *torque = t * M_POLES;
}

/* ------------------------------------------------------------------------
 * Harness
 * ------------------------------------------------------------------------ */
typedef struct {
    double id, iq, torque;      /* true values, averaged over the tail  */
    double peak_iq;             /* worst true iq seen at any cycle       */
    double t_settle_ms;         /* first time |iq - ref| stays < 5%      */
    double duty_lo, duty_hi;
    int    nan_seen;
    int    clamped_seen;
} result_t;

typedef struct {
    double theta0;
    double omega;               /* imposed electrical speed, rad/s       */
    double vbus;
    double iq_ref, id_ref;
    double angle_err;           /* observer error, rad                   */
    int    mirrored;
    double run_ms;
} scenario_t;

static result_t run(const scenario_t *s)
{
    plant_t pl;
    plant_init(&pl, s->theta0, s->omega, s->mirrored);

    /* Gains from the plant, the way the bench procedure will derive them:
     * bandwidth wc, kp = L*wc, ki = R*wc, normalised to the bus because the
     * PI outputs are fractions of vbus. */
    const double wc = 2.0 * PI_D * 1000.0;
    float kp = (float)(M_L * wc / s->vbus);
    float ki = (float)(kp * (M_R / M_L));

    foc_axle_t ax;
    foc_init(&ax, kp, ki);
    ax.enabled = true;
    ax.iq_ref = (float)s->iq_ref;
    ax.id_ref = (float)s->id_ref;

    result_t r = {.t_settle_ms = -1.0, .duty_lo = 1.0, .duty_hi = 0.0};
    double duty_applied[3] = {0.5, 0.5, 0.5};   /* the one-cycle delay slot */

    int cycles = (int)(s->run_ms * 1e-3 * PWM_HZ);
    int tail = cycles / 5;
    double sum_id = 0, sum_iq = 0, sum_t = 0;
    int stay_since = -1;

    for (int k = 0; k < cycles; k++) {
        /* Sample at the start of the period, compute the NEXT duties. */
        float da, db, dc;
        float theta_obs = (float)(pl.theta + s->angle_err);
        foc_step(&ax, (float)pl.i[0], (float)pl.i[1], theta_obs,
                 (float)s->vbus, (float)CTRL_DT, &da, &db, &dc);
        double next[3] = {da, db, dc};

        /* Apply the PREVIOUS period's duties across this period. */
        for (int n = 0; n < SUBSTEPS; n++) {
            plant_step(&pl, duty_applied, s->vbus, CTRL_DT / SUBSTEPS);
        }
        for (int x = 0; x < 3; x++) {
            duty_applied[x] = next[x];
            if (next[x] < r.duty_lo) r.duty_lo = next[x];
            if (next[x] > r.duty_hi) r.duty_hi = next[x];
        }

        double id, iq, t;
        plant_dq(&pl, &id, &iq, &t);
        if (isnan(id) || isnan(iq) || isinf(id) || isinf(iq)) {
            r.nan_seen = 1;
            break;
        }
        if (fabs(iq) > fabs(r.peak_iq)) r.peak_iq = iq;
        if (ax.pi_d.clamped || ax.pi_id_q.clamped) r.clamped_seen = 1;

        if (fabs(iq - s->iq_ref) < 0.05 * fabs(s->iq_ref)) {
            if (stay_since < 0) stay_since = k;
        } else {
            stay_since = -1;
        }
        if (k >= cycles - tail) {
            sum_id += id; sum_iq += iq; sum_t += t;
        }
    }
    if (stay_since >= 0) r.t_settle_ms = stay_since * CTRL_DT * 1e3;
    r.id = sum_id / tail;
    r.iq = sum_iq / tail;
    r.torque = sum_t / tail;
    return r;
}

int main(void)
{
    const double kt = 1.5 * M_POLES * M_LAMBDA;   /* Nm per amp of iq */

    printf("\nLocked rotor, torque step: iq tracks, id stays zero, torque is positive\n");
    {
        scenario_t s = {.theta0 = 0.9, .omega = 0.0, .vbus = 14.8,
                        .iq_ref = 10.0, .run_ms = 20.0};
        result_t r = run(&s);
        check("iq (true, from phase currents)", r.iq, 10.0, 0.05);
        check("id (true)", r.id, 0.0, 0.05);
        check("torque from phase currents (Nm)", r.torque, kt * 10.0, 1e-4);
        check_true("reaches the 5% band at all", r.t_settle_ms >= 0.0);
        check("settling time (ms), must be under 2", r.t_settle_ms, 0.0, 2.0);
        check("iq overshoot (A)", r.peak_iq - 10.0 > 0 ? r.peak_iq - 10.0 : 0.0,
              0.0, 1.5);
        check_true("duties stay inside 0..1", r.duty_lo >= 0.0 && r.duty_hi <= 1.0);
        check_true("no NaN or runaway", !r.nan_seen);
    }

    printf("\nLocked rotor: the phase currents point where the angle says they should\n");
    {
        /* Independent of Clarke/Park: at iq = 10 and angle theta, the closed
         * form is ia = -10 sin(theta), ib = -10 sin(theta - 2pi/3), ... */
        scenario_t s = {.theta0 = 0.9, .omega = 0.0, .vbus = 14.8,
                        .iq_ref = 10.0, .run_ms = 20.0};
        plant_t pl;
        plant_init(&pl, s.theta0, 0.0, 0);
        /* re-run just to read final currents */
        foc_axle_t ax;
        double wc = 2.0 * PI_D * 1000.0;
        float kp = (float)(M_L * wc / s.vbus);
        foc_init(&ax, kp, (float)(kp * (M_R / M_L)));
        ax.enabled = true; ax.iq_ref = 10.0f;
        double dap[3] = {0.5, 0.5, 0.5};
        for (int k = 0; k < 400; k++) {
            float da, db, dc;
            foc_step(&ax, (float)pl.i[0], (float)pl.i[1], (float)pl.theta,
                     (float)s.vbus, (float)CTRL_DT, &da, &db, &dc);
            for (int n = 0; n < SUBSTEPS; n++)
                plant_step(&pl, dap, s.vbus, CTRL_DT / SUBSTEPS);
            dap[0] = da; dap[1] = db; dap[2] = dc;
        }
        check("ia", pl.i[0], -10.0 * sin(0.9), 0.1);
        check("ib", pl.i[1], -10.0 * sin(0.9 - 2.0 * PI_D / 3.0), 0.1);
        check("ic", pl.i[2], -10.0 * sin(0.9 + 2.0 * PI_D / 3.0), 0.1);
        check("ia+ib+ic", pl.i[0] + pl.i[1] + pl.i[2], 0.0, 1e-6);
    }

    printf("\nNegative torque command gives negative torque\n");
    {
        scenario_t s = {.theta0 = 2.0, .omega = 0.0, .vbus = 14.8,
                        .iq_ref = -10.0, .run_ms = 20.0};
        result_t r = run(&s);
        check("iq", r.iq, -10.0, 0.05);
        check("torque (Nm)", r.torque, -kt * 10.0, 1e-4);
    }

    printf("\nSpinning at 20,000 ERPM (back-EMF fighting the loop)\n");
    {
        double w = 20000.0 / 60.0 * 2.0 * PI_D;     /* electrical rad/s */
        scenario_t s = {.theta0 = 0.0, .omega = w, .vbus = 14.8,
                        .iq_ref = 10.0, .run_ms = 30.0};
        result_t r = run(&s);
        check("iq", r.iq, 10.0, 0.3);
        check("id", r.id, 0.0, 0.3);
        check("torque (Nm)", r.torque, kt * 10.0, 0.003);
        check_true("duties stay inside 0..1", r.duty_lo >= 0.0 && r.duty_hi <= 1.0);
    }

    printf("\nSagged pack: the loop still holds torque at 11 V\n");
    {
        /* NOTE: this does NOT prove vbus is used correctly. A closed current
         * loop's integrator absorbs a voltage-scaling error, so a foc_svpwm()
         * that ignored vbus would still pass here (checked by mutation). That
         * bug shows up only as a gain error on the transient, and on the
         * bench as a loop that is stable at 14.8 V and rings at 11 V. */
        double w = 20000.0 / 60.0 * 2.0 * PI_D;
        scenario_t s = {.theta0 = 0.0, .omega = w, .vbus = 11.0,
                        .iq_ref = 10.0, .run_ms = 30.0};
        result_t r = run(&s);
        check("iq", r.iq, 10.0, 0.3);
        check("id", r.id, 0.0, 0.3);
    }

    printf("\nFlux command moves id and leaves iq alone\n");
    {
        double w = 20000.0 / 60.0 * 2.0 * PI_D;
        scenario_t s = {.theta0 = 0.0, .omega = w, .vbus = 14.8,
                        .iq_ref = 6.0, .id_ref = -4.0, .run_ms = 30.0};
        result_t r = run(&s);
        check("id follows id_ref (field-weakening sign)", r.id, -4.0, 0.3);
        check("iq unaffected", r.iq, 6.0, 0.3);
    }

    printf("\nObserver angle error costs torque, and the loop shows it as id\n");
    {
        /* The loop regulates in the OBSERVED frame, so a 20 deg angle error
         * leaves the observed currents perfect and the real ones wrong. The
         * real torque falls by cos(error) and a real d-axis current appears —
         * which is why bringup watches id_meas as the observer-health signal. */
        double w = 20000.0 / 60.0 * 2.0 * PI_D;
        double err = 20.0 * PI_D / 180.0;
        scenario_t s = {.theta0 = 0.0, .omega = w, .vbus = 14.8,
                        .iq_ref = 10.0, .angle_err = err, .run_ms = 30.0};
        result_t r = run(&s);
        check("true torque = kt * 10 * cos(err)", r.torque,
              kt * 10.0 * cos(err), 0.003);
        check_true("true id is clearly nonzero", fabs(r.id) > 2.0);
    }

    printf("\nNEGATIVE CONTROL: this harness must catch a reversed phase order\n");
    {
        /* If this passed with the phases swapped, every result above would be
         * meaningless. A reversed sequence has to defeat torque control. */
        double w = 20000.0 / 60.0 * 2.0 * PI_D;
        scenario_t s = {.theta0 = 0.0, .omega = w, .vbus = 14.8,
                        .iq_ref = 10.0, .mirrored = 1, .run_ms = 30.0};
        result_t r = run(&s);
        int tracked = !r.nan_seen && fabs(r.iq - 10.0) < 1.0 &&
                      fabs(r.id) < 1.0;
        check_true("swapped-phase plant does NOT track iq=10", !tracked);
    }

    printf("\n%s (%d failure%s)\n\n",
           failures == 0 ? "ALL PASS" : "FAILURES",
           failures, failures == 1 ? "" : "s");
    return failures != 0;
}
