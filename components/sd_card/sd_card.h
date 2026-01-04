#pragma once
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the SD Card via SPI and mount the FAT filesystem.
 * 
 * Mount point: "/sdcard"
 * Pins: MOSI=2, MISO=4, CLK=3, CS=1
 * Host: SPI3_HOST
 * 
 * @return ESP_OK on success
 */
esp_err_t sd_card_init(void);

/**
 * @brief Check if the SD card is currently mounted.
 * 
 * @return true if mounted
 */
bool sd_card_is_mounted(void);

#ifdef __cplusplus
}
#endif
