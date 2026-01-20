#ifndef HAL_MGR_H
#define HAL_MGR_H

#include <stdbool.h>
#include "esp_err.h"
#include "cst226se.h"
#include "rm690b0.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*hal_mgr_usb_event_cb_t)(bool plugged, void *user_ctx);
typedef void (*hal_mgr_charge_event_cb_t)(bool charging, void *user_ctx);
typedef void (*hal_mgr_battery_event_cb_t)(bool present, void *user_ctx);
typedef void (*hal_mgr_rotation_cb_t)(rm690b0_rotation_t rot, void *user_ctx);

esp_err_t hal_mgr_init(void);
void hal_mgr_set_rotation(rm690b0_rotation_t rot);
rm690b0_rotation_t hal_mgr_get_rotation(void);

void hal_mgr_register_usb_callback(hal_mgr_usb_event_cb_t cb, void *user_ctx);
void hal_mgr_register_charge_callback(hal_mgr_charge_event_cb_t cb, void *user_ctx);
void hal_mgr_register_battery_callback(hal_mgr_battery_event_cb_t cb, void *user_ctx);
void hal_mgr_register_rotation_callback(hal_mgr_rotation_cb_t cb, void *user_ctx);

// User test hooks for display/touch
void hal_mgr_register_touch_callback(cst226se_event_callback_t cb, void *user_ctx);
void hal_mgr_register_display_vsync_callback(rm690b0_vsync_cb_t cb, void *user_ctx);
void hal_mgr_register_display_power_callback(rm690b0_power_cb_t cb, void *user_ctx);
void hal_mgr_register_display_error_callback(rm690b0_error_cb_t cb, void *user_ctx);

// --- SD Card ---
esp_err_t hal_mgr_sd_init(void);
bool hal_mgr_sd_is_mounted(void);

// --- LED Control ---
typedef enum {
    HAL_LED_STATUS_OFF,
    HAL_LED_STATUS_CHARGING,      // Breath (PWM)
    HAL_LED_STATUS_FULL,          // Solid On
    HAL_LED_STATUS_FAULT_GENERIC, // 500ms blink
    HAL_LED_STATUS_FAULT_WDT,     // 100ms frantic blink
    HAL_LED_STATUS_FAULT_OVP,     // 2000ms slow blink
    HAL_LED_STATUS_FAULT_TEMP,    // 1000ms standard blink
} hal_led_status_t;

/**
 * @brief Set the Status LED to a specific system state.
 * This encapsulates the blink patterns and PWM logic.
 * @param status The target system status to indicate.
 */
void hal_mgr_set_led_status(hal_led_status_t status);

/**
 * @brief Lock the LED control to manual mode.
 * When locked, the auto-status task will not update the LED state.
 * @param locked true to lock (manual control), false to unlock (auto status indication)
 */
void hal_mgr_lock_led(bool locked);

// --- GFX Stack Integration (LVGL/BSP) ---
typedef void (*hal_mgr_done_cb_t)(void *user_ctx);

/**
 * @brief Flush a buffer to the display. 
 * This is the primary function used by LVGL's flush_cb.
 */
void hal_mgr_display_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const void *color_p);

/**
 * @brief Async flush a buffer to the display. 
 * This is the primary function used by LVGL's flush_cb for DMA.
 */
void hal_mgr_display_flush_async(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const void *color_p, hal_mgr_done_cb_t cb, void *user_ctx);

/**
 * @brief Check if the display is busy (for DMA transfers)
 */
bool hal_mgr_display_is_busy(void);

/**
 * @brief Read the latest touch state.
 * This is the primary function used by LVGL's read_cb.
 * @return true if pressed, false otherwise
 */
bool hal_mgr_touch_read(int16_t *x, int16_t *y);

#ifdef __cplusplus
}
#endif

#endif // HAL_MGR_H
