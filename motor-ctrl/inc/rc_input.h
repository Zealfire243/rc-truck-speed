/**
 * @file rc_input.h
 * @brief CRSF receiver parsing. Wired to the G4, not the ESP32.
 *
 * The receiver lands here because the coordinator must never be in the control
 * path — the same rule that let the crawler survive its Raspberry Pi dying
 * mid-run. See docs/ARCHITECTURE.md#why-the-receiver-goes-to-the-g4, and the
 * Phase 5 gate that tests it by pulling the ESP32's power at 20 mph.
 *
 * ============================================================================
 * WHY CRSF AND NOT SBUS
 * ============================================================================
 *
 *   CRSF @ 500 Hz     ~2 ms
 *   CRSF @ 250 Hz     ~4 ms
 *   SBUS              7-14 ms
 *
 * At 31 m/s the truck covers 31 mm per millisecond, so the difference between
 * those is 60-370 mm of travel per control update. Everything else in the
 * stack is measured in microseconds; the RC link is the slowest thing in the
 * machine that isn't the human. Spend the money.
 *
 * CRSF is 420000 baud 8N1, uninverted, single wire. Frame:
 *
 *     [sync 0xC8] [len] [type] [payload...] [crc8 poly 0xD5]
 *
 * Channels arrive in type 0x16 as 16 channels packed into 11 bits each.
 *
 * ============================================================================
 * FAILSAFE
 * ============================================================================
 *
 * Two independent layers, and you need both:
 *
 *   1. The receiver's own failsafe, configured AT the receiver, which holds
 *      even if this firmware is confused.
 *   2. This module's frame timeout (RC_TIMEOUT_MS), which catches a receiver
 *      that is still transmitting but has itself lost the link.
 *
 * On timeout: torque to zero and COAST. Do not brake. Braking at 70 mph with
 * no steering authority puts the truck sideways — see safety.h.
 *
 * **Test the failsafe by switching the transmitter off, every session, before
 * the first run.** It is a docs/SAFETY.md checklist item and a Phase 4 gate.
 * "I tested it last week" is not a test.
 *
 * STUB — CRSF parsing is not yet implemented. Phase 4.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float    throttle;   /**< -1.0 .. +1.0 */
    float    steer;      /**< -1.0 .. +1.0 */
    int      aux;        /**< switch position */
    bool     valid;
    uint32_t last_frame_ms;
    uint32_t frame_count;
    uint32_t crc_errors;  /**< watch this; rising means a marginal link */
} rc_state_t;

void rc_input_init(void);

/** Feed received bytes in. Call from the UART ISR or a drain task. */
void rc_input_feed(const uint8_t *bytes, size_t len);

/** Latest decoded state. Check `.valid` and the timeout before trusting it. */
const rc_state_t *rc_input_get(void);

/** True if no valid frame has arrived within RC_TIMEOUT_MS. */
bool rc_input_timed_out(uint32_t now_ms);
