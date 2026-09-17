/**
 * @file pin_map.h
 * @brief Single source of truth for every pin and every drivetrain constant.
 *
 * ============================================================================
 * TARGET: STM32F405RG on a Flipsky Mini FSESC6.7 Pro.
 * ============================================================================
 *
 * Bench and flight are the same board. An earlier version of this file mapped
 * a Nucleo-G474RE + X-NUCLEO-IHM08M1 bench rig, with a port planned at Phase
 * 2b; that rig was dropped and there is no port. See
 * docs/BOM.md#decision-no-stage-1-rig-straight-to-the-fsesc.
 *
 * ============================================================================
 * !! THE PIN ASSIGNMENTS BELOW ARE NOT FILLED IN, AND THAT IS DELIBERATE.
 * ============================================================================
 *
 * There is no published schematic for this board revision. Guessing the pin
 * map of a 70 A inverter is the one shortcut this project will not take: a
 * wrong high/low-side pairing is a shoot-through, and a wrong current-sense
 * channel is a torque loop that lies to you. So every hardware-specific value
 * here is left unset and the build refuses to complete until you clear
 * PIN_MAP_VERIFIED below.
 *
 * WHERE THE REAL VALUES COME FROM, in order of authority:
 *
 *   1. The board itself. Read the gate driver part number off the IC, find
 *      and continuity-check the SWD pads, read the shunt markings. This is
 *      the on-arrival checklist in docs/BOM.md and it is not optional.
 *   2. Upstream VESC 6 hardware. The FSESC6.7 is a derivative of VESC 6.6,
 *      which is open hardware: `hw_60.h` / `hw_60.c` in vedder/bldc are the
 *      closest thing to a schematic you will get, and they are a starting
 *      point to VERIFY AGAINST THE BOARD, never to trust outright.
 *   3. Flipsky, if they answer the email. Nice to have, not on the path.
 *
 * `amps_per_count` is MEASURED, not read off a page, however this resolves —
 * bench supply in constant current, rotor clamped. See
 * docs/BUILD_PHASES.md#phase-1b--one-motor-current-limited.
 *
 * ============================================================================
 * THE ARRANGEMENT
 * ============================================================================
 *
 *   TIM1  ->  this controller's single inverter (3 complementary pairs + BKIN)
 *   ADC   ->  three phase currents, pack voltage, board temp
 *
 * One chip, ONE motor. The vehicle runs two copies of this firmware on two
 * boards, coordinated over the peer link — see docs/ARCHITECTURE.md. The
 * front/rear split that used to live in this file was a second FOC instance
 * on TIM8; it is a second board now, so there is one inverter block here.
 *
 * !! The break input must be fed by a hardware overcurrent source. On this
 * !! board that is the gate driver's own fault output — its VDS overcurrent
 * !! protection trips in microseconds, which is what kills the gates when the
 * !! software is wrong, and "the software is wrong" is the case protection
 * !! exists for. Arm it at its most sensitive setting for all of Phase 1.
 * !! See docs/SAFETY.md.
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

/* =========================================================================
 * HARDWARE MAP — UNVERIFIED. Read the block comment at the top of this file
 * before you touch anything below.
 *
 * Fill each PIN_UNVERIFIED in from the board, then define PIN_MAP_VERIFIED
 * and record in the commit message HOW you established each value: read off
 * the IC, buzzed out with a meter, diffed against vedder/bldc `hw_60.h`, or
 * confirmed by Flipsky. "It built" is not a verification.
 * ========================================================================= */
#define PIN_UNVERIFIED  (-1)

/* Fires on firmware builds only. The host tests (test/test_safety.c,
 * test/test_foc.c) pull this header in through safety.h and must keep
 * building with no hardware known — proving the math on your laptop is
 * Phase 1a step 1, and it would be perverse for the pin map to block it. */
#if defined(STM32F405xx) && !defined(PIN_MAP_VERIFIED)
#  error "pin_map.h has not been filled in for your FSESC6.7 board revision. This file ships with no pin assignments on purpose: guessing the high/low-side pairing of a 70 A inverter is a shoot-through, and guessing a current-sense channel is a torque loop that lies. See the block comment at the top of this file, and the on-arrival checklist in docs/BOM.md."
#endif

/* -------------------------------------------------------------------------
 * The inverter — TIM1, three complementary pairs plus the break input.
 *
 * The three high/low pairs MUST be scoped as complementary, with deadtime,
 * before a motor is ever connected. That is Phase 1a step 5 and it is the
 * single test standing between a mistake here and a dead board.
 * ------------------------------------------------------------------------- */
#define PWM_TIMER            TIM1
#define PIN_PWM_AH           PIN_UNVERIFIED
#define PIN_PWM_BH           PIN_UNVERIFIED
#define PIN_PWM_CH           PIN_UNVERIFIED
#define PIN_PWM_AL           PIN_UNVERIFIED
#define PIN_PWM_BL           PIN_UNVERIFIED
#define PIN_PWM_CL           PIN_UNVERIFIED
#define PIN_BKIN             PIN_UNVERIFIED  /* gate driver nFAULT -> TIM1 BKIN */

/* -------------------------------------------------------------------------
 * The gate driver.
 *
 * DRV8301 and DRV8323 both ship on VESC 6 derivatives and their SPI register
 * maps DIFFER. Read the part number off the largest IC next to the FETs
 * before writing a single register — this is item 1 on the on-arrival
 * checklist in docs/BOM.md, and it is the cheapest open question to close.
 *
 * Unlike the IHM08M1, this driver has its own VDS overcurrent protection.
 * Arm it at its most sensitive threshold for all of Phases 1 and 2, and
 * relax it deliberately at Phase 2b rather than by forgetting it is set.
 * ------------------------------------------------------------------------- */
#define GATE_DRIVER_SPI      PIN_UNVERIFIED  /* SPI peripheral               */
#define PIN_GATE_CS          PIN_UNVERIFIED
#define PIN_GATE_ENABLE      PIN_UNVERIFIED
#define PIN_GATE_FAULT       PIN_UNVERIFIED  /* also feeds PIN_BKIN above    */

/* -------------------------------------------------------------------------
 * Analog. All three current channels are sampled by INJECTED conversion
 * triggered off the timer update event, so every phase is measured at the
 * same instant in the PWM cycle. See current_sense.h — this is the single
 * most important configuration detail in the whole firmware.
 *
 * This board is three-shunt, low-side, which is the topology current_sense.h
 * already assumes. What is NOT known is the shunt value, the amplifier gain,
 * or which ADC channel carries which phase.
 * ------------------------------------------------------------------------- */
#define ADC_IA               PIN_UNVERIFIED
#define ADC_IB               PIN_UNVERIFIED
#define ADC_IC               PIN_UNVERIFIED
#define ADC_VBUS             PIN_UNVERIFIED  /* divider ratio also unverified */
#define ADC_TEMP_BOARD       PIN_UNVERIFIED  /* thermistor on the FET plate   */
#define ADC_TEMP_MOTOR       PIN_UNVERIFIED  /* motor thermistor, if wired    */

/* -------------------------------------------------------------------------
 * Comms and I/O. These come off the FSESC's labelled COMM / SENSE headers
 * rather than out of the inverter, so they are the easiest block to confirm
 * — but confirm them anyway.
 * ------------------------------------------------------------------------- */
#define RC_UART              PIN_UNVERIFIED  /* CRSF receiver goes HERE, not
                                              * to the ESP32. ARCHITECTURE.md */
#define LINK_UART            PIN_UNVERIFIED  /* to ESP32-S3, 921600           */
#define PIN_STEER_SERVO      PIN_UNVERIFIED  /* 50 Hz                         */
#define PIN_HALL_A           PIN_UNVERIFIED  /* SENSE header; motor is sensored */
#define PIN_HALL_B           PIN_UNVERIFIED
#define PIN_HALL_C           PIN_UNVERIFIED

/* -------------------------------------------------------------------------
 * Motor sign correction.
 *
 * Set to 1 when this controller's motor drives backwards relative to
 * "positive iq = vehicle forward". Fixing polarity HERE rather than by
 * swapping motor phases keeps every downstream consumer on one convention:
 * positive == forward, always. It is per-build because the two axles can
 * disagree — see AXLE_SELF above and docs/PROTOCOL.md §2.
 * ------------------------------------------------------------------------- */
#define MOTOR_INVERT         0

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
 * 20 kHz is above the audible band and gives 50 us for this controller's full
 * FOC pipeline — which is now one motor's worth, not two, since each axle has
 * its own board. The F405 runs it at roughly 10-15% CPU. Phase 2 measures
 * whether that holds; `overrun` in the telemetry flags is the runtime canary.
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
 * The cost of running below that ceiling while bringing up is small and
 * entirely in a place that does not matter:
 *
 *     limit   0-50 mph
 *      15 A     4.02 s   <- tuning phases
 *      20 A     2.80 s
 *      25 A     2.39 s   <- final stages, traction-limited
 *
 * A slower launch is irrelevant while tuning traction control and chasing
 * geometry.
 *
 * ** THIS NUMBER IS NO LONGER HANDED TO YOU BY THE HARDWARE. ** The dropped
 * IHM08M1 bench rig had a hard ~12 A instrumentation ceiling (amplifier
 * saturation, not thermal) which set this constant for you through Phases
 * 1-2. The FSESC's sensing is scaled for real current and has no such
 * ceiling, so the starting value is now a judgement call — and the loss of
 * that guardrail is worth being conscious of. 2 A is chosen to be obviously
 * inside anything, not because anything forces it.
 *
 * Note also that "only launches challenge the rating" is true for the speed
 * run and FALSE for 3-5 minute course running, where ~15-20 A per motor is
 * continuous. See BOM.md, "Two vehicles, one truck".
 *
 * Raise this the same way as SPEED_CAP_CEILING_MPH — one step, in source, in
 * a commit, when the phase under it has been passed:
 *
 *   Phase 1b     2.0   <- starved on purpose; the bench supply's limit is
 *                         the real protection now that no sacrificial board
 *                         exists. See BUILD_PHASES.md Phase 1a.
 *   Phase 2      5.0   <- enough to load the loops and the observer
 *   Phase 2b    30.0   <- the ladder that proves sensing stays honest
 *   Phase 4+    25.0   <- vehicle design ceiling; motor-limited
 *   Full send   50.0   <- QuicRun 3650 peak. Not the ESC's 70.
 * ------------------------------------------------------------------------- */
#define IQ_LIMIT_A           2.0f
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
