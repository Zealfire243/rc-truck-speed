/**
 * @file pin_map.h
 * @brief ESP32 supervisor pins.
 *
 * ESP32 GPIO ground rules, carried over from the crawler unchanged:
 *
 *   GPIO 6..11      NEVER USE. SPI flash. Touching them bricks boot.
 *   GPIO 34,35,
 *        36,39      INPUT ONLY, and no internal pull-ups.
 *   GPIO 0,2,12,15  Strapping pins. Check boot-time levels before using.
 *
 * Nothing on this chip can hurt anything. That is deliberate — see
 * docs/ARCHITECTURE.md.
 */

#pragma once

#include "driver/gpio.h"

/* Link to the STM32G4. Common ground required. */
#define PIN_LINK_RX       GPIO_NUM_16   /* UART2 */
#define PIN_LINK_TX       GPIO_NUM_17
#define LINK_BAUD         921600

/* GPS — 10 Hz module. */
#define PIN_GPS_RX        GPIO_NUM_4    /* UART1 */
#define PIN_GPS_TX        GPIO_NUM_5    /* for configuring the module */
#define GPS_BAUD          115200

/* SD card, SPI. */
#define PIN_SD_MISO       GPIO_NUM_19
#define PIN_SD_MOSI       GPIO_NUM_23
#define PIN_SD_CLK        GPIO_NUM_18
#define PIN_SD_CS         GPIO_NUM_13

#define PIN_STATUS_LED    GPIO_NUM_2    /* strapping: low/floating at boot */
