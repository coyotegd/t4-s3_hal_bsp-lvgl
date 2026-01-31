#include "ota_mgr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_https_ota.h"
#include "esp_sntp.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "string.h"
#include <stdio.h>

static const char *TAG = "ota_mgr";
static bool s_ota_busy = false;

typedef struct {
    char *url;
    ota_progress_cb_t progress_cb;
    ota_completion_cb_t complete_cb;
    void *user_ctx;
} ota_task_params_t;

// -- Helpers for Version Comparison --
static void parse_version(const char *version, int *major, int *minor, int *patch) {
    *major = 0; *minor = 0; *patch = 0;
    if (version == NULL) return;
    
    // Skip 'v' or 'V' prefix if present
    const char *ptr = version;
    if (*ptr == 'v' || *ptr == 'V') ptr++;
    
    // Use standard sscanf to parse Major.Minor.Patch
    sscanf(ptr, "%d.%d.%d", major, minor, patch);
}

static bool is_newer_version(const char *current_ver, const char *new_ver) {
    int cur_major, cur_minor, cur_patch;
    int new_major, new_minor, new_patch;
    
    parse_version(current_ver, &cur_major, &cur_minor, &cur_patch);
    parse_version(new_ver, &new_major, &new_minor, &new_patch);
    
    ESP_LOGI(TAG, "Comparing Current: v%d.%d.%d vs New: v%d.%d.%d", 
             cur_major, cur_minor, cur_patch, new_major, new_minor, new_patch);
             
    if (new_major > cur_major) return true;
    if (new_major < cur_major) return false;
    
    if (new_minor > cur_minor) return true;
    if (new_minor < cur_minor) return false;
    
    if (new_patch > cur_patch) return true;
    
    return false; // Equal or older
}

static esp_err_t _validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;

    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

    ESP_LOGI(TAG, "New firmware version: %s", new_app_info->version);

    // Check if new version is strictly newer than current
    if (!is_newer_version(running_app_info.version, new_app_info->version)) {
        ESP_LOGW(TAG, "New version (%s) is not newer than running version (%s). Aborting.", 
                 new_app_info->version, running_app_info.version);
        return ESP_ERR_OTA_UP_TO_DATE;
    }

    return ESP_OK;
}

static void ota_task(void *pvParameter) {
    ota_task_params_t *params = (ota_task_params_t *)pvParameter;
    ESP_LOGI(TAG, "Starting OTA update from %s", params->url);

    esp_http_client_config_t config = {
        .url = params->url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
        .timeout_ms = 10000,
    };

// Note: If you want to support HTTP (insecure), you need to enable CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP in menuconfig
#ifdef CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP
    config.cert_pem = NULL;
#endif

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed: %s", esp_err_to_name(err));
        goto ota_end;
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed: %s", esp_err_to_name(err));
        goto ota_end;
    }
    
    err = _validate_image_header(&app_desc);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_UP_TO_DATE) {
             ESP_LOGI(TAG, "System is already up to date.");
        } else {
             ESP_LOGE(TAG, "image header verification failed");
        }
        goto ota_end;
    }

    int last_progress = -1;
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        // Calculate progress
        int len_total = esp_https_ota_get_image_size(https_ota_handle);
        int len_read = esp_https_ota_get_image_len_read(https_ota_handle);
        
        if (len_total > 0) {
            int progress = (len_read * 100) / len_total;
            if (progress != last_progress) {
                last_progress = progress;
                // ESP_LOGI(TAG, "Progress: %d%%", progress);
                if (params->progress_cb) {
                    params->progress_cb(progress, params->user_ctx);
                }
            }
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA fetch failed: %s", esp_err_to_name(err));
        goto ota_end;
    }

    esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if (ota_finish_err == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful! Rebooting...");
        if (params->complete_cb) {
            params->complete_cb(ESP_OK, params->user_ctx);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "ESP HTTPS OTA finish failed: %s", esp_err_to_name(ota_finish_err));
        err = ota_finish_err; // Provide correct error code
    }
    
    // Manual cleanup not needed if finish is called? 
    // Actually esp_https_ota_finish cleans up the handle.
    https_ota_handle = NULL; 

ota_end:
    if (https_ota_handle) {
        esp_https_ota_abort(https_ota_handle);
    }
    
    if (err != ESP_OK) {
        if (params->complete_cb) {
            params->complete_cb(err, params->user_ctx);
        }
    }

    free(params->url);
    free(params);
    s_ota_busy = false;
    vTaskDelete(NULL);
}

esp_err_t ota_mgr_start_update(const char *url, ota_progress_cb_t progress_cb, ota_completion_cb_t complete_cb, void *user_ctx) {
    if (s_ota_busy) {
        ESP_LOGW(TAG, "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    if (!url) return ESP_ERR_INVALID_ARG;

    ota_task_params_t *params = malloc(sizeof(ota_task_params_t));
    if (!params) return ESP_ERR_NO_MEM;

    params->url = strdup(url);
    params->progress_cb = progress_cb;
    params->complete_cb = complete_cb;
    params->user_ctx = user_ctx;

    s_ota_busy = true;
    
    // Stack size needs to be fairly large for TLS/OTA
    BaseType_t ret = xTaskCreate(ota_task, "ota_task", 8192, params, 5, NULL);
    if (ret != pdPASS) {
        free(params->url);
        free(params);
        s_ota_busy = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool ota_mgr_is_busy(void) {
    return s_ota_busy;
}
