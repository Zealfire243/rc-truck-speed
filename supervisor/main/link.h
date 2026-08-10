/**
 * @file link.h
 * @brief The G4 link. Relay, don't interpret.
 *
 * Frames from the G4 are passed through VERBATIM to the log and to any WiFi
 * client. This chip adds its own GPS frames and merges a `gps` object into
 * outbound telemetry, and that is the extent of its involvement.
 *
 * It must never originate a command that affects driving. Not a torque
 * request, not a cap change, not a fault clear. If a future feature seems to
 * need one, that feature belongs on the G4.
 *
 * See docs/ARCHITECTURE.md and the Phase 5 gate that tests this by pulling
 * this chip's power at 20 mph.
 *
 * STUB — Phase 5.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void link_init(void);

/** Drain the G4 UART, relay to log and WiFi, parse what we care about. */
void link_task_step(void);

/** Forward a host command to the G4. Tuning and telemetry parameters only —
 *  this is a pipe for the laptop, not an authority of its own. */
bool link_forward_command(const char *json, size_t len);
