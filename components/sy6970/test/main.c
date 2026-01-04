#include <stdio.h>
#include "sy6970.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
    printf("Testing SY6970...\n");
    sy6970_init();
    
    while(1) {
        bool vbus = sy6970_is_vbus_connected();
        printf("VBUS connected: %s\n", vbus ? "YES" : "NO");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
