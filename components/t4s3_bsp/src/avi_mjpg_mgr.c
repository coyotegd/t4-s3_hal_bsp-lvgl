#include "avi_mjpg_mgr.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "jpeglib.h"

static const char *TAG = "AVI_MJPG";

#define CHUNK_ID_RIFF 0x46464952 // 'RIFF'
#define CHUNK_ID_AVI  0x20495641 // 'AVI '
#define CHUNK_ID_LIST 0x5453494C // 'LIST'
#define CHUNK_ID_MOVI 0x69766F6D // 'movi'

typedef struct {
    uint32_t id;
    uint32_t size;
} chunk_hdr_t;

struct avi_mjpg_context_t {
    FILE *f;
    uint8_t *frame_buffer;
    size_t frame_buffer_cap;
    size_t movi_start; 
    size_t movi_size;  
    bool in_movi;

    // Decoding - standard libjpeg
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    uint8_t *rgb_buffer;
    uint32_t width;
    uint32_t height;
};

static uint32_t read_u32(FILE *f) {
    uint32_t val;
    if (fread(&val, 1, 4, f) != 4) return 0;
    return val; // AVI is little-endian
}

avi_mjpg_handle_t avi_mjpg_open(const char *file_path, avi_mjpg_config_t *config) {
    FILE *f = fopen(file_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open AVI file: %s", file_path);
        return NULL;
    }

    struct avi_mjpg_context_t *ctx = calloc(1, sizeof(struct avi_mjpg_context_t));
    if (!ctx) {
        fclose(f);
        return NULL;
    }
    ctx->f = f;
    
    // Simple RIFF parser definition
    chunk_hdr_t riff;
    if (fread(&riff, 1, sizeof(riff), f) != sizeof(riff)) goto err;
    if (riff.id != CHUNK_ID_RIFF) {
        ESP_LOGE(TAG, "Not a RIFF file");
        goto err;
    }
    uint32_t format = read_u32(f);
    if (format != CHUNK_ID_AVI) {
        ESP_LOGE(TAG, "Not an AVI file");
        goto err;
    }

    long file_end = ftell(f) + riff.size - 4;
    uint32_t parsed_w = 0, parsed_h = 0;
    uint32_t parsed_fps = 30;

    // Traverse chunks
    while (ftell(f) < file_end) {
        chunk_hdr_t chunk;
        if (fread(&chunk, 1, sizeof(chunk), f) != sizeof(chunk)) break;

        long next_chunk_pos = ftell(f) + chunk.size + (chunk.size & 1); // Word align

        if (chunk.id == CHUNK_ID_LIST) {
            uint32_t list_type = read_u32(f);
            if (list_type == CHUNK_ID_MOVI) {
                ctx->movi_start = ftell(f);
                ctx->movi_size = chunk.size - 4;
                ctx->in_movi = true;
                fseek(f, next_chunk_pos, SEEK_SET);
            } else if (list_type == 0x6C726468) { // 'hdrl'
                // Enter this list to find 'avih'
                continue; 
            } else {
                fseek(f, next_chunk_pos, SEEK_SET);
            }
        } else if (chunk.id == 0x68697661) { // 'avih'
             uint32_t usec_per_frame = read_u32(f);
             fseek(f, 28, SEEK_CUR);
             parsed_w = read_u32(f);
             parsed_h = read_u32(f);
             if (usec_per_frame > 0) parsed_fps = 1000000 / usec_per_frame;

             if (config) {
                 config->width = parsed_w;
                 config->height = parsed_h;
                 config->fps = parsed_fps;
             }
             fseek(f, next_chunk_pos, SEEK_SET);
        } else {
            fseek(f, next_chunk_pos, SEEK_SET);
        }
    }

    if (!ctx->in_movi) {
        ESP_LOGE(TAG, "MOVI list not found");
        goto err;
    }

    ctx->width = parsed_w;
    ctx->height = parsed_h;

    // Move file pointer to start of frames
    fseek(f, ctx->movi_start, SEEK_SET);

    // Initial buffer for JPEG data
    ctx->frame_buffer_cap = 32 * 1024; // Start with 32KB
    ctx->frame_buffer = heap_caps_malloc(ctx->frame_buffer_cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ctx->frame_buffer) goto err;

    // Buffer for RGB565 data
    if (ctx->width > 0 && ctx->height > 0) {
        ctx->rgb_buffer = heap_caps_malloc(ctx->width * ctx->height * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!ctx->rgb_buffer) {
            ESP_LOGE(TAG, "Failed to allocate RGB buffer");
            goto err;
        }
    } else {
         ESP_LOGE(TAG, "Invalid dimensions");
         goto err;
    }

    // Initialize standard libjpeg decompressor
    ctx->cinfo.err = jpeg_std_error(&ctx->jerr);
    jpeg_create_decompress(&ctx->cinfo);

    return ctx;

err:
    if (ctx) {
        if (ctx->frame_buffer) free(ctx->frame_buffer);
        if (ctx->rgb_buffer) free(ctx->rgb_buffer);
        free(ctx);
    }
    if (f) fclose(f);
    return NULL;
}

esp_err_t avi_mjpg_rewind(avi_mjpg_handle_t handle) {
    if (!handle || !handle->f) return ESP_FAIL;
    if (fseek(handle->f, handle->movi_start, SEEK_SET) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t avi_mjpg_get_next_frame(avi_mjpg_handle_t handle, uint8_t **out_buffer, size_t *out_size) {
    if (!handle || !handle->f) return ESP_ERR_INVALID_ARG;

    while (1) {
        long current_pos = ftell(handle->f);
        if (current_pos >= handle->movi_start + handle->movi_size) {
            return ESP_ERR_INVALID_STATE; // EOF
        }

        chunk_hdr_t chunk;
        if (fread(&chunk, 1, sizeof(chunk), handle->f) != sizeof(chunk)) {
            return ESP_ERR_INVALID_STATE; // EOF
        }

        uint8_t *id = (uint8_t*)&chunk.id;
        bool is_video = (id[2] == 'd' && id[3] == 'c');
        size_t pad = (chunk.size & 1);

        if (is_video) {
            size_t data_size = chunk.size;
            if (data_size > handle->frame_buffer_cap) {
                uint8_t *new_buf = heap_caps_realloc(handle->frame_buffer, data_size + 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (!new_buf) return ESP_ERR_NO_MEM;
                handle->frame_buffer = new_buf;
                handle->frame_buffer_cap = data_size + 1024;
            }

            if (fread(handle->frame_buffer, 1, data_size, handle->f) != data_size) {
                 return ESP_ERR_INVALID_STATE; 
            }
            if (pad) fseek(handle->f, 1, SEEK_CUR);

            // Decode with standard libjpeg
            if (!handle->rgb_buffer) return ESP_ERR_INVALID_STATE;

            // Setup libjpeg to read from memory buffer
            jpeg_mem_src(&handle->cinfo, handle->frame_buffer, data_size);
            
            // Read JPEG header
            if (jpeg_read_header(&handle->cinfo, TRUE) != JPEG_HEADER_OK) {
                ESP_LOGE(TAG, "JPEG header read failed");
                jpeg_abort_decompress(&handle->cinfo);
                continue;
            }

            // We want RGB output
            handle->cinfo.out_color_space = JCS_RGB;
            
            // Start decompression
            if (!jpeg_start_decompress(&handle->cinfo)) {
                ESP_LOGE(TAG, "JPEG start decompress failed");
                jpeg_abort_decompress(&handle->cinfo);
                continue;
            }

            // Check dimensions match
            if (handle->cinfo.output_width != handle->width || 
                handle->cinfo.output_height != handle->height) {
                ESP_LOGE(TAG, "Frame size mismatch: expected %dx%d, got %dx%d",
                         handle->width, handle->height,
                         handle->cinfo.output_width, handle->cinfo.output_height);
                jpeg_abort_decompress(&handle->cinfo);
                continue;
            }

            // Allocate temporary RGB24 buffer for scanlines
            int row_stride = handle->width * 3; // RGB = 3 bytes per pixel
            uint8_t *temp_rgb24 = heap_caps_malloc(row_stride * handle->height, 
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!temp_rgb24) {
                ESP_LOGE(TAG, "Failed to allocate temp RGB24 buffer");
                jpeg_abort_decompress(&handle->cinfo);
                return ESP_ERR_NO_MEM;
            }

            // Read scanlines
            JSAMPROW row_pointer[1];
            int row = 0;
            while (handle->cinfo.output_scanline < handle->cinfo.output_height) {
                row_pointer[0] = &temp_rgb24[row * row_stride];
                jpeg_read_scanlines(&handle->cinfo, row_pointer, 1);
                row++;
            }

            // Finish decompression
            jpeg_finish_decompress(&handle->cinfo);

            // Convert RGB24 to RGB565
            uint16_t *rgb565 = (uint16_t *)handle->rgb_buffer;
            uint8_t *rgb24 = temp_rgb24;
            for (int i = 0; i < handle->width * handle->height; i++) {
                uint8_t r = rgb24[0];
                uint8_t g = rgb24[1];
                uint8_t b = rgb24[2];
                rgb565[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                rgb24 += 3;
            }
            heap_caps_free(temp_rgb24);

            *out_size = handle->width * handle->height * 2;
            *out_buffer = handle->rgb_buffer;
            return ESP_OK;
        } else {
            fseek(handle->f, chunk.size + pad, SEEK_CUR);
        }
    }
}

void avi_mjpg_close(avi_mjpg_handle_t handle) {
    if (handle) {
        jpeg_destroy_decompress(&handle->cinfo);
        if (handle->f) fclose(handle->f);
        if (handle->frame_buffer) free(handle->frame_buffer);
        if (handle->rgb_buffer) free(handle->rgb_buffer);
        free(handle);
    }
}
