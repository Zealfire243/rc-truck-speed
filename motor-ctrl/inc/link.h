/**
 * @file link.h
 * @brief JSON-lines telemetry and command handling over UART.
 *
 * Same wire format as the crawler, which is the point — `host-tools/` ports
 * across with the field names changed and nothing else. See docs/PROTOCOL.md.
 *
 * Two consumers, one format:
 *   - Phases 1-3: straight to a laptop over USB serial, no ESP32 in existence.
 *   - Phase 5+:   to the ESP32, which relays verbatim and adds GPS.
 *
 * ============================================================================
 * SERIALIZATION MUST NOT HAPPEN IN THE CONTROL ISR
 * ============================================================================
 *
 * Building a ~250 byte JSON frame with float formatting takes far longer than
 * the 50 us the control loop has. So: the ISR writes a snapshot struct, and a
 * lower-priority task serializes and transmits it. If serialization ever shows
 * up in the ISR's timing, `overrun` will tell you — but it should never get
 * that far.
 *
 * Inbound commands are parsed in the task and applied at a tick boundary, not
 * mid-pipeline. A gain that changes halfway through a FOC step produces one
 * corrupt output and a confusing plot.
 *
 * `estop` is the exception: it is honoured immediately, in every state.
 * docs/PROTOCOL.md §4.3.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pin_map.h"

/** What the ISR hands to the serializer. Double-buffered; the ISR fills one
 *  while the task reads the other. */
typedef struct {
    uint32_t ts_ms;
    float    cmd[AXLE_COUNT];
    float    rpm[AXLE_COUNT];
    float    iq[AXLE_COUNT];
    float    id[AXLE_COUNT];
    float    temp_c[AXLE_COUNT];
    float    vbat;
    float    mph;
    float    slip;
    uint32_t flags;
    uint8_t  mode;
} tlm_snapshot_t;

void link_init(uint32_t baud);

/** Called from the control ISR. Cheap: a struct copy and a buffer swap. */
void link_publish(const tlm_snapshot_t *snap);

/** Called from the telemetry task. Serializes and transmits at tlm.hz. */
void link_task_step(void);

/** Feed received bytes in from the UART ISR. */
void link_feed(const uint8_t *bytes, size_t len);

/** Emit an `evt` frame — for things a human should see. Not a log replacement. */
void link_event(const char *level, const char *fmt, ...);

void link_send_hello(void);
