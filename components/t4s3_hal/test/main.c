#include <stdio.h>
#include "hal_mgr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void touch_handler(const cst226se_data_t *data, void *user_ctx) {
    printf("HAL Touch: %u, %u (pressed: %d)\n", data->x, data->y, data->pressed);
}

void app_main(void) {
    printf("Testing T4-S3 HAL...\n");
    hal_mgr_init();
    hal_mgr_register_touch_callback(touch_handler, NULL);
    
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
