/**
 * @file pin_map.h
 * @brief Single source of truth for every pin and every drivetrain constant.
 *
 * ============================================================================
 * STATUS: PROPOSED, NOT YET WIRED. Expect to revise.
 * ============================================================================
 *
 * Nothing here has been built. Verify every assignment against the STM32G474
 * datasheet and your board's schematic in CubeMX before committing to a
 * harness — the G4's alternate-function matrix is dense and several of these
 * pins have three plausible uses. docs/WIRING.md has the same table with the
 * reasoning attached.
 *
 * ============================================================================
 * THE ARRANGEMENT
 * ============================================================================
 *
 *   TIM1  ->  FRONT axle inverter   (3 complementary pairs + break input)
 *   TIM8  ->  REAR  axle inverter   (3 complementary pairs + break input)
 *   ADC1  ->  front phase currents, pack voltage, temps
 *   ADC2  ->  rear phase currents
 *
 * One chip, both axles, one control ISR, one clock. See docs/ARCHITECTURE.md
 * for why that beats two controllers.
 *
 * !! Both break inputs must be fed by a hardware overcurrent comparator. That
 * !! is what kills the gates when the software is wrong, and "the software is
 * !! wrong" is the case protection exists for. See docs/SAFETY.md.
 */

#pragma once

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Axle indexing. Must match docs/PROTOCOL.md §2 exactly — telemetry arrays
 * are emitted in this order and the host tools rely on it.
 * ------------------------------------------------------------------------- */
typedef enum {
    AXLE_FRONT = 0,
    AXLE_REAR  = 1,
    AXLE_COUNT = 2,
} axle_id_t;

/** Which axle THIS controller drives.
 *
 *  Each controller drives exactly one motor — the vehicle runs two copies of
 *  this firmware, one per axle, coordinated over the peer link. Set at build
 *  time: -DAXLE_SELF=AXLE_FRONT or -DAXLE_SELF=AXLE_REAR.
 *
 *  This replaces AXLES_POPULATED, which counted how many axles one chip drove.
 *  Nothing counts them any more; the answer is always one. What varies is
 *  WHICH one, because it decides peer-link identity and the torque-split sign.
 *
 *  AXLE_COUNT stays 2: the arrays below still hold two axles' worth of STATE
 *  (own, measured + peer, received over the link), which is what the desync
 *  interlock and the telemetry format in docs/PROTOCOL.md §2 both need. Two
 *  axles of data, one axle of drive. */
#ifndef AXLE_SELF
#define AXLE_SELF AXLE_FRONT
#endif

/** The other axle — the one on the far end of the peer link. */
#define AXLE_PEER ((AXLE_SELF) == AXLE_FRONT ? AXLE_REAR : AXLE_FRONT)

/* -------------------------------------------------------------------------
 * Front axle — TIM1
 * ------------------------------------------------------------------------- */
#define FRONT_PWM_TIMER      TIM1
#define PIN_F_PWM_AH         GPIO_PIN_8    /* PA8  */
#define PIN_F_PWM_BH         GPIO_PIN_9    /* PA9  */
#define PIN_F_PWM_CH         GPIO_PIN_10   /* PA10 */
#define PIN_F_PWM_AL         GPIO_PIN_13   /* PB13 */
#define PIN_F_PWM_BL         GPIO_PIN_14   /* PB14 */
#define PIN_F_PWM_CL         GPIO_PIN_15   /* PB15 */
#define PIN_F_BKIN           GPIO_PIN_12   /* PB12 — overcurrent, hardware  */

/* -------------------------------------------------------------------------
 * Rear axle — TIM8
 * ------------------------------------------------------------------------- */
#define REAR_PWM_TIMER       TIM8
#define PIN_R_PWM_AH         GPIO_PIN_6    /* PC6  */
#define PIN_R_PWM_BH         GPIO_PIN_7    /* PC7  */
#define PIN_R_PWM_CH         GPIO_PIN_8    /* PC8  */
#define PIN_R_PWM_AL         GPIO_PIN_7    /* PA7  */
#define PIN_R_PWM_BL         GPIO_PIN_0    /* PB0  */
#define PIN_R_PWM_CL         GPIO_PIN_1    /* PB1  */
#define PIN_R_BKIN           GPIO_PIN_6    /* PB6  — overcurrent, hardware  */

/* -------------------------------------------------------------------------
 * Analog. All six current channels are sampled by INJECTED conversion
 * triggered off the timer update event, so every phase is measured at the
 * same instant in the PWM cycle. See current_sense.h — this is the single
 * most important configuration detail in the whole firmware.
 * ------------------------------------------------------------------------- */
#define ADC_F_IA             1    /* PA0, ADC1_IN1  */
#define ADC_F_IB             2    /* PA1, ADC1_IN2  */
#define ADC_F_IC             3    /* PA2, ADC1_IN3  */
#define ADC_R_IA             17   /* PA4, ADC2_IN17 */
#define ADC_R_IB             13   /* PA5, ADC2_IN13 */
#define ADC_R_IC             3    /* PA6, ADC2_IN3  */
#define ADC_VBUS             4    /* PA3, ADC1_IN4, divider */
#define ADC_TEMP_F           6    /* PC0, ADC1_IN6, thermistor */
#define ADC_TEMP_R           7    /* PC1, ADC1_IN7, thermistor */

/* -------------------------------------------------------------------------
 * Comms and I/O
 * ------------------------------------------------------------------------- */
#define RC_UART              USART3   /* PB10/PB11 — CRSF receiver goes HERE,
                                       * not to the ESP32. docs/ARCHITECTURE.md */
#define LINK_UART            UART4    /* PC10/PC11 — to ESP32, 921600         */
#define PIN_STEER_SERVO      GPIO_PIN_9   /* PC9, TIM3 CH4, 50 Hz             */

/* -------------------------------------------------------------------------
 * Per-axle sign correction.
 *
 * Set to 1 when an axle drives backwards relative to "positive iq = vehicle
 * forward". Fixing polarity HERE rather than by swapping motor phases keeps
 * every downstream consumer on one convention: positive == forward, always.
 * See docs/PROTOCOL.md §2.
 * ------------------------------------------------------------------------- */
#define MOTOR_INVERT_FRONT   0
#define MOTOR_INVERT_REAR    0

/* -------------------------------------------------------------------------
 * Drivetrain constants. MEASURE THESE — the defaults come from Phase 0's
 * gearing solve and your speed readings are only as good as they are.
 *
 * Run `cd host-tools && uv run gearing.py` to regenerate the working point
 * whenever tire diameter, mass, or gearing changes.
 * ------------------------------------------------------------------------- */
#define POLE_PAIRS           1       /* QuicRun 3650 G2 is 2-POLE. VERIFY.   */

/* FINAL_DRIVE — motor turns per wheel turn.
 *
 * The MJX Hyper Go diff's ring-and-pinion IS the entire final drive; the motor
 * drives its bevel pinion directly and there is no spur stage.
 *
 * RING GEAR IS 30T — confirmed from clear product photography, no longer a
 * range. So the input pinion alone now sets the ratio:
 *
 *      10T -> 3.00:1      12T -> 2.50:1
 *      11T -> 2.73:1      13T -> 2.31:1
 *
 * !! 2.31f IS DELIBERATELY THE LOWEST RATIO IN THAT SPACE, NOT THE LIKELY ONE.
 * !!
 * !! Speed is derived as motor_rpm / FINAL_DRIVE (see MPH_PER_MOTOR_RPM below),
 * !! so setting this HIGHER than the true ratio makes every speed reading read
 * !! LOW, and the speed cap therefore engages LATE — it fails OPEN. Enter 3.00
 * !! when the truth is 2.31 and the cap holds at 90.9 mph on a tire rated for
 * !! 70.5. Setting it LOW does the reverse: readings run high, the cap engages
 * !! early, you lose a few mph and stay inside the tire rating.
 * !!
 * !! This is the same fail-open mode as selecting the wrong tire profile, and
 * !! it is why the conservative bound ships instead of the likely value.
 * !!
 * !! SET THIS TO 30.0f/<pinion teeth> AS SOON AS THE PINION IS CHOSEN, then
 * !! re-run `cd host-tools && uv run gearing.py`. 2.31f is only the safe
 * !! placeholder covering the whole 10-13T pinion space, and it costs real top
 * !! speed if the fitted pinion is a 10T. See docs/MECHANICAL.md and
 * !! docs/BUILD_PHASES.md Phase 0.
 */
#define FINAL_DRIVE          2.31f

/* -------------------------------------------------------------------------
 * TIRE PROFILES — the two-configuration plan.
 *
 * One chassis, two identical drive units, one final-drive ratio. Top speed
 * changes by changing wheels. The reason this works, and it is not obvious:
 *
 *   **Wheel RPM is constant across every tire size.** Motor RPM ceiling
 *   divided by FINAL_DRIVE sets it, full stop. Tire diameter only changes how
 *   far the truck travels per revolution.
 *
 * At 2850 kV on 4S with 2.31:1, wheel RPM tops out at 15,101 in EVERY profile
 * (it was 13,954 at the retired 2.50:1 — shorter gearing shrinks it).
 * Chasing 70 mph on a small tire is what pushed wheel RPM to 10,866 and put
 * it 36% over typical 1/12 pan-car duty. Chasing it with a bigger tire and
 * taller gearing does not. Run `gearing.py --swap`.
 *
 * ---------------------------------------------------------------------------
 * THE CAP CEILINGS ARE DERIVED FROM THE TIRE, NOT FROM THE GEARING
 * ---------------------------------------------------------------------------
 *
 * Each profile's ceiling is simply the road speed at which that tire reaches
 * its ~8,600 rpm rating:
 *
 *     55 mm:  8600/60 * (pi*0.055) = 24.8 m/s = 55.4 mph
 *     70 mm:  8600/60 * (pi*0.070) = 31.5 m/s = 70.5 mph
 *
 * That is why they are 55 and 70, and it is why they DO NOT MOVE when the
 * final drive changes. Gearing is therefore free to be chosen on motor
 * longevity rather than on top speed — see below.
 *
 * ---------------------------------------------------------------------------
 * WHY 2.5:1 AND NOT 3.5:1 — ROTATIONAL WEAR
 * ---------------------------------------------------------------------------
 *
 * Acceleration is traction-limited at every ratio from 2.5:1 to 5:1 (0-50 mph
 * is ~2.4 s in all of them), so gearing costs nothing off the line. That frees
 * the ratio to be picked purely on what it does to the motor. At a sustained
 * 70 mph, relative to 3.5:1:
 *
 *     ratio   motor rpm   loss    bearing life   centrifugal stress
 *     2.5:1      21,345   -30%          1.40x                 0.51x
 *     3.5:1      29,882      --          1.00x                 1.00x
 *     5.0:1      42,689   +66%          0.70x                 2.04x
 *
 * Bearing L10 life goes as 1/rpm and magnet-retention stress goes as rpm^2,
 * so both punish a short gear far harder than heat does. 29,882 rpm is also
 * near the practical limit for grease-lubricated small bearings; 21,345 is
 * comfortably inside it.
 *
 * The surplus is deliberate: the 2850 kV motor at 2.5:1 models out to ~111 mph
 * and is capped at 70 by the tire. Running an over-specified motor at 45% of
 * its no-load speed IS the wear reduction. The winding was chosen on motor
 * constant and ERPM, not on required kV -- which is only 1744 here.
 * Run `accel.py --thermal 70`.
 *
 * !! TIRE OD IS NOT A COMPILE-TIME CONSTANT ANYMORE, and that is a safety
 * !! fix, not a convenience. It feeds the speed calculation, which feeds the
 * !! speed cap. Fit 70 mm tires with 55 mm compiled in and the truck computes
 * !! 55 mph while actually doing 70 -- the cap fails OPEN, in the dangerous
 * !! direction. Profile selection binds tire OD and cap ceiling together so
 * !! they cannot be mismatched.
 * !!
 * !! The firmware CANNOT detect what tires are fitted. Selecting the profile
 * !! is a human step on the pre-run checklist. See docs/SAFETY.md.
 * ------------------------------------------------------------------------- */
typedef struct {
    const char *name;
    float tire_od_mm;
    float tire_width_mm;
    float rated_rpm;         /**< manufacturer's max. GET IT IN WRITING.      */
    float mu_nominal;        /**< grip estimate; replace with Phase 4 measurement */
} tire_profile_t;

typedef enum {
    TIRE_PROFILE_STREET = 0,   /**< 55 x 22, cheap, tucks in the stock arches */
    TIRE_PROFILE_TEST   = 1,   /**< 70 x 25, cheap consumable for tuning      */
    TIRE_PROFILE_WIDE   = 2,   /**< 70 x 35, cheap, grip config for tuning    */
    TIRE_PROFILE_RECORD = 3,   /**< 70 x 25, high-rated, for the attempt      */
    TIRE_PROFILE_COUNT  = 4,
} tire_profile_id_t;

/** Never run a tire at its rated ceiling.
 *
 *  Ratings are quoted at some load and temperature; a speed run is high load
 *  (weight transfer plus whatever downforce the spoiler makes) on a tire that
 *  has been heating for the whole run. 10% is a modest, deliberate haircut.
 *
 *  This constant exists because an earlier version of this file DERIVED the
 *  ceilings straight from the rating with no margin at all, putting the record
 *  attempt at 99.3% of rated on the most catastrophic failure mode in the
 *  project. Do not set this to 1.0. */
#define TIRE_RPM_DERATE   0.90f

/* ---------------------------------------------------------------------------
 * WHY WIDTH IS A PROFILE FIELD BUT DOES NOT MOVE THE CAP
 * ---------------------------------------------------------------------------
 *
 * Speed is computed from OD and gearing, so width does not enter it at all.
 * `wide` shares `speed`'s 70 mm OD and therefore its exact ceiling and its
 * exact wheel RPM. Nothing about the speed-cap logic changes.
 *
 * What width changes is GRIP, and grip is the acceleration lever. Rubber and
 * foam do not obey F = mu*N -- elastomer friction carries adhesion and
 * hysteresis terms that make mu load-sensitive, falling as contact pressure
 * rises (roughly mu ~ N^-0.15). A wider tire at the same load runs lower
 * pressure and grips harder.
 *
 * Be honest about the size of that effect though: 22 -> 35 mm is a 59% width
 * increase and buys maybe 5-10% of mu on load sensitivity alone. COMPOUND is
 * the bigger lever. `wide` is specced at mu 1.20 on the assumption of both.
 *
 * So width is here for two real reasons, neither of them the cap:
 *
 *   1. The log has to say which tires produced which run. You will be
 *      measuring mu per tire set in Phase 4 and choosing the final drive from
 *      it (accel.py --match) -- the profile is how a measurement gets attached
 *      to the tires that produced it.
 *   2. mu_nominal gives launch control a tire-aware slew limit. Maximum useful
 *      acceleration is mu*g; ramping the setpoint faster than that just spins
 *      the tires. See safety_launch_accel_mps2().
 *
 * !! DO NOT ASSUME THE WIDE TIRE INHERITS THE NARROW ONE'S RPM RATING. Same
 * !! OD does not mean same rating -- a wider carcass carries more mass at
 * !! radius and sees higher centrifugal stress. Verify it separately, and if
 * !! it comes back lower, drop this profile's cap_ceiling_mph to match.
 * ------------------------------------------------------------------------- */

extern const tire_profile_t TIRE_PROFILES[TIRE_PROFILE_COUNT];

/** Default on boot. The SLOWER profile, deliberately — booting into the fast
 *  config with the pretty tires fitted is the failure this ordering avoids. */
#define TIRE_PROFILE_DEFAULT  TIRE_PROFILE_STREET

/* Wheel-derived speed, given a tire OD:
 *   mph = motor_rpm / FINAL_DRIVE / 60 * (pi * od_mm / 1000) / 0.44704   */
#define RPM_TO_MPH_FOR(od_mm) \
    ((3.14159265f * (od_mm) / 1000.0f) / (FINAL_DRIVE * 60.0f * 0.44704f))

/* -------------------------------------------------------------------------
 * PWM and control rates.
 *
 * 20 kHz is above the audible band and gives 50 us for both axles' full FOC
 * pipelines. Phase 3 measures whether that holds; `overrun` in the telemetry
 * flags is the runtime canary.
 *
 * !! DEADTIME IS NOT OPTIONAL. Shoot-through destroys a half-bridge in
 * !! microseconds. Scope it before connecting the gate driver. docs/SAFETY.md
 * ------------------------------------------------------------------------- */
#define PWM_FREQ_HZ          20000
#define PWM_DEADTIME_NS      500     /* check against your gate driver spec  */
#define CONTROL_DT           (1.0f / (float)PWM_FREQ_HZ)

#define VELOCITY_LOOP_DIV    10      /* velocity loop at PWM_FREQ / 10 = 2 kHz */
#define POLICY_LOOP_DIV      50      /* traction/launch at 400 Hz              */

/* -------------------------------------------------------------------------
 * THE SPEED CAP.
 *
 * This ceiling goes up ONE PHASE GATE AT A TIME, in source, in a commit. The
 * runtime `cap.mph` parameter is clamped to it, so no tuning session and no
 * host-side bug can talk the truck into going faster than the phase it has
 * actually passed. See docs/BUILD_PHASES.md and docs/PROTOCOL.md §4.4.
 *
 *   Phase 4       20.0
 *   Phase 6       30.0 -> 40.0 -> 50.0
 *   Phase 7       re-validate from 20.0 back up on the new chassis
 *   Phase 8       55.0 -> 60.0 -> 65.0 -> 70.0
 * ------------------------------------------------------------------------- */
#define SPEED_CAP_CEILING_MPH   20.0f

/* -------------------------------------------------------------------------
 * Protection thresholds. Software layer — the hardware comparator is the one
 * that actually saves the FETs.
 * ------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------
 * TORQUE CURRENT LIMIT — phase-gated, like the speed cap.
 *
 * 25 A is the vehicle's design ceiling: the motor (28 x 47 mm, 2400 kV) is
 * rated 28 A, and launch goes traction-limited at ~23 A, so 25 sits above what
 * is usable and below what the motor tolerates. Raising it buys nothing —
 * 23, 25 and 28 A all give the same 2.39 s to 50 mph.
 *
 * But the BRING-UP POWER STAGE gets there first. An X-NUCLEO-IHM08M1 is
 * ~15 A RMS, so running 25 A through it during Phase 4-6 launches is asking
 * the shunts and traces to do something they are not rated for. The cost of
 * respecting that is small and entirely in a place that does not matter:
 *
 *     limit   0-50 mph
 *      15 A     4.02 s   <- bring-up hardware, tuning phases
 *      20 A     2.80 s
 *      25 A     2.39 s   <- final stages, traction-limited
 *
 * A slower launch is irrelevant while tuning traction control and chasing
 * geometry.
 *
 * ** STAGE 1 IS CAPPED BY INSTRUMENTATION, NOT BY THERMALS. ** The IHM08M1's
 * current-sense amplifiers saturate near 12 A (1.65 V bias / ~13.7 gain /
 * 10 mOhm shunt). Past that the reading silently stops being true, which is
 * the one failure mode Phase 1 exists to catch. No heatsink, duct or short
 * duty cycle moves that number. 10 A leaves honest margin and is plenty to
 * prove current sensing and close both loops. See BOM.md, Stage 1.
 *
 * Note also that "only launches challenge the rating" is true for the speed
 * run and FALSE for 3-5 minute course running, where ~15-20 A per motor is
 * continuous. See BOM.md, "Two vehicles, one truck".
 *
 * So raise this the same way as SPEED_CAP_CEILING_MPH — one step, in source,
 * in a commit, when the hardware under it has been proven:
 *
 *   Phases 1-2  on IHM08M1     10.0   <- amplifier ceiling, not thermal
 *   Phase 2b+   on FSESC6.7    25.0   <- real current sensing; motor-limited
 *   Full send   on FSESC6.7    50.0   <- QuicRun 3650 peak. Not the ESC's 70.
 * ------------------------------------------------------------------------- */
#define IQ_LIMIT_A           10.0f
#define OVERCURRENT_TRIP_A   45.0f
#define OVERTEMP_FOLDBACK_C  80.0f
#define OVERTEMP_CUT_C       95.0f
#define VBAT_LOW_V           12.8f   /* 4S under load; 3.2 V/cell           */
/* -------------------------------------------------------------------------
 * AXLE DESYNC DETECTION
 *
 * The front axle runs a DIFFERENTIAL (decided in Phase 4a), which means a
 * single front wheel lifting over a pavement seam or breaking traction spins
 * up freely and spikes front motor RPM — with no axle failure whatsoever.
 *
 * A bare "divergence > 500 rpm, latch" check would treat that as a desync and
 * cut both channels. **A nuisance latch at 60 mph is not a nuisance** — it is
 * total loss of drive at speed, which is the thing the interlock exists to
 * prevent. So the detector has to separate "a wheel slipped for a moment" from
 * "an axle has actually failed":
 *
 *   PROPORTIONAL  500 rpm is a real divergence at 5,000 and noise at 25,000.
 *                 Threshold scales with mean axle speed, with a floor so it
 *                 stays meaningful at low speed.
 *   PERSISTENT    A wheel lift is tens of milliseconds. A stripped gear, a
 *                 dead inverter, or a lost observer does not heal. Requiring
 *                 the divergence to hold for DESYNC_PERSIST_MS rejects the
 *                 first and still catches the second.
 *
 * The trade is explicit and deliberate: we tolerate up to 50 ms of genuine
 * desync in exchange for not cutting drive over a bump. The failure this
 * guards against — asymmetric torque yawing the truck — develops over far
 * longer than 50 ms.
 * ------------------------------------------------------------------------- */
#define DESYNC_RPM_FLOOR     300.0f  /* absolute floor, low-speed relevance  */
#define DESYNC_FRAC          0.15f   /* fraction of mean axle rpm            */
#define DESYNC_PERSIST_MS    50      /* must hold this long before latching  */
#define RC_TIMEOUT_MS        100
