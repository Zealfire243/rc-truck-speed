/**
 * @file logger.h
 * @brief SD card run logging.
 *
 * One file per run, JSON lines, exactly what came off the link plus the GPS
 * frames this chip injects. Same format as the wire protocol, so the host
 * tools replay a log file and a live link with the same parser.
 *
 * Write buffered and flush on a timer. An SD write can block for tens of
 * milliseconds, which is fine here (nothing safety-critical runs on this chip)
 * but will drop telemetry frames if you write per-frame.
 *
 * Name files with the boot timestamp and a sequence number. You will do a
 * dozen runs in an afternoon and they will all blur together.
 *
 * STUB — Phase 5.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

bool logger_init(void);
bool logger_start_run(void);
void logger_write_line(const char *line, size_t len);
void logger_flush(void);
void logger_end_run(void);
