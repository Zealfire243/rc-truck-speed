/**
 * @file main.c
 * @brief Supervisor entry point.
 *
 * This chip does three things: relay the G4's telemetry, add GPS, and write it
 * all to an SD card. It has no authority over the motors and no path to them.
 *
 * The test that keeps it honest is a Phase 5 pass criterion: pull this board's
 * power at 20 mph and keep driving. If that ever stops working, something has
 * crept into the control path and needs to come out. See docs/ARCHITECTURE.md.
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gps.h"
#include "link.h"
#include "logger.h"
#include "pin_map.h"
#include "protocol.h"

static const char *TAG = "supervisor";

void app_main(void)
{
    ESP_LOGI(TAG, "speed truck supervisor, proto %d, fw %s",
             PROTO_VERSION, FW_VERSION);
    ESP_LOGI(TAG, "no motor authority by design -- see docs/ARCHITECTURE.md");

    link_init();

#if CONFIG_SPEED_GPS_ENABLED
    gps_init();
#endif

#if CONFIG_SPEED_SD_LOGGING
    if (!logger_init()) {
        /* Not fatal. Losing the log is annoying; it is not a reason to stop. */
        ESP_LOGW(TAG, "SD logging unavailable, continuing without it");
    }
#endif

    for (;;) {
        link_task_step();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
