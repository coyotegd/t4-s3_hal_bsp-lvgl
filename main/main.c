#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "hal_mgr.h"
#include "lvgl_mgr.h"
#include "lvgl.h"
#include "lv_ui.h"
#include "ui_private.h"

static const char *TAG = "main";

// Minimal USB handler
static void my_usb_handler(bool plugged, void *user_ctx) {
    ESP_LOGI(TAG, "USB %s", plugged ? "Plugged" : "Unplugged");
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting T4-S3 BSP Project...");
    ESP_LOGI(TAG, "LVGL Version: %d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    // Initialize BSP (which inits LVGL, HAL)
    if (bsp_init() != ESP_OK) {
        ESP_LOGE(TAG, "BSP Initialization failed! System halted.");
        while(1) vTaskDelay(1000);
    }

    // Create the UI
    lvgl_mgr_lock();
    lv_ui_init();
    lvgl_mgr_unlock();

    ESP_LOGI(TAG, "UI initialized");

    // Register callbacks to the HAL via the manager if needed
    hal_mgr_register_usb_callback(my_usb_handler, NULL);

    ESP_LOGI(TAG, "HAL & BSP + LVGL Systems ready . . .");

    // Main loop - UI is now interactive via touch. Put you code here. Arduino style loop.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
