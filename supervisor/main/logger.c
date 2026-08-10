/**
 * @file logger.c
 * @brief SD run logging. STUB — Phase 5. See logger.h.
 */

#include "logger.h"

#include "pin_map.h"

bool logger_init(void)
{
    /* TODO Phase 5: mount FATFS over SPI. */
    return false;
}

bool logger_start_run(void) { return false; }

void logger_write_line(const char *line, size_t len)
{
    (void)line; (void)len;
    /* TODO: buffer. Do NOT write per-frame — an SD write can block for tens
     * of milliseconds and you will drop telemetry. */
}

void logger_flush(void) { }
void logger_end_run(void) { }
