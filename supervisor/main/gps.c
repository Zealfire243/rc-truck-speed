/**
 * @file gps.c
 * @brief 10 Hz GPS parsing. STUB — Phase 5. See gps.h.
 */

#include "gps.h"

#include "pin_map.h"

static gps_state_t s_gps;

void gps_init(void)
{
    s_gps.ground_mph = 0.0f;
    s_gps.sats = 0;
    s_gps.fix = false;
    s_gps.last_update_ms = 0;
    /* TODO Phase 5: UART1 at GPS_BAUD; configure the module to 10 Hz and to
     * emit its velocity solution. Doppler velocity, NOT position differencing
     * — see gps.h. */
}

const gps_state_t *gps_get(void)
{
    return &s_gps;
}
