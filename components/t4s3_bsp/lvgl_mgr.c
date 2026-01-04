#include "lvgl_mgr.h"
#include "hal_mgr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "lvgl_mgr";

static SemaphoreHandle_t lvgl_mux = NULL;
static SemaphoreHandle_t s_vsync_sem = NULL;
static lv_display_t *lv_disp = NULL;
static lv_indev_t *lv_touch = NULL;
static rm690b0_rotation_t s_cur_rot = RM690B0_ROTATION_0;

// --- LVGL Callbacks ---

static void lvgl_vsync_cb(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_vsync_sem) xSemaphoreGiveFromISR(s_vsync_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

static void lvgl_power_cb(bool on, void *arg) {
    ESP_LOGI(TAG, "Display Power: %s", on ? "ON" : "OFF");
    // We could pause/resume LVGL timer here if desired
}

static void lvgl_error_cb(int error_code, void *arg) {
    ESP_LOGE(TAG, "Display Error: %d", error_code);
}

static void lvgl_rotation_cb(rm690b0_rotation_t rot, void *arg) {
    if (!lv_disp) return;
    
    s_cur_rot = rot;

    uint16_t w = rm690b0_get_width();
    uint16_t h = rm690b0_get_height();
    
    ESP_LOGI(TAG, "Rotation changed to %d. Updating LVGL resolution to %dx%d", rot, w, h);
    
    lvgl_mgr_lock();
    lv_display_set_resolution(lv_disp, w, h);
    // Invalidate the whole screen to force a full redraw
    lv_obj_invalidate(lv_screen_active());
    lvgl_mgr_unlock();
}

static void lvgl_flush_done_cb(void *user_ctx) {
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
}

static void lvgl_rounder_cb(lv_event_t *e) {
    lv_area_t * area = lv_event_get_param(e);
    
    // Round x1 down to even
    if(area->x1 & 1) area->x1--;
    
    // Round x2 up to odd (so width = x2 - x1 + 1 is even)
    if(!(area->x2 & 1)) area->x2++;

    // Round y1 down to even (Required for Landscape/MV modes where Y->Col)
    if(area->y1 & 1) area->y1--;

    // Round y2 up to odd (so height = y2 - y1 + 1 is even)
    if(!(area->y2 & 1)) area->y2++;
    
    // Ensure we don't go out of bounds (assuming display size is even)
    // If display width is odd, this logic might need adjustment, but 600 is even.
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    static uint32_t last_flush = 0;
    if (esp_log_timestamp() - last_flush > 5000) {
        ESP_LOGI(TAG, "LVGL Flush active (last area: %d,%d to %d,%d)", 
                 (int)area->x1, (int)area->y1, (int)area->x2, (int)area->y2);
        last_flush = esp_log_timestamp();
    }
    // Swap bytes for RGB565 because the RM690B0 expects Big Endian
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    // Only swap if we are in RGB565 mode (which we should be)
    if (lv_display_get_color_format(disp) == LV_COLOR_FORMAT_RGB565) {
        // Since we use the rounder callback, w should always be even.
        // w * 2 is therefore always a multiple of 4.
        // So stride (aligned to 4) == w * 2.
        // No packing needed.
        
        lv_draw_sw_rgb565_swap(px_map, w * h);
    } else {
        ESP_LOGW(TAG, "Flush called with unexpected format: %d", lv_display_get_color_format(disp));
    }

    // Wait for VSYNC (TE) to prevent tearing
    // We wait up to 20ms (approx 1 frame at 50Hz)
    // NOTE: Waiting on every chunk causes a "slow wipe" effect if the frame is split into many chunks.
    // For now, we disable this to prioritize speed.
    /*
    if (s_vsync_sem) {
        xSemaphoreTake(s_vsync_sem, pdMS_TO_TICKS(20));
    }
    */

    // Map LVGL flush to our HAL flush (Async DMA)
    hal_mgr_display_flush_async(area->x1, area->y1, area->x2, area->y2, px_map, lvgl_flush_done_cb, disp);
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    static uint32_t call_count = 0;
    call_count++;

    int16_t x, y;
    bool pressed = hal_mgr_touch_read(&x, &y);
    
    if (pressed) {
        // --- TOUCH COORDINATE MAPPING (VERIFIED 2026-01-04) ---
        // Rotation 0 (Landscape): Native driver output is correct.
        // Rotation 1 (Portrait): X is mirrored, Y is correct.
        // Rotation 2 (Landscape Inv): X is correct, Y is reversed.
        // Rotation 3 (Portrait Inv): X is mirrored, Y is correct.

        // Fix for mirrored X in portrait modes (1 and 3)
        if (s_cur_rot == RM690B0_ROTATION_90 || s_cur_rot == RM690B0_ROTATION_270) {
             uint16_t w = rm690b0_get_width();
             if (x < w) {
                 x = w - 1 - x;
             }
        }

        // Fix for reversed Y in rotation 2 only
        if (s_cur_rot == RM690B0_ROTATION_180) {
             uint16_t h = rm690b0_get_height();
             if (y < h) {
                 y = h - 1 - y;
             }
        }

        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        // Log every press to be sure
        ESP_LOGI(TAG, "LVGL Input Pressed: x=%d, y=%d", (int)x, (int)y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        // Keep the last coordinates to avoid jumping to 0,0 on release
        // data->point.x = last_x;
        // data->point.y = last_y;
    }

    // Log every 100 calls (~3 seconds at 30ms) to confirm polling is alive
    if (call_count % 100 == 0) {
        ESP_LOGI(TAG, "LVGL polling touch heartbeat: pressed=%d, x=%d, y=%d", pressed, x, y);
    }
}

// --- LVGL Timer Task ---
static void lvgl_timer_task(void *arg) {
    ESP_LOGI(TAG, "Starting LVGL timer task");
    uint32_t last_heartbeat = 0;
    uint32_t last_tick = esp_log_timestamp();

    while (1) {
        uint32_t now = esp_log_timestamp();
        lv_tick_inc(now - last_tick);
        last_tick = now;

        lvgl_mgr_lock();
        uint32_t sleep_ms = lv_timer_handler();
        lvgl_mgr_unlock();
        
        if (now - last_heartbeat > 5000) {
            ESP_LOGI(TAG, "LVGL timer task heartbeat, next sleep: %lu ms", (unsigned long)sleep_ms);
            last_heartbeat = now;
        }

        if (sleep_ms < 1) sleep_ms = 1;
        if (sleep_ms > 100) sleep_ms = 100; 
        vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    }
}

void lvgl_mgr_lock(void) {
    if (lvgl_mux) xSemaphoreTakeRecursive(lvgl_mux, portMAX_DELAY);
}

void lvgl_mgr_unlock(void) {
    if (lvgl_mux) xSemaphoreGiveRecursive(lvgl_mux);
}

void lvgl_mgr_show_rainbow(void) {
    // Bypass LVGL to show the hardware test pattern
    rm690b0_draw_test_pattern();
}

esp_err_t bsp_init(void) {

    // Initialize HAL
    if (hal_mgr_init() != ESP_OK) {
        ESP_LOGE(TAG, "HAL Init failed");
        return ESP_FAIL;
    }

    // Initialize LVGL Library
    lv_init();

    // Create Mutex
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    if (!lvgl_mux) return ESP_ERR_NO_MEM;

    // Create VSYNC semaphore
    s_vsync_sem = xSemaphoreCreateBinary();
    
    // Register Callbacks
    hal_mgr_register_display_vsync_callback(lvgl_vsync_cb, NULL);
    hal_mgr_register_display_power_callback(lvgl_power_cb, NULL);
    hal_mgr_register_display_error_callback(lvgl_error_cb, NULL);
    hal_mgr_register_rotation_callback(lvgl_rotation_cb, NULL);

    // Setup Display
    uint16_t w = rm690b0_get_width();
    uint16_t h = rm690b0_get_height();
    ESP_LOGI(TAG, "Creating display with resolution: %u x %u", w, h);
    
    lv_disp = lv_display_create(w, h);
    lv_display_set_default(lv_disp);
    lv_display_set_flush_cb(lv_disp, lvgl_flush_cb);
    lv_display_add_event_cb(lv_disp, lvgl_rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    
    // Force RGB565 format for the display
    lv_display_set_color_format(lv_disp, LV_COLOR_FORMAT_RGB565);
    
    ESP_LOGI(TAG, "LVGL Config: LV_COLOR_DEPTH=%d, Native Format=%d", LV_COLOR_DEPTH, LV_COLOR_FORMAT_NATIVE);
    ESP_LOGI(TAG, "Display Format set to: %d (RGB565=%d)", lv_display_get_color_format(lv_disp), LV_COLOR_FORMAT_RGB565);

    // Allocate draw buffers (Internal RAM for speed and DMA compatibility)
    // Using ~24KB buffer (approx 20 lines at 600px width) to be safe and avoid SPI transaction limits
    // 600 * 20 * 2 = 24,000 bytes
    size_t buf_size = w * 20 * 2; // Explicitly 2 bytes per pixel
    ESP_LOGI(TAG, "Allocating LVGL draw buffers: %d bytes (Explicit 2 bytes/px)", (int)buf_size);

    void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf1) return ESP_ERR_NO_MEM;

    void *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf2) {
        ESP_LOGW(TAG, "Failed to allocate second buffer, using single buffer mode");
        lv_display_set_buffers(lv_disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    } else {
        lv_display_set_buffers(lv_disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    }

    // Setup Input (Touch)
    ESP_LOGI(TAG, "Registering LVGL input device...");
    lv_touch = lv_indev_create();
    if (lv_touch) {
        lv_indev_set_type(lv_touch, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(lv_touch, lvgl_touch_read_cb);
        lv_indev_set_display(lv_touch, lv_disp);
        
        lv_timer_t * read_timer = lv_indev_get_read_timer(lv_touch);
        if (read_timer) {
            lv_timer_set_period(read_timer, 30);
            ESP_LOGI(TAG, "Touch timer period set to 30ms");
        } else {
            ESP_LOGW(TAG, "Failed to get touch read timer!");
        }
    } else {
        ESP_LOGE(TAG, "Failed to create LVGL input device!");
    }

    // Show Rainbow Test Pattern (Hardware diagnostic)
    lvgl_mgr_show_rainbow();
    vTaskDelay(pdMS_TO_TICKS(1500)); // Show for 1.5 seconds as requested

    // Start LVGL Task
    // Increased stack to 32KB for GIF decoding and file operations
    xTaskCreatePinnedToCore(lvgl_timer_task, "lvgl_task", 32768, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "LVGL Manager Initialized");
    return ESP_OK;
}

