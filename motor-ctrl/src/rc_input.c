/**
 * @file rc_input.c
 * @brief CRSF frame decoding.
 *
 * STUB — Phase 4. The frame format and the two-layer failsafe requirement are
 * in rc_input.h.
 *
 * Implementation notes for when you get here:
 *   - Ring-buffer the UART bytes; do not parse in the ISR.
 *   - Resync on 0xC8 after any CRC failure rather than trying to recover the
 *     frame. A dropped frame costs 2 ms; a misparsed one commands torque.
 *   - Count CRC errors and publish them. A link that is degrading shows up
 *     here long before it fails outright.
 */

#include "rc_input.h"

#include "pin_map.h"

static rc_state_t s_rc;

void rc_input_init(void)
{
    s_rc.throttle = 0.0f;
    s_rc.steer = 0.0f;
    s_rc.aux = 0;
    s_rc.valid = false;
    s_rc.last_frame_ms = 0;
    s_rc.frame_count = 0;
    s_rc.crc_errors = 0;
}

void rc_input_feed(const uint8_t *bytes, size_t len)
{
    (void)bytes; (void)len;
    /* TODO Phase 4: [0xC8][len][type][payload][crc8 0xD5], type 0x16 is
     * 16 channels x 11 bits. */
}

const rc_state_t *rc_input_get(void)
{
    return &s_rc;
}

bool rc_input_timed_out(uint32_t now_ms)
{
    if (!s_rc.valid) {
        return true;
    }
    return (now_ms - s_rc.last_frame_ms) > RC_TIMEOUT_MS;
}
