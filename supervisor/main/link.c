/**
 * @file link.c
 * @brief G4 link relay. STUB — Phase 5. See link.h.
 */

#include "link.h"

#include "gps.h"
#include "logger.h"
#include "pin_map.h"

void link_init(void)
{
    /* TODO Phase 5: UART2 at LINK_BAUD, ring-buffer RX. */
}

void link_task_step(void)
{
    /* TODO Phase 5:
     *   - drain UART2 line by line
     *   - skip any line whose first non-whitespace char is not '{'
     *     (docs/PROTOCOL.md §1.1)
     *   - merge a `gps` object into `tlm` frames
     *   - write to the log, push to any WiFi client
     *
     * Relay verbatim. Do not reformat, do not filter, do not "improve". The
     * log should be what the G4 actually said. */
}

bool link_forward_command(const char *json, size_t len)
{
    (void)json; (void)len;
    /* TODO Phase 5: forward tuning and telemetry commands only.
     *
     * This is a pipe for the laptop, not an authority. It must never
     * originate a command of its own. */
    return false;
}
