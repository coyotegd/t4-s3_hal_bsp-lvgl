#include "ui_avi.h"
#include "avi_mjpg_mgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "misc/cache/instance/lv_image_cache.h"

static const char *TAG = "ui_avi";

typedef struct {
    avi_mjpg_handle_t avi_handle;
    lv_timer_t *timer;
    lv_image_dsc_t img_dsc;
    uint32_t frame_delay_ms;
    bool is_playing;
} ui_avi_t;

static void ui_avi_cleanup(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    ui_avi_t * avi = (ui_avi_t *)lv_obj_get_user_data(obj);
    if (avi) {
        if (avi->timer) {
            lv_timer_delete(avi->timer);
            avi->timer = NULL;
        }
        if (avi->avi_handle) {
            avi_mjpg_close(avi->avi_handle);
            avi->avi_handle = NULL;
        }
        free(avi);
        lv_obj_set_user_data(obj, NULL);
    }
}

static void avi_timer_cb(lv_timer_t * timer) {
    lv_obj_t * obj = (lv_obj_t *)lv_timer_get_user_data(timer);
    ui_avi_t * avi = (ui_avi_t *)lv_obj_get_user_data(obj);
    
    if (!avi || !avi->is_playing || !avi->avi_handle) return;
    
    uint8_t *frame_data = NULL;
    size_t frame_size = 0;

    // Drop cache BEFORE data might be realloc/freed by get_next_frame
    lv_image_cache_drop(&avi->img_dsc);
    
    // Get next frame
    esp_err_t ret = avi_mjpg_get_next_frame(avi->avi_handle, &frame_data, &frame_size);
    
    // Auto-loop on EOF
    if (ret == ESP_ERR_INVALID_STATE) {
        avi_mjpg_rewind(avi->avi_handle);
        // Try getting first frame again immediately
        ret = avi_mjpg_get_next_frame(avi->avi_handle, &frame_data, &frame_size);
    }

    if (ret == ESP_OK) {
        // Update Descriptor
        avi->img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        avi->img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
        avi->img_dsc.header.flags = 0;
        avi->img_dsc.data_size = frame_size;
        avi->img_dsc.data = frame_data;
        
        // Debug: Check header bytes (first pixel)
        if (frame_size > 4) {
             uint16_t *pixels = (uint16_t*)frame_data;
             ESP_LOGD(TAG, "First Pixel: 0x%04X", pixels[0]);
             // ESP_LOGD(TAG, "Frame Data Size: %u", (unsigned int)frame_size);
        }

        // Set Source
        lv_image_set_src(obj, &avi->img_dsc);
        
        // Force inval
        lv_obj_invalidate(obj);
    } else {
        ESP_LOGE(TAG, "AVI Error or empty file %d", ret);
        ui_avi_pause(obj);
    }
}

lv_obj_t * ui_avi_create(lv_obj_t * parent) {
    lv_obj_t * obj = lv_image_create(parent);
    
    ui_avi_t * avi = (ui_avi_t *)calloc(1, sizeof(ui_avi_t));
    if (!avi) return obj;

    lv_obj_set_user_data(obj, avi);
    lv_obj_add_event_cb(obj, ui_avi_cleanup, LV_EVENT_DELETE, NULL);
    
    return obj;
}

void ui_avi_set_src(lv_obj_t * obj, const char * src) {
    ui_avi_t * avi = (ui_avi_t *)lv_obj_get_user_data(obj);
    if (!avi) return;
    
    // Close existing
    if (avi->avi_handle) {
        avi_mjpg_close(avi->avi_handle);
        avi->avi_handle = NULL;
    }
    
    const char * path = src;
    if (strncmp(path, "S:", 2) == 0) path += 2;

    avi_mjpg_config_t config = {0};
    avi->avi_handle = avi_mjpg_open(path, &config);
    
    if (avi->avi_handle) {
         // Default to 30fps if not specified or 0
        avi->frame_delay_ms = (config.fps > 0) ? (1000 / config.fps) : 33;
        
        // Populate descriptor headers initially
        avi->img_dsc.header.w = config.width;
        avi->img_dsc.header.h = config.height;
        avi->img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
        avi->img_dsc.header.stride = config.width * 2;

        ESP_LOGI(TAG, "AVI Set Src: %s (%dx%d @ %ld ms)", path, config.width, config.height, avi->frame_delay_ms);
        
        // Explicitly set object size to match video
        lv_obj_set_size(obj, config.width, config.height);

        // Load first frame immediately (Poster Frame)
        uint8_t *frame_data = NULL;
        size_t frame_size = 0;
        if (avi_mjpg_get_next_frame(avi->avi_handle, &frame_data, &frame_size) == ESP_OK) {
             avi->img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
             avi->img_dsc.data_size = frame_size;
             avi->img_dsc.data = frame_data;
             lv_image_set_src(obj, &avi->img_dsc);
             lv_obj_invalidate(obj);
        } else {
             ESP_LOGW(TAG, "AVI failed to load first frame (empty?)");
        }
    } else {
        ESP_LOGE(TAG, "Failed to open AVI file: %s", src);
    }
}

void ui_avi_play(lv_obj_t * obj) {
    ui_avi_t * avi = (ui_avi_t *)lv_obj_get_user_data(obj);
    if (!avi || !avi->avi_handle) return;
    
    if (!avi->timer) {
        avi->timer = lv_timer_create(avi_timer_cb, avi->frame_delay_ms, obj);
    } else {
        lv_timer_set_period(avi->timer, avi->frame_delay_ms);
        lv_timer_resume(avi->timer);
    }
    avi->is_playing = true;
}

void ui_avi_pause(lv_obj_t * obj) {
    ui_avi_t * avi = (ui_avi_t *)lv_obj_get_user_data(obj);
    if (!avi) return;
    if (avi->timer) {
        lv_timer_pause(avi->timer);
    }
    avi->is_playing = false;
}

void ui_avi_stop(lv_obj_t * obj) {
    ui_avi_pause(obj);
    ui_avi_t * avi = (ui_avi_t *)lv_obj_get_user_data(obj);
    if (!avi) return;
    if (avi->avi_handle) {
        avi_mjpg_rewind(avi->avi_handle);
    }
}
