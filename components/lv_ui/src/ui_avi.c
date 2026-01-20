#include "ui_avi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "misc/cache/instance/lv_image_cache.h"
#include "jpeglib.h"
#include "jerror.h"
#include <setjmp.h>

// Max resolution 600x450 * 2 bytes/pixel = 540,000 bytes
// Round up to 600KB for safety
#define AVI_PIXEL_BUFFER_SIZE (600 * 1024) 
#define AVI_WORK_BUFFER_SIZE (100 * 1024)

static const char *TAG = "ui_avi";
#define LOG_AVI(...) ESP_LOGI(TAG, __VA_ARGS__)

// Error handling for libjpeg to prevent exit()
struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

static void my_error_exit(j_common_ptr cinfo) {
  struct my_error_mgr *myerr = (struct my_error_mgr *)cinfo->err;
  (*cinfo->err->output_message)(cinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

typedef struct {
    FILE *f;
    uint8_t *frame_buffer[2]; // Uncompressed RGB565 buffers
    uint8_t *work_buffer;     // Compressed JPEG data
    lv_timer_t *timer;
    lv_image_dsc_t img_dsc[2]; // Double buffered descriptors
    uint8_t dsc_idx;
    uint32_t movi_start_offset;
    uint32_t movi_end_offset;
    uint32_t current_offset;
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
        if (avi->f) {
            fclose(avi->f);
            avi->f = NULL;
        }
        for(int i=0; i<2; i++) {
            if (avi->frame_buffer[i]) {
                free(avi->frame_buffer[i]);
                avi->frame_buffer[i] = NULL;
            }
        }
        if (avi->work_buffer) {
            free(avi->work_buffer);
            avi->work_buffer = NULL;
        }
        free(avi);
        lv_obj_set_user_data(obj, NULL);
    }
}

static uint32_t read_u32(FILE *f) {
    uint32_t val;
    if (fread(&val, 1, 4, f) != 4) return 0;
    return val; // AVI is little-endian, ESP32 is little-endian (mostly), usually matches
}

static bool parse_avi_chunks(ui_avi_t *avi) {
    if (!avi->f) return false;
    fseek(avi->f, 0, SEEK_SET);
    
    uint32_t riff = read_u32(avi->f);
    if (riff != 0x46464952) { // "RIFF"
        ESP_LOGE(TAG, "Not a RIFF file");
        return false;
    }
    
    read_u32(avi->f); // file size
    
    uint32_t avi_sig = read_u32(avi->f);
    if (avi_sig != 0x20495641) { // "AVI "
        ESP_LOGE(TAG, "Not an AVI file");
        return false;
    }
    
    bool movi_found = false;
    long file_size_check = 0;
    fseek(avi->f, 0, SEEK_END);
    file_size_check = ftell(avi->f);
    fseek(avi->f, 12, SEEK_SET); // Skip RIFF header

    while (ftell(avi->f) < file_size_check) {
        uint32_t id;
        uint32_t size;
        
        if (fread(&id, 1, 4, avi->f) != 4) break;
        if (fread(&size, 1, 4, avi->f) != 4) break;

        long chunk_start = ftell(avi->f);
        // Align to even bytes
        long next_chunk = chunk_start + size + (size & 1);
        
        if (next_chunk > file_size_check + 8) { // Allow small tolerance
             ESP_LOGW(TAG, "Chunk size %u extends past EOF", (unsigned int)size);
             break; 
        }

        if (id == 0x5453494C) { // "LIST"
            uint32_t type = read_u32(avi->f);
            
            if (type == 0x6C726468) { // "hdrl"
                // We are inside hdrl, let's look for avih
                long hdrl_end = next_chunk;
                long current_pos = ftell(avi->f);
                
                while (current_pos < hdrl_end - 8) {
                    uint32_t sub_id;
                    uint32_t sub_size;
                    
                    if (fread(&sub_id, 1, 4, avi->f) != 4) break;
                    if (fread(&sub_size, 1, 4, avi->f) != 4) break;
                    
                    if (sub_size == 0 && sub_id == 0) break; // Avoid infinite loop on zeros

                    long sub_next = ftell(avi->f) + sub_size + (sub_size & 1);
                    
                    if (sub_id == 0x68697661) { // "avih"
                        if (sub_size >= 4) {
                            uint32_t usec_per_frame = read_u32(avi->f);
                            if (usec_per_frame > 0) {
                                avi->frame_delay_ms = usec_per_frame / 1000;
                                ESP_LOGI(TAG, "AVI Header: %lu us/frame -> %lu ms delay", (unsigned long)usec_per_frame, (unsigned long)avi->frame_delay_ms);
                            }
                        }
                    }
                    
                    fseek(avi->f, sub_next, SEEK_SET);
                    current_pos = ftell(avi->f);
                }
                // Done with hdrl, move to next main chunk
                fseek(avi->f, next_chunk, SEEK_SET);
                continue;
            }
            
            if (type == 0x69766F6D) { // "movi"
                avi->movi_start_offset = ftell(avi->f);
                avi->movi_end_offset = next_chunk - 4; // -4 because we read type
                avi->current_offset = avi->movi_start_offset;
                movi_found = true;
                ESP_LOGI(TAG, "MOVI chunk found: Start=%ld End=%ld", avi->movi_start_offset, avi->movi_end_offset);
                // Continue scanning to find other chunks if necessary, or break if satisfied
                // For safety, let's break if we found movi and header (optional, but robust)
            }
        }
        
        fseek(avi->f, next_chunk, SEEK_SET);
    }
    
    return movi_found;
}


static void avi_timer_cb(lv_timer_t * timer) {
    lv_obj_t * obj = (lv_obj_t *)lv_timer_get_user_data(timer);
    ui_avi_t * avi = (ui_avi_t *)lv_obj_get_user_data(obj);
    
    if (!avi || !avi->is_playing || !avi->f) return;
    
    // Seek to current offset
    fseek(avi->f, avi->current_offset, SEEK_SET);
    
    int chunks_checked = 0;
    while (ftell(avi->f) < avi->movi_end_offset) {
        if (chunks_checked++ > 200) { // Limit search (increased to handle audio/padding gaps)
             LOG_AVI("Search limit reached (200 chunks scanned without valid frame).");
             break;
        }

        uint32_t id = read_u32(avi->f);
        uint32_t size = read_u32(avi->f);
        
        long payload_pos = ftell(avi->f);
        long next_chunk = payload_pos + size + (size & 1); // Align to even
        
        // Debug first few chunks ended
        // if (chunks_checked < 5) { ... }

        // Check for "00dc" (Compressed video frame)
        // 00dc = 0x63643030 (Little Endian)
        if ((id & 0xFFFF0000) == 0x63640000) { 
             // LOG_AVI("Chunk %c%c%c%c found, Size=%u", (char)(id), (char)(id>>8), (char)(id>>16), (char)(id>>24), (unsigned int)size);
             if (size > 0 && size <= AVI_WORK_BUFFER_SIZE) {
                 // Read into work buffer
                 size_t read_len = fread(avi->work_buffer, 1, size, avi->f);
                 if (read_len == size) {
                     // Verify JPEG header (SOI)
                     if (avi->work_buffer[0] == 0xFF && avi->work_buffer[1] == 0xD8) {
                         
                         // Decode JPEG using libjpeg-turbo
                         struct jpeg_decompress_struct cinfo;
                         struct my_error_mgr jerr;
                         int64_t t_start = esp_timer_get_time();
                         
                         cinfo.err = jpeg_std_error(&jerr.pub);
                         jerr.pub.error_exit = my_error_exit;
                         
                         if (setjmp(jerr.setjmp_buffer)) {
                             // If we get here, the JPEG code has signaled an error.
                             ESP_LOGE(TAG, "JPEG decode error");
                             jpeg_destroy_decompress(&cinfo);
                             goto skip_frame;
                         }
                         
                         jpeg_create_decompress(&cinfo);
                         jpeg_mem_src(&cinfo, avi->work_buffer, size);
                         jpeg_read_header(&cinfo, TRUE);
                         
                         // Configure for RGB565 Output
                         cinfo.out_color_space = JCS_RGB565;
                         cinfo.dct_method = JDCT_IFAST; // Speed over quality
                         
                         jpeg_start_decompress(&cinfo);
                         
                         uint32_t w = cinfo.output_width;
                         uint32_t h = cinfo.output_height;
                         
                         // Update descriptor if needed
                         if (avi->img_dsc[avi->dsc_idx].header.w != w || avi->img_dsc[avi->dsc_idx].header.h != h) {
                             avi->img_dsc[avi->dsc_idx].header.w = w;
                             avi->img_dsc[avi->dsc_idx].header.h = h;
                         }
                         
                         // Decompress into the large frame buffer
                         uint8_t *dest = avi->frame_buffer[avi->dsc_idx];
                         JSAMPROW row_pointer[1];
                         int row_stride = w * 2; // RGB565
                         
                         while (cinfo.output_scanline < h) {
                             row_pointer[0] = &dest[cinfo.output_scanline * row_stride];
                             jpeg_read_scanlines(&cinfo, row_pointer, 1);
                         }
                         
                         jpeg_finish_decompress(&cinfo);
                         jpeg_destroy_decompress(&cinfo);
                         
                         int64_t t_end = esp_timer_get_time();

                         avi->img_dsc[avi->dsc_idx].data_size = w * h * 2; // Valid pixel data size

                         // Refresh LVGL image
                         // Vital: Drop cache for the reused descriptor pointer
                         lv_image_cache_drop(&avi->img_dsc[avi->dsc_idx]);
                         
                         static int frame_count = 0;
                         if (++frame_count % 15 == 0) {
                             ESP_LOGI(TAG, "AVI Frame: %d, Size: %u, Decode: %lld ms", frame_count, (unsigned int)size, (t_end - t_start)/1000);
                         }

                         // Re-apply source and invalidate to force redraw
                         // Using the toggled descriptor address trick
                         lv_image_set_src(obj, &avi->img_dsc[avi->dsc_idx]); 
                         
                         // Toggle index for next frame
                         avi->dsc_idx = (avi->dsc_idx + 1) % 2;

                         lv_obj_invalidate(obj);
                         
                         // Save state for next tick
                         avi->current_offset = next_chunk;
                         return; // Displayed a frame, yield to LVGL

                     } else {
                         // Not a JPEG
                     }
                 } else {
                     ESP_LOGE(TAG, "Read mismatch! Req=%u Got=%u at %ld", (unsigned int)size, (unsigned int)read_len, payload_pos);
                 }
                   skip_frame:;
             } else {
                 if (size > AVI_WORK_BUFFER_SIZE) {
                    ESP_LOGE(TAG, "Frame size %u too big (Max %d)", (unsigned int)size, AVI_WORK_BUFFER_SIZE);
                 }
             }
        }
        
        // Move to next chunk
        fseek(avi->f, next_chunk, SEEK_SET);
        avi->current_offset = next_chunk;
    }
    
    // End of movie loop
    avi->current_offset = avi->movi_start_offset;
}

lv_obj_t * ui_avi_create(lv_obj_t * parent) {
    lv_obj_t * obj = lv_image_create(parent);
    
    ui_avi_t * avi = (ui_avi_t *)calloc(1, sizeof(ui_avi_t));
    if (!avi) return NULL;
    
    // Allocate frame buffers in PSRAM
    // We allocate TWO buffers to ensure that when we switch descriptors,
    // the 'data' pointer also changes. explicit cache invalidation 
    // works better when the underlying data pointer is different.
    for(int i=0; i<2; i++) {
        avi->frame_buffer[i] = (uint8_t *)heap_caps_malloc(AVI_PIXEL_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        if (!avi->frame_buffer[i]) {
            ESP_LOGE(TAG, "Failed to allocate AVI frame buffer %d in PSRAM", i);
            return NULL; 
        }
    }

    // Allocate work buffer for compressed data
    avi->work_buffer = (uint8_t *)heap_caps_malloc(AVI_WORK_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!avi->work_buffer) {
        ESP_LOGE(TAG, "Failed to allocate AVI work buffer");
        return NULL;
    }
    
    // Initialize standard Image Descriptors (Double Buffered)
    for(int i=0; i<2; i++) {
        avi->img_dsc[i].header.magic = LV_IMAGE_HEADER_MAGIC;
        avi->img_dsc[i].header.cf = LV_COLOR_FORMAT_RGB565; 
        avi->img_dsc[i].header.w = 300; 
        avi->img_dsc[i].header.h = 300; 
        avi->img_dsc[i].header.stride = 0; // LVGL will calculate stride from width
        avi->img_dsc[i].data = avi->frame_buffer[i]; // POINT TO SEPARATE BUFFERS
        avi->img_dsc[i].data_size = 0;
    }

    avi->frame_delay_ms = 33; // Default to ~30fps, will be overridden by header
    
    lv_obj_set_user_data(obj, avi);
    lv_obj_add_event_cb(obj, ui_avi_cleanup, LV_EVENT_DELETE, NULL);
    
    return obj;
}

void ui_avi_set_src(lv_obj_t * obj, const char * src) {
    ui_avi_t * avi = (ui_avi_t *)lv_obj_get_user_data(obj);
    if (!avi) return;
    
    if (avi->f) {
        fclose(avi->f);
        avi->f = NULL;
    }
    
    // Handle LVGL path (e.g. "S:path" or "S:/path") by stripping driver letter if present
    const char * real_path = src;
    if (src[0] && src[1] == ':') {
        real_path = src + 2;
    }
    
    char full_path[512];
    // If path is relative (no leading slash), prepend /sdcard/
    if (real_path[0] != '/') {
        snprintf(full_path, sizeof(full_path), "/sdcard/%s", real_path);
        avi->f = fopen(full_path, "rb");
    } else {
        avi->f = fopen(real_path, "rb");
    }

    if (!avi->f) {
        // Fallback: try opening EXACTLY as passed (after stripping S:) just in case
        if (real_path[0] != '/') {
             ESP_LOGW(TAG, "Failed to open AVI at %s. Retrying relative: %s", full_path, real_path);
             avi->f = fopen(real_path, "rb");
        }
    }

    if (!avi->f) {
        ESP_LOGE(TAG, "Failed to open AVI: %s", src);
        return;
    }
    
    if (parse_avi_chunks(avi)) {
        ESP_LOGI(TAG, "AVI MOVI list found at %u", (unsigned int)avi->movi_start_offset);
        avi->is_playing = true;
        
        if (!avi->timer) {
            avi->timer = lv_timer_create(avi_timer_cb, avi->frame_delay_ms, obj);
        } else {
            // Update existing timer period if file changed
             lv_timer_set_period(avi->timer, avi->frame_delay_ms);
        }
    } else {
        ESP_LOGE(TAG, "Bad AVI structure");
        fclose(avi->f);
        avi->f = NULL;
    }
}

void ui_avi_play(lv_obj_t * obj) {
    ui_avi_t * avi = (ui_avi_t *)lv_obj_get_user_data(obj);
    if (avi) avi->is_playing = true;
}

void ui_avi_pause(lv_obj_t * obj) {
    ui_avi_t * avi = (ui_avi_t *)lv_obj_get_user_data(obj);
    if (avi) avi->is_playing = false;
}

void ui_avi_stop(lv_obj_t * obj) {
    ui_avi_t * avi = (ui_avi_t *)lv_obj_get_user_data(obj);
    if (avi) {
        avi->is_playing = false;
        avi->current_offset = avi->movi_start_offset;
    }
}
