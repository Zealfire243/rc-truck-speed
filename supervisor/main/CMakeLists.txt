idf_component_register(
    SRCS
        "main.c"
        "link.c"
        "gps.c"
        "logger.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        driver          # uart, spi
        esp_timer
        json            # cJSON, inbound command parsing only
        fatfs           # SD card logging
        esp_wifi
)
