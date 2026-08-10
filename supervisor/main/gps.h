/**
 * @file gps.h
 * @brief 10 Hz GPS. Ground truth for speed, and nothing else.
 *
 * ============================================================================
 * WHY 10 Hz AND NOT 1 Hz
 * ============================================================================
 *
 * At 31 m/s a 1 Hz module samples once every 31 meters. A speed run is over in
 * a couple of seconds — that is two or three data points for the entire run,
 * and none of them necessarily at the peak.
 *
 * 10 Hz gives ~3 m resolution, which is enough to see the shape of the run:
 * where it stopped accelerating, whether it held speed, where the cap bit.
 *
 * ============================================================================
 * WHAT THIS IS FOR, AND WHAT IT IS NOT FOR
 * ============================================================================
 *
 * IS:  proving the number, and calibrating the fast wheel-only slip estimator
 *      that traction control actually closes on. See speed_loop.h.
 *
 * IS NOT: a control input. 10 Hz is 100 ms, which is 3 meters of travel and
 * roughly 2000 control ticks. It arrives over a link the architecture
 * explicitly permits to die. Never close a loop on it.
 *
 * ============================================================================
 * PRACTICALITIES
 * ============================================================================
 *
 * - Use the module's velocity solution (Doppler-derived), NOT position
 *   differencing. Doppler velocity is far more accurate and far less noisy.
 * - Wait for a real fix before a run. Publish `sats` in telemetry so the
 *   number on the log has provenance.
 * - Log the raw NMEA/UBX alongside the parsed values. When a number looks too
 *   good, you will want the raw stream.
 *
 * STUB — Phase 5.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float    ground_mph;
    uint8_t  sats;
    bool     fix;
    uint32_t last_update_ms;
} gps_state_t;

void gps_init(void);
const gps_state_t *gps_get(void);
