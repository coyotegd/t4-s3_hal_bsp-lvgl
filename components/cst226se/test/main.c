#include <stdio.h>
#include "cst226se.h"
#include "sy6970.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void touch_cb(const cst226se_data_t *data, void *user_ctx) {
    if (data->pressed) {
        printf("Touch: %u, %u\n", data->x, data->y);
    } else {
        printf("Touch released\n");
    }
}

void app_main(void) {
    printf("Testing CST226SE...\n");
    sy6970_init(); // Required for I2C bus
    cst226se_init();
    cst226se_register_callback(touch_cb, NULL);
    
    cst226se_data_t data;
    while(1) {
        if (cst226se_wait_event(1000)) {
            cst226se_read(&data);
        }
    }
}
