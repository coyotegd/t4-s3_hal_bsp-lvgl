#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "rm690b0.h"
#include "sy6970.h"
#include "cst226se.h"

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

// App Dimensions (Dynamic based on rotation)
#define APP_DISP_WIDTH  450
#define APP_DISP_HEIGHT 600

// Get current dimensions from display driver
static inline uint16_t get_display_width(void) {
    // For Rotation 0 and 2: 600 (portrait), For Rotation 1 and 3: 450 (landscape)
    return rm690b0_get_width();
}

static inline uint16_t get_display_height(void) {
    // For Rotation 0 and 2: 450 (portrait), For Rotation 1 and 3: 600 (landscape)
    return rm690b0_get_height();
}

void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    uint16_t disp_w = get_display_width();
    uint16_t disp_h = get_display_height();
    
    // Boundary check
    if (x >= disp_w || y >= disp_h) return;
    if (x + w > disp_w) w = disp_w - x;
    if (y + h > disp_h) h = disp_h - y;

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
        rm690b0_send_pixels((uint8_t *)buf, w * current_h * 2);
    }
    free(buf);
}

// Current rotation state
static uint8_t current_rotation = 0;

void cycle_rotation(void) {
    current_rotation = (current_rotation + 1) % 4;
    rm690b0_set_rotation(current_rotation);
    // Update touch max coordinates to match new display orientation
    cst226se_set_max_coordinates(rm690b0_get_width(), rm690b0_get_height());
    cst226se_set_rotation(current_rotation);
    ESP_LOGI(TAG, "Rotation changed to: %d", current_rotation);
}

void redraw_screen(void) {
    uint16_t disp_w = get_display_width();
    uint16_t disp_h = get_display_height();
    
    // Clear screen to black
    draw_rect(0, 0, disp_w, disp_h, 0x0000);
    
    // Additionally clear a small top band for specific rotations that show artifacts
    // Some MADCTL/offset combinations can leave 1-4 rows with leftover pixels.
    if (current_rotation == RM690B0_ROTATION_0 || current_rotation == RM690B0_ROTATION_270) {
        draw_rect(0, 0, disp_w, 4, 0x0000);
    }
    
    // Draw 7 Rainbow Bars
    uint16_t bar_w = disp_w / 7;
    for (int i = 0; i < 7; i++) {
        uint16_t current_bar_w = (i == 6) ? (disp_w - (i * bar_w)) : bar_w;
        draw_rect(i * bar_w, 0, current_bar_w, disp_h, rainbow[i]);
    }

    // Drawing Primitive: A white square in the center
    draw_rect((disp_w - 100) / 2, (disp_h - 100) / 2, 100, 100, 0xFFFF);
    
    ESP_LOGI(TAG, "Screen redrawn: %dx%d", disp_w, disp_h);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting App...");
    
    // Initialize PMIC first to ensure stable power
    sy6970_init();
    
    // Initialize Touch Driver (shares I2C with PMIC)
    cst226se_init();
    cst226se_set_rotation(CST226SE_ROTATION_0);

    rm690b0_init();
    rm690b0_set_rotation(RM690B0_ROTATION_0);
    // Ensure touch mapping matches display logical resolution
    cst226se_set_max_coordinates(rm690b0_get_width(), rm690b0_get_height());
    // Re-apply rotation so transforms use the correct max coordinates
    cst226se_set_rotation((cst226se_rotation_t)0);
    
    // Reduce brightness to ~50% to prevent brownouts/USB disconnects
    rm690b0_set_brightness(128);

    // Draw initial screen
    redraw_screen();

    cst226se_data_t touch_data;
    bool was_pressed = false;

    // Give the touch controller a moment to stabilize
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        // Add a heartbeat log every 5 seconds to ensure loop is running
        static int loop_count = 0;
        static int fail_count = 0;

        if (loop_count++ % 250 == 0) {
            ESP_LOGI(TAG, "Alive... (Free Heap: %ld)", (long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        }

        if (cst226se_read(&touch_data)) {
            fail_count = 0; // Reset failure counter on success
            
            if (touch_data.pressed) {
                // Clamp to logical limits (just in case)
                uint16_t disp_w = get_display_width();
                uint16_t disp_h = get_display_height();
                if (touch_data.x >= disp_w) touch_data.x = disp_w - 1;
                if (touch_data.y >= disp_h) touch_data.y = disp_h - 1;

                // When touched, draw a black square at the touch location
                if (!was_pressed) {
                    ESP_LOGI(TAG, "Touch: %d,%d", touch_data.x, touch_data.y);
                    was_pressed = true;
                    
                    // Check if touch is in center white box area
                    uint16_t disp_w = get_display_width();
                    uint16_t disp_h = get_display_height();
                    uint16_t box_x = (disp_w - 100) / 2;
                    uint16_t box_y = (disp_h - 100) / 2;
                    
                    if (touch_data.x >= box_x && touch_data.x < (box_x + 100) &&
                        touch_data.y >= box_y && touch_data.y < (box_y + 100)) {
                        cycle_rotation();
                        redraw_screen();
                        continue;  // Skip drawing the touch indicator this frame
                    }
                }
                
                // Draw Red Box at touch point (centered)
                int16_t draw_x = touch_data.x - 5;
                int16_t draw_y = touch_data.y - 5;
                if (draw_x < 0) draw_x = 0;
                if (draw_y < 0) draw_y = 0;
                
                ESP_LOGI(TAG, "Drawing Black Square at %d, %d (Touch: %d, %d)", draw_x, draw_y, touch_data.x, touch_data.y);
                draw_rect(draw_x, draw_y, 10, 10, 0x0000); 

            } else {
                if (was_pressed) {
                    ESP_LOGI(TAG, "Touch Released");
                    was_pressed = false;
                }
            }
        } else {
            // I2C Read Failed or Garbage Data
            // If we were pressed, force release to avoid stuck state
            if (was_pressed) {
                 ESP_LOGW(TAG, "Touch Read Failed - Forcing Release");
                 draw_rect(250, 175, 100, 100, 0xFFFF); 
                 was_pressed = false;
            }
            
            // If read fails repeatedly, reset the controller
            if (fail_count++ > 50) {
                ESP_LOGW(TAG, "Too many touch failures, resetting...");
                cst226se_reset();
                fail_count = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // Poll every 20ms
    }
}