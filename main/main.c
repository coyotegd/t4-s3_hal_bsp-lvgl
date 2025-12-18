#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "rm690b0.h"
#include "sy6970.h"

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

void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    ESP_LOGI(TAG, "Drawing Rect: x=%d, y=%d, w=%d, h=%d, color=0x%04X", x, y, w, h, color);
    // Boundary check for 450 active width limit
    if (x >= LOGICAL_WIDTH || y >= LOGICAL_HEIGHT) return;
    if (x + w > LOGICAL_WIDTH) w = LOGICAL_WIDTH - x;
    if (y + h > LOGICAL_HEIGHT) h = LOGICAL_HEIGHT - y;

    // Chunking to avoid huge allocations and SPI limits
    // ESP32-S3 DMA limit is 2^18 bits = 32768 bytes.
    // 600 width * 20 height * 2 bytes/pixel = 24000 bytes < 32768 bytes.
    const int CHUNK_HEIGHT = 20; 
    size_t max_chunk_pixels = w * CHUNK_HEIGHT;
    
    // Try to allocate in SPIRAM first
    uint16_t *buf = heap_caps_malloc(max_chunk_pixels * 2, MALLOC_CAP_SPIRAM);
    if (!buf) {
        // Fallback to default malloc (Internal RAM)
        buf = malloc(max_chunk_pixels * 2);
    }
    
    if (!buf) {
        printf(
            "Malloc failed\n");
        return;
    }

    // Swap bytes for SPI (Big Endian)
    uint16_t color_be = (color << 8) | (color >> 8);
    for (size_t i = 0; i < max_chunk_pixels; i++) buf[i] = color_be;

    for (uint16_t current_y = y; current_y < y + h; current_y += CHUNK_HEIGHT) {
        uint16_t current_h = (current_y + CHUNK_HEIGHT > y + h) ? (y + h - current_y) : CHUNK_HEIGHT;
        
        rm690b0_set_window(x, current_y, x + w - 1, current_y + current_h - 1);
        rm690b0_send_cmd(RM690B0_RAMWR, NULL, 0);
        rm690b0_send_pixels((uint8_t *)buf, w * current_h * 2);
    }
    free(buf);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting App...");
    
    // Initialize PMIC first to ensure stable power
    sy6970_init();
    
    rm690b0_init();
    rm690b0_set_rotation(RM690B0_ROT_90_CCW);

    // Clear Screen to Black (Within 600x450 active area)
    draw_rect(0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT, 0x0000);

    // Draw 7 Rainbow Bars
    uint16_t bar_w = LOGICAL_WIDTH / 7;
    for (int i = 0; i < 7; i++) {
        draw_rect(i * bar_w, 0, bar_w, LOGICAL_HEIGHT, rainbow[i]);
    }

    // Drawing Primitive: A white square in the center
    draw_rect(250, 175, 100, 100, 0xFFFF);

    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "Heartbeat %d", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}