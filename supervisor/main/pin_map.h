/**
 * @file pin_map.h
 * @brief ESP32-S3 supervisor pins.
 *
 * Hardware: **ESP32-S3-DevKitC-1U-N16R8** (WROOM-1U, 16 MB flash, 8 MB octal
 * PSRAM, u.FL antenna connector). See docs/BOM.md.
 *
 * ---------------------------------------------------------------------------
 * S3 GPIO GROUND RULES. These REPLACE the classic-ESP32 rules this file used
 * to carry, which were inherited from the crawler and are wrong for the S3 in
 * both directions -- they forbid pins that are fine and permit pins that are
 * not:
 *
 *   GPIO 26..32     SPI flash. Not broken out on WROOM-1; never reachable.
 *   GPIO 35,36,37   NOT AVAILABLE on this board. Octal PSRAM uses them. An
 *                   N8R2 (quad PSRAM) part would free them; N16R8 does not.
 *   GPIO 19,20      Native USB D-/D+. The console lives here, which is the
 *                   entire reason the S3 was chosen -- see below.
 *   GPIO 0,3,45,46  Strapping pins. Check boot-time levels before using.
 *   GPIO 43,44      Default UART0 TX/RX. Free for our use ONLY because the
 *                   console moved to USB.
 *   GPIO 33,34      Exist on the chip, NOT on the DevKitC-1 headers. Avoided.
 *
 *   GPIO 6..11      **USABLE.** On the classic ESP32 these were SPI flash and
 *                   touching them bricked boot. On the S3 flash is on 26..32
 *                   instead, so 6..11 are ordinary GPIOs. The old warning in
 *                   this file was a real trap carried forward from the wrong
 *                   chip.
 *
 * ---------------------------------------------------------------------------
 * WHY THE S3, AND NOT ANY ESP32. The UART count binds. Two motor controllers
 * plus GPS plus a console is four serial endpoints, and both chips have only
 * three UART controllers. The S3's native USB-Serial-JTAG takes the console
 * off UART0, which is what makes the fourth endpoint exist at all:
 *
 *              classic ESP32              ESP32-S3
 *   console    UART0                      USB (GPIO 19/20)  <- the win
 *   GPS        UART1                      UART1
 *   front      UART2                      UART2
 *   rear       nothing left               UART0
 *
 * ---------------------------------------------------------------------------
 * TWO LINK TOPOLOGIES. CAN is preferred and is the default; the UART pairs are
 * the documented fallback. See docs/ARCHITECTURE.md and docs/WIRING.md.
 *
 * Both are mapped here to pins that do not overlap, so the fallback can be
 * brought up without re-cutting the harness.
 *
 * Nothing on this chip can hurt anything. That is deliberate -- this board has
 * no motor authority and no path to one. See docs/ARCHITECTURE.md.
 */

#pragma once

#include "driver/gpio.h"

/* ---------------------------------------------------------------------------
 * PRIMARY LINK -- CAN / TWAI, one bus, both controllers.
 *
 * Preferred over the UART pairs: one bus serves all three nodes, the F405 has
 * bxCAN natively, and absence of a peer frame is itself a fault the interlock
 * can act on.
 *
 * !! REQUIRES AN EXTERNAL TRANSCEIVER. TWAI is a protocol controller, not a
 * line driver -- these two pins are logic-level TX/RX and must reach the bus
 * through an SN65HVD230 / TJA1051 / MCP2562 or equivalent. The FSESC boards
 * carry their own transceivers (VESC 6 exposes CANH/CANL directly); the ESP32
 * does not. One transceiver, on this end only. See docs/BOM.md.
 *
 * Bus termination is 120 ohm at each of the two physical ends, and only those.
 * ------------------------------------------------------------------------- */
#define PIN_CAN_TX        GPIO_NUM_5
#define PIN_CAN_RX        GPIO_NUM_4
#define CAN_BITRATE       500000     /* 500 kbit/s; both F405s must match. */

/* ---------------------------------------------------------------------------
 * FALLBACK LINKS -- one UART per controller, point to point.
 *
 * Only if CAN does not work out. Two links because there are two controllers:
 * the single PIN_LINK_* pair this file used to define predates the per-axle
 * rearchitecture and could not address the rear axle at all.
 *
 * Front sits on UART0 deliberately: it is free only while the console is on
 * USB, so if someone reverts the console to UART this link is what breaks, and
 * it breaks loudly at init rather than subtly at speed.
 * ------------------------------------------------------------------------- */
#define PIN_LINK_FRONT_TX GPIO_NUM_43   /* UART0 -- see note above */
#define PIN_LINK_FRONT_RX GPIO_NUM_44
#define PIN_LINK_REAR_TX  GPIO_NUM_15   /* UART2 */
#define PIN_LINK_REAR_RX  GPIO_NUM_16
#define LINK_BAUD         921600

/* ---------------------------------------------------------------------------
 * GPS -- 10 Hz module, UART1.
 *
 * 10 Hz is a hard requirement, not a preference: a 1 Hz fix samples once every
 * 31 m at target speed. TX is wired so the module can be configured, not just
 * read. See docs/BOM.md.
 * ------------------------------------------------------------------------- */
#define PIN_GPS_TX        GPIO_NUM_17   /* UART1 -- to the module's RX */
#define PIN_GPS_RX        GPIO_NUM_18
#define GPS_BAUD          115200

/* ---------------------------------------------------------------------------
 * SPI2 -- IMU and SD card share one bus, separate chip selects.
 *
 * These are the S3's native IOMUX pins for SPI2 (FSPI). Using them keeps the
 * bus off the GPIO matrix, which is worth doing: matrix-routed SPI is capped
 * around 40 MHz where IOMUX-routed reaches 80. The SD card is the only device
 * that cares, and it is the one that blocks.
 * ------------------------------------------------------------------------- */
#define PIN_SPI_SCLK      GPIO_NUM_12
#define PIN_SPI_MOSI      GPIO_NUM_11
#define PIN_SPI_MISO      GPIO_NUM_13

#define PIN_SD_CS         GPIO_NUM_9

/* ---------------------------------------------------------------------------
 * IMU -- 6-axis, >=200 Hz. THIS DID NOT EXIST IN THE OLD PIN MAP.
 *
 * It is not an optional instrument. The supervisor fuses this with the 10 Hz
 * GPS into a 200 Hz ground-speed estimate and publishes it to both controllers
 * as an ADVISORY input -- one that can only ever reduce a torque limit, never
 * raise one, enforced by a min(). Detecting all four wheels spinning together
 * needs an acceleration reference independent of the wheels, and nothing else
 * on this vehicle has one. See docs/ARCHITECTURE.md, "the supervisor
 * exception", and docs/BOM.md, "Why the IMU, specifically".
 *
 * SPI, not I2C: at 200 Hz+ with a scope on the bus you want the headroom, and
 * the part is sharing a bus with an SD card either way.
 *
 * INT is the data-ready line. Sample on it rather than polling on a timer --
 * polling aliases against the sensor's own ODR and puts jitter straight into
 * the fused estimate. There is a lot of vibration at 8,537 wheel rpm to alias;
 * set the part's configurable low-pass well below the sample rate.
 * ------------------------------------------------------------------------- */
#define PIN_IMU_CS        GPIO_NUM_10
#define PIN_IMU_INT       GPIO_NUM_14   /* data-ready, rising edge */

/* ---------------------------------------------------------------------------
 * STATUS -- the DevKitC-1 v1.1 onboard addressable RGB LED.
 *
 * Addressable (WS2812-style), not a plain GPIO LED: drive it with RMT or the
 * led_strip component, not gpio_set_level(). v1.0 boards use GPIO 38 instead.
 * ------------------------------------------------------------------------- */
#define PIN_STATUS_LED    GPIO_NUM_48

/* ---------------------------------------------------------------------------
 * Deliberately unassigned, and why:
 *
 *   GPIO 1,2,6,7,8,21,38,39,40,41,42,47   free, on the headers, unclaimed.
 *
 * That is ~12 spare pins against the ~16 in use, which is the margin the S3
 * was bought for. Do not spend it on anything that belongs on a motor
 * controller -- the constraint on this chip is authority, not pin count.
 * ------------------------------------------------------------------------- */
