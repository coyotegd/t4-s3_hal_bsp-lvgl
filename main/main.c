
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "hal_mgr.h"

// Static/global declarations after includes
static const char *TAG = "main";

// Color bars (RGB565)
const uint16_t rainbow[] = {
    0xF800, // Red
    0xFD20, // Orange
    0xFFE0, // Yellow
    0x07E0, // Green
    0x001F, // Blue
    0x4810, // Indigo
    0xA81F  // Violet
};

// Get current dimensions from display driver
static inline uint16_t get_display_width(void) {
    return hal_get_display_width();
}

static inline uint16_t get_display_height(void) {
    return hal_get_display_height();
}

void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    hal_draw_rect(x, y, w, h, color);
}

void cycle_rotation(void) {
    hal_cycle_rotation();
    ESP_LOGI(TAG, "Rotation cycled via HAL");
}

void redraw_screen(void) {
    hal_redraw_screen();
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting App...");
    

    // Initialize all hardware via HAL
    hal_init();

    // Draw initial screen
    redraw_screen();

    // Touch and button handling loop
    while (1) {
        hal_handle_touch();
        hal_button_poll();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}