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

/** How many axles are physically populated. Phases 1-2 = 1, Phase 3+ = 2. */
#ifndef AXLES_POPULATED
#define AXLES_POPULATED 1
#endif

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
#define POLE_PAIRS           2       /* count magnets / 2. VERIFY.           */
#define FINAL_DRIVE          2.75f    /* motor turns per wheel turn           */
#define TIRE_OD_MM           55.0f
#define TIRE_CIRC_M          (3.14159265f * TIRE_OD_MM / 1000.0f)

/* Wheel-derived speed:
 *   mph = motor_rpm / FINAL_DRIVE / 60 * TIRE_CIRC_M / 0.44704            */
#define RPM_TO_MPH           (TIRE_CIRC_M / (FINAL_DRIVE * 60.0f * 0.44704f))

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
#define IQ_LIMIT_A           30.0f
#define OVERCURRENT_TRIP_A   45.0f
#define OVERTEMP_FOLDBACK_C  80.0f
#define OVERTEMP_CUT_C       95.0f
#define VBAT_LOW_V           12.8f   /* 4S under load; 3.2 V/cell           */
#define DESYNC_RPM           500.0f  /* axle divergence that cuts BOTH      */
#define RC_TIMEOUT_MS        100
