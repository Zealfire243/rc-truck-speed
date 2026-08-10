/**
 * @file link.c
 * @brief JSON-lines serialization and command dispatch.
 *
 * STUB. See link.h and docs/PROTOCOL.md.
 *
 * The one rule that must not be broken: serialization happens in the task, not
 * the control ISR. Building a ~250 byte frame with float formatting takes far
 * longer than the 50 us the control loop has.
 */

#include "link.h"

#include "protocol.h"

void link_init(uint32_t baud)
{
    (void)baud;
    /* TODO: UART4 at PROTO_LINK_BAUD, DMA TX, ring-buffer RX. */
}

void link_publish(const tlm_snapshot_t *snap)
{
    (void)snap;
    /* TODO: struct copy into the back buffer, swap. Must stay cheap — this is
     * called from the control ISR. */
}

void link_task_step(void)
{
    /* TODO: serialize the front buffer at tlm.hz, transmit, then drain and
     * dispatch inbound commands. Apply parameter changes at a tick boundary,
     * never mid-pipeline. `estop` is the exception — honour it immediately. */
}

void link_feed(const uint8_t *bytes, size_t len)
{
    (void)bytes; (void)len;
    /* TODO: ring buffer. Skip any line whose first non-whitespace character
     * is not '{' — docs/PROTOCOL.md §1.1. */
}

void link_event(const char *level, const char *fmt, ...)
{
    (void)level; (void)fmt;
    /* TODO: emit an `evt` frame. */
}

void link_send_hello(void)
{
    /* TODO: emit `hello` with proto, fw, app, axles, hz, and cap_mph.
     * cap_mph must be there — a host is required to display it prominently. */
}
