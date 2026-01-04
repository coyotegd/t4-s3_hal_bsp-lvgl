#include <stdio.h>
#include "rm690b0.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
    printf("Testing RM690B0...\n");
    rm690b0_init();
    rm690b0_set_brightness(128);
    rm690b0_clear_full_display(0xF800); // Red
    
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
