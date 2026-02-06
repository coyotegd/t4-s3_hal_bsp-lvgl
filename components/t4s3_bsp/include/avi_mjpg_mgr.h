#ifndef AVI_MJPG_MGR_H
#define AVI_MJPG_MGR_H

#include "esp_err.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct avi_mjpg_context_t* avi_mjpg_handle_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t fps;
} avi_mjpg_config_t;

/**
 * @brief Open an AVI file for MJPEG playback
 * 
 * @param file_path Path to the AVI file (e.g., "/sdcard/video.avi")
 * @return avi_mjpg_handle_t Handle to the AVI context, or NULL on failure
 */
avi_mjpg_handle_t avi_mjpg_open(const char *file_path, avi_mjpg_config_t *config);

/**
 * @brief Get the next JPEG frame from the AVI file
 * 
 * This function reads the next video chunk. If the frame is missing JPEG headers
 * (like Huffman tables), it will attempt to inject them to make the buffer
 * valid for standard decoders.
 * 
 * @param handle AVI handle
 * @param out_buffer Pointer to the data (internal buffer, do not free)
 * @param out_size Pointer to store the size of the frame
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if EOF, other errors otherwise
 */
esp_err_t avi_mjpg_get_next_frame(avi_mjpg_handle_t handle, uint8_t **out_buffer, size_t *out_size);

/**
 * @brief Reset playback to the beginning of the video stream
 * 
 * @param handle AVI handle
 * @return ESP_OK on success
 */
esp_err_t avi_mjpg_rewind(avi_mjpg_handle_t handle);

/**
 * @brief Close the AVI file and free resources
 * 
 * @param handle AVI handle
 */
void avi_mjpg_close(avi_mjpg_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // AVI_MJPG_MGR_H
