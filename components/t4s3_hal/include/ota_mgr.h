#pragma once

#include "esp_err.h"
#include "esp_https_ota.h"
#include <stdbool.h>

// Manually define base if not found, though esp_err.h should have it
#ifndef ESP_ERR_OTA_BASE
#define ESP_ERR_OTA_BASE 0x1500
#endif

#define ESP_ERR_OTA_UP_TO_DATE (ESP_ERR_OTA_BASE + 0xAA)

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ota_progress_cb_t)(int percent, void *user_ctx);
typedef void (*ota_completion_cb_t)(esp_err_t err, void *user_ctx);

/**
 * @brief Start the OTA update process from a URL.
 * This function will spawn a task to handle the update in the background.
 * 
 * @param url The URL to the firmware .bin file (HTTPS recommended).
 * @param progress_cb Optional callback for progress updates (0-100%).
 * @param complete_cb Optional callback when finished (success or error).
 * @param user_ctx User context pointer passed to callbacks.
 * @return ESP_OK if the task was started successfully.
 */
esp_err_t ota_mgr_start_update(const char *url, ota_progress_cb_t progress_cb, ota_completion_cb_t complete_cb, void *user_ctx);

/**
 * @brief Check if OTA is currently in progress.
 */
bool ota_mgr_is_busy(void);

#ifdef __cplusplus
}
#endif
