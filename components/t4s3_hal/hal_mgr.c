// All includes at the top, deduplicated
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>
#include "hal_mgr.h"
#include "include/rm690b0.h"
#include "include/sy6970.h"
#include "include/cst226se.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "include/button.h"

static const char *TAG = "hal_mgr";
static uint8_t prev_brightness = 128;
static bool display_asleep = false;
static bool display_dimmed = false;
static TimerHandle_t sleep_timer = NULL;
static volatile bool display_sleep_pending = false;

// HAL display control wrappers
void hal_display_power(bool on) {
    rm690b0_display_power(on);
}

void hal_display_sleep(bool sleep) {
    rm690b0_sleep_mode(sleep);
}

void hal_display_set_brightness(uint8_t brightness) {
    rm690b0_set_brightness(brightness);
}

void hal_button_poll(void) {
    if (display_sleep_pending) {
    ESP_LOGI(TAG, "Display entering sleep mode after inactivity");
    hal_display_set_brightness(0);
    hal_display_sleep(true);
    hal_display_power(false);
    display_asleep = true;
    display_dimmed = false;
    display_sleep_pending = false;
    }
    button_poll();
    button_event_t event = button_get_event();
    switch (event) {
        case BUTTON_EVENT_PRESS:
            hal_button_press();
            break;
        case BUTTON_EVENT_DOUBLE_PRESS:
            hal_button_double_press();
            break;
        case BUTTON_EVENT_LONG_PRESS:
            hal_button_long_press();
            break;
        default:
            break;
    }
}

// Forward declaration to avoid implicit declaration warning
void hal_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

// --- Red box blink logic ---


static bool stat_led_on = true; // Track STAT LED state

esp_err_t hal_set_stat_led(bool on) {
    esp_err_t ret = sy6970_set_stat_led(on);
    if (ret == ESP_OK) {
        stat_led_on = on;
    }
    return ret;
}
// Stub implementations for missing static functions to resolve build errors
static void get_black_box_geometry(uint16_t *black_box_x, uint16_t *black_box_y, uint16_t *red_box_x, uint16_t *red_box_y, uint16_t *red_box_size) {
    // Provide default geometry matching hal_redraw_screen logic
    uint16_t disp_w = rm690b0_get_width();
    uint16_t disp_h = rm690b0_get_height();
    uint16_t center_box_x = (disp_w - 100) / 2;
    uint16_t center_box_y = (disp_h - 100) / 2;
    uint16_t upright_box_x = disp_w - 100 - 20;
    uint16_t upright_box_y = 20;
    uint16_t center_cx = center_box_x + 100/2;
    uint16_t center_cy = center_box_y + 100/2;
    uint16_t upright_cx = upright_box_x + 100/2;
    uint16_t upright_cy = upright_box_y + 100/2;
    uint16_t black_cx = center_cx + ((upright_cx - center_cx) * 3) / 4;
    uint16_t black_cy = center_cy + ((upright_cy - center_cy) * 3) / 4;
    *black_box_x = black_cx - 100/2;
    *black_box_y = black_cy - 100/2;
    *red_box_size = 75;
    *red_box_x = *black_box_x + (100 - *red_box_size) / 2;
    *red_box_y = *black_box_y + (100 - *red_box_size) / 2;
}
static void hal_redraw_blinking_box(void) {
    // Redraw the red box in the black box, color depends on stat_led_on
    uint16_t black_box_x, black_box_y, red_box_x, red_box_y, red_box_size;
    get_black_box_geometry(&black_box_x, &black_box_y, &red_box_x, &red_box_y, &red_box_size);
    // Redraw black box background
    hal_draw_rect(black_box_x, black_box_y, 100, 100, 0x0000);
    // Red or dark red depending on stat_led_on
    uint16_t color = stat_led_on ? 0xF800 : 0x8000; // 0xF800 = bright red, 0x8000 = dark red
    hal_draw_rect(red_box_x, red_box_y, red_box_size, red_box_size, color);
}

// --- Button event stubs ---
void hal_button_press(void) {
    // ...code...
}

static void display_sleep_cb(TimerHandle_t xTimer) {
    display_sleep_pending = true;
}

void hal_button_double_press(void) {
    ESP_LOGI(TAG, "Double press detected: Dimming screen to 0");
    // Save current brightness
    prev_brightness = 128; // Default, or track actual if adjustable elsewhere
    // Smoothly dim to 0
    for (int b = prev_brightness; b >= 0; b -= 8) {
        hal_display_set_brightness((uint8_t)b);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    hal_display_set_brightness(0);
    ESP_LOGI(TAG, "Screen dimmed to 0. Scheduling sleep in 1 minute.");
    // Start/restart 1 minute timer for sleep
    if (!sleep_timer) {
        sleep_timer = xTimerCreate("sleep_timer", pdMS_TO_TICKS(60000), pdFALSE, NULL, display_sleep_cb);
    }
    xTimerStop(sleep_timer, 0);
    xTimerStart(sleep_timer, 0);
    display_dimmed = true;
}

void hal_button_long_press(void) {
    // ...existing code...
}


static uint8_t current_rotation = 0;

void hal_init(void) {
    sy6970_init();
    cst226se_init();
    rm690b0_init();
    button_init();
    current_rotation = 0;
    rm690b0_set_rotation(current_rotation);
    cst226se_set_rotation(current_rotation);
    cst226se_set_max_coordinates(rm690b0_get_width(), rm690b0_get_height());
    rm690b0_set_brightness(128);
}

void hal_set_rotation(uint8_t rot) {
    current_rotation = rot;
    rm690b0_set_rotation(rot);
    cst226se_set_rotation(rot);
    cst226se_set_max_coordinates(rm690b0_get_width(), rm690b0_get_height());
}

void hal_clear_full_display(uint16_t color) {
    rm690b0_clear_full_display(color);
}

void hal_redraw_screen(void) {
    hal_clear_full_display(0x0000); // Always clear to black before any redraw
    uint16_t disp_w = rm690b0_get_width();
    uint16_t disp_h = rm690b0_get_height();
    // Draw 7 rainbow bars
    const uint16_t rainbow[] = {0xF800,0xFD20,0xFFE0,0x07E0,0x001F,0x4810,0xA81F};
    uint16_t bar_w = disp_w / 7;
    for (int i = 0; i < 7; i++) {
        uint16_t current_bar_w = (i == 6) ? (disp_w - (i * bar_w)) : bar_w;
        hal_draw_rect(i * bar_w, 0, current_bar_w, disp_h, rainbow[i]);
    }
    // Center white box
    uint16_t center_box_x = (disp_w - 100) / 2;
    uint16_t center_box_y = (disp_h - 100) / 2;
    hal_draw_rect(center_box_x, center_box_y, 100, 100, 0xFFFF);


    // Calculate upper right box position (top-right, margin 20)
    uint16_t upright_box_x = disp_w - 100 - 20;
    uint16_t upright_box_y = 20;

    // Find centers of both boxes
    uint16_t center_cx = center_box_x + 100/2;
    uint16_t center_cy = center_box_y + 100/2;
    uint16_t upright_cx = upright_box_x + 100/2;
    uint16_t upright_cy = upright_box_y + 100/2;

    // Move 3/4 of the way from center to upper right
    uint16_t black_cx = center_cx + ((upright_cx - center_cx) * 3) / 4;
    uint16_t black_cy = center_cy + ((upright_cy - center_cy) * 3) / 4;

    // Convert back to top-left for drawing
    uint16_t black_box_x = black_cx - 100/2;
    uint16_t black_box_y = black_cy - 100/2;


    // Draw the dynamic black box
    hal_draw_rect(black_box_x, black_box_y, 100, 100, 0x0000);

    // Draw the red/dark red box based on STAT LED state
    hal_redraw_blinking_box();

    ESP_LOGI(TAG, "Screen redrawn: %dx%d", disp_w, disp_h);
}

void hal_set_brightness(uint8_t brightness) {
    hal_display_set_brightness(brightness);
}

uint16_t hal_get_display_width(void) {
    return rm690b0_get_width();
}

uint16_t hal_get_display_height(void) {
    return rm690b0_get_height();
}

void hal_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    uint16_t disp_w = rm690b0_get_width();
    uint16_t disp_h = rm690b0_get_height();
    if (x >= disp_w || y >= disp_h) return;
    if (x + w > disp_w) w = disp_w - x;
    if (y + h > disp_h) h = disp_h - y;
    const int CHUNK_HEIGHT = 20;
    size_t max_chunk_pixels = w * CHUNK_HEIGHT;
    uint16_t *buf = heap_caps_malloc(max_chunk_pixels * 2, MALLOC_CAP_SPIRAM);
    if (!buf) buf = malloc(max_chunk_pixels * 2);
    if (!buf) { return; }
    uint16_t color_be = (color << 8) | (color >> 8);
    for (size_t i = 0; i < max_chunk_pixels; i++) buf[i] = color_be;
    for (uint16_t current_y = y; current_y < y + h; current_y += CHUNK_HEIGHT) {
        uint16_t current_h = (current_y + CHUNK_HEIGHT > y + h) ? (y + h - current_y) : CHUNK_HEIGHT;
        rm690b0_set_window(x, current_y, x + w - 1, current_y + current_h - 1);
        rm690b0_send_pixels((uint8_t *)buf, w * current_h * 2);
    }
    free(buf);
}

void hal_cycle_rotation(void) {
    current_rotation = (current_rotation + 1) % 4;
    hal_set_rotation(current_rotation);
    ESP_LOGI(TAG, "Rotation changed to: %d", current_rotation);
}

bool hal_handle_touch(void) {
    static bool was_pressed = false;
    static int fail_count = 0;
    hal_touch_data_t touch_data;
    cst226se_data_t td;
    if (cst226se_read(&td)) {
        fail_count = 0;
        touch_data.x = td.x;
        touch_data.y = td.y;
        touch_data.pressed = td.pressed;
        if (touch_data.pressed) {
            // Wake display if asleep or dimmed
            if (display_asleep || display_dimmed) {
                ESP_LOGI(TAG, "Touch detected: Waking display and restoring brightness");
                hal_display_power(true);
                hal_display_sleep(false);
                if (display_asleep) {
                    vTaskDelay(pdMS_TO_TICKS(120)); 
                    hal_display_set_brightness(prev_brightness);
                } else {
                    hal_display_set_brightness(prev_brightness);
                }
                display_asleep = false;
                display_dimmed = false;
                hal_redraw_screen();
                if (sleep_timer) xTimerStop(sleep_timer, 0);
                return true;
            }
            uint16_t disp_w = hal_get_display_width();
            uint16_t disp_h = hal_get_display_height();
            if (touch_data.x >= disp_w) touch_data.x = disp_w - 1;
            if (touch_data.y >= disp_h) touch_data.y = disp_h - 1;
            if (!was_pressed) {
                ESP_LOGI(TAG, "Touch: %d,%d", touch_data.x, touch_data.y);
                was_pressed = true;
                // Center white box
                uint16_t box_x = (disp_w - 100) / 2;
                uint16_t box_y = (disp_h - 100) / 2;
                if (touch_data.x >= box_x && touch_data.x < (box_x + 100) &&
                    touch_data.y >= box_y && touch_data.y < (box_y + 100)) {
                    hal_cycle_rotation();
                    hal_redraw_screen();
                    return true;
                }
                // --- Red box geometry ---
                uint16_t black_box_x, black_box_y, red_box_x, red_box_y, red_box_size;
                get_black_box_geometry(&black_box_x, &black_box_y, &red_box_x, &red_box_y, &red_box_size);
                if (touch_data.x >= red_box_x && touch_data.x < (red_box_x + red_box_size) &&
                    touch_data.y >= red_box_y && touch_data.y < (red_box_y + red_box_size)) {
                    // Toggle STAT LED
                    hal_set_stat_led(!stat_led_on);
                    // Redraw blinking box immediately
                    hal_redraw_blinking_box();
                    return true;
                }
            }
            int16_t draw_x = touch_data.x - 5;
            int16_t draw_y = touch_data.y - 5;
            if (draw_x < 0) draw_x = 0;
            if (draw_y < 0) draw_y = 0;
            ESP_LOGI(TAG, "Drawing Black Square at %d, %d (Touch: %d, %d)", draw_x, draw_y, touch_data.x, touch_data.y);
            hal_draw_rect(draw_x, draw_y, 10, 10, 0x0000);
        } else {
            if (was_pressed) {
                ESP_LOGI(TAG, "Touch Released");
                was_pressed = false;
            }
        }
    } else {
        if (was_pressed) {
            ESP_LOGW(TAG, "Touch Read Failed - Forcing Release");
            hal_draw_rect(250, 175, 100, 100, 0xFFFF);
            was_pressed = false;
        }
        if (fail_count++ > 50) {
            ESP_LOGW(TAG, "Too many touch failures, resetting...");
            cst226se_reset();
            fail_count = 0;
        }
        return false;
    }
    return true;
}
