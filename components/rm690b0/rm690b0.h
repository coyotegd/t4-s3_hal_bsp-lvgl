#ifndef RM690B0_H
#define RM690B0_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Physical Driver Hardware Limits
#define RM690B0_HW_WIDTH     520
#define RM690B0_HW_HEIGHT    640

// Actual physical screen size hardware defaults
#define RM690B0_PHYSICAL_W   450
#define RM690B0_PHYSICAL_H   600

// MADCTL bits
#define RM690B0_MADCTL_MY    0x80  // Row Address Order
#define RM690B0_MADCTL_MX    0x40  // Column Address Order
#define RM690B0_MADCTL_MV    0x20  // Row/Column Exchange
#define RM690B0_MADCTL_ML    0x10  // Vertical Refresh Order
#define RM690B0_MADCTL_RGB   0x00  // RGB Order (vs BGR)

// Register Definitions (8-bit)
#define RM690B0_SWRESET      0x01
#define RM690B0_SLPIN        0x10
#define RM690B0_SLPOUT       0x11
#define RM690B0_INVOFF       0x20
#define RM690B0_INVON        0x21
#define RM690B0_DISPOFF      0x28
#define RM690B0_DISPON       0x29
#define RM690B0_CASET        0x2A
#define RM690B0_RASET        0x2B
#define RM690B0_RAMWR        0x2C
#define RM690B0_TEOFF        0x34
#define RM690B0_TEON         0x35
#define RM690B0_MADCTR       0x36
#define RM690B0_COLMOD       0x3A
#define RM690B0_WRDISBV      0x51

typedef enum {
    RM690B0_ROTATION_0   = 0, // Portrait (default) USB on left
    RM690B0_ROTATION_90  = 1, // Landscape
    RM690B0_ROTATION_180 = 2, // Portrait Inverted
    RM690B0_ROTATION_270 = 3  // Landscape Inverted
} rm690b0_rotation_t;

// Callback types
typedef void (*rm690b0_vsync_cb_t)(void *user_ctx);
typedef void (*rm690b0_error_cb_t)(int error_code, void *user_ctx);
typedef void (*rm690b0_power_cb_t)(bool on, void *user_ctx);
typedef void (*rm690b0_done_cb_t)(void *user_ctx);

// Callback registration
void rm690b0_register_vsync_callback(rm690b0_vsync_cb_t cb, void *user_ctx);
void rm690b0_register_error_callback(rm690b0_error_cb_t cb, void *user_ctx);
void rm690b0_register_power_callback(rm690b0_power_cb_t cb, void *user_ctx);

esp_err_t rm690b0_init(void);
esp_err_t rm690b0_deinit(void);
void rm690b0_send_cmd(uint8_t cmd, const uint8_t *data, size_t len);
void rm690b0_send_pixels(const uint8_t *data, size_t len);
void rm690b0_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/**
 * @brief Unified flush function for GFX stacks (LVGL)
 * Sets window and starts pixel transfer.
 */
void rm690b0_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const uint8_t *data);

/**
 * @brief Async flush function for GFX stacks (LVGL)
 * Sets window (blocking) and starts pixel transfer (async DMA).
 * Calls cb when done.
 */
void rm690b0_flush_async(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const uint8_t *data, rm690b0_done_cb_t cb, void *user_ctx);

void rm690b0_set_rotation(rm690b0_rotation_t rot);
rm690b0_rotation_t rm690b0_get_rotation(void);
uint16_t rm690b0_get_width(void);
uint16_t rm690b0_get_height(void);
void rm690b0_read_id(uint8_t *id);
void rm690b0_set_brightness(uint8_t level);
void rm690b0_sleep_mode(bool sleep);
void rm690b0_display_power(bool on);
void rm690b0_invert_colors(bool invert);
void rm690b0_enable_te(bool enable);
void rm690b0_clear_full_display(uint16_t color);
void rm690b0_draw_test_pattern(void);

#ifdef __cplusplus
}
#endif

#endif
