#include "cst226se.h"
#include "sy6970.h"
#include "rm690b0.h"
#include "sd_card.h"
#include "hal_mgr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"
#include "button_gpio.h"

static const char *TAG = "hal_mgr";

// --- Internal State ---
static cst226se_data_t s_latest_touch = {0};

// --- Touch event task ---
static void hal_mgr_touch_task(void *arg) {
	cst226se_data_t data;
	while (1) {
		if (cst226se_wait_event(1000)) {
			cst226se_read(&data);
		}
	}
}

// --- User test hooks ---
static cst226se_event_callback_t s_user_touch_cb = NULL;
static void *s_user_touch_ctx = NULL;
static rm690b0_vsync_cb_t s_user_vsync_cb = NULL;
static void *s_user_vsync_ctx = NULL;

void hal_mgr_register_touch_callback(cst226se_event_callback_t cb, void *user_ctx) {
	s_user_touch_cb = cb;
	s_user_touch_ctx = user_ctx;
}
void hal_mgr_register_display_vsync_callback(rm690b0_vsync_cb_t cb, void *user_ctx) {
	s_user_vsync_cb = cb;
	s_user_vsync_ctx = user_ctx;
}

// Internal touch event handler (calls user if set)
static void hal_mgr_touch_event_handler(const cst226se_data_t *data, void *user_ctx) {
	s_latest_touch = *data;
	if (s_user_touch_cb) s_user_touch_cb(data, s_user_touch_ctx);
}

// --- GFX Stack Integration ---
void hal_mgr_display_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const void *color_p) {
	rm690b0_flush((uint16_t)x1, (uint16_t)y1, (uint16_t)x2, (uint16_t)y2, (const uint8_t *)color_p);
}

void hal_mgr_display_flush_async(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const void *color_p, hal_mgr_done_cb_t cb, void *user_ctx) {
	rm690b0_flush_async((uint16_t)x1, (uint16_t)y1, (uint16_t)x2, (uint16_t)y2, (const uint8_t *)color_p, (rm690b0_done_cb_t)cb, user_ctx);
}

bool hal_mgr_display_is_busy(void) {
	return false; // Currently using polling (blocking) transfers
}

bool hal_mgr_touch_read(int16_t *x, int16_t *y) {
	if (x) *x = s_latest_touch.x;
	if (y) *y = s_latest_touch.y;
	// Debug log to see if LVGL is calling this
	// ESP_LOGD(TAG, "hal_mgr_touch_read: pressed=%d, x=%d, y=%d", s_latest_touch.pressed, s_latest_touch.x, s_latest_touch.y);
	return s_latest_touch.pressed;
}

// Internal VSYNC event handler (calls user if set)
static void hal_mgr_vsync_handler(void *user_ctx) {
	if (s_user_vsync_cb) s_user_vsync_cb(s_user_vsync_ctx);
}

// Internal Power event handler
// static void hal_mgr_power_handler(bool on, void *user_ctx) {
//     // We could add HAL-level power logic here if needed
//     // For now, just pass through to user callback if registered
//     // Note: We need to store the user callback separately if we want to support it
// }

// --- Event callback storage ---
static hal_mgr_usb_event_cb_t s_usb_cb = NULL;
static void *s_usb_ctx = NULL;
static hal_mgr_charge_event_cb_t s_charge_cb = NULL;
static void *s_charge_ctx = NULL;
static hal_mgr_battery_event_cb_t s_batt_cb = NULL;
static void *s_batt_ctx = NULL;

void hal_mgr_register_display_power_callback(rm690b0_power_cb_t cb, void *user_ctx) {
    rm690b0_register_power_callback(cb, user_ctx);
}

void hal_mgr_register_display_error_callback(rm690b0_error_cb_t cb, void *user_ctx) {
    rm690b0_register_error_callback(cb, user_ctx);
}

void hal_mgr_register_usb_callback(hal_mgr_usb_event_cb_t cb, void *user_ctx) {
	s_usb_cb = cb;
	s_usb_ctx = user_ctx;
}
void hal_mgr_register_charge_callback(hal_mgr_charge_event_cb_t cb, void *user_ctx) {
	s_charge_cb = cb;
	s_charge_ctx = user_ctx;
}
void hal_mgr_register_battery_callback(hal_mgr_battery_event_cb_t cb, void *user_ctx) {
	s_batt_cb = cb;
	s_batt_ctx = user_ctx;
}

static hal_mgr_rotation_cb_t s_rot_cb = NULL;
static void *s_rot_ctx = NULL;

void hal_mgr_register_rotation_callback(hal_mgr_rotation_cb_t cb, void *user_ctx) {
    s_rot_cb = cb;
    s_rot_ctx = user_ctx;
}

void hal_mgr_set_rotation(rm690b0_rotation_t rot) {
    // 1. Clear the physical display (black) to remove artifacts from old orientation
    rm690b0_clear_full_display(0x0000);
    
    // 2. Update hardware rotation
	rm690b0_set_rotation(rot);
	cst226se_set_rotation((cst226se_rotation_t)rot);

    // 3. Notify upper layers (LVGL)
    if (s_rot_cb) s_rot_cb(rot, s_rot_ctx);
}

rm690b0_rotation_t hal_mgr_get_rotation(void) {
	return rm690b0_get_rotation();
}

static bool s_led_manual_lock = false;
static hal_led_status_t s_last_led_status = (hal_led_status_t)-1;

void hal_mgr_lock_led(bool locked) {
    s_led_manual_lock = locked;
    // When unlocking, reset last status to force re-evaluation
    if (!locked) {
        s_last_led_status = (hal_led_status_t)-1;
    }
}

void hal_mgr_set_led_status(hal_led_status_t status) {
    sy6970_led_mode_t mode = SY6970_LED_OFF;
    uint32_t pattern = 0;

    ESP_LOGI(TAG, "hal_mgr_set_led_status called: status=%d", status);

    switch (status) {
        case HAL_LED_STATUS_OFF:
            mode = SY6970_LED_OFF;
            break;
        case HAL_LED_STATUS_CHARGING:
            mode = SY6970_LED_ON; // Continuous hardware blink
            break;
        case HAL_LED_STATUS_FULL:
            mode = SY6970_LED_OFF; // Charge done - LED off
            break;
        case HAL_LED_STATUS_FAULT_GENERIC:
            mode = SY6970_LED_ON; // Continuous blink for unknown fault
            break;
        case HAL_LED_STATUS_FAULT_WDT:
            mode = SY6970_LED_BLINK;
            pattern = (2 << 16) | 4; // 2 second on (2 blinks), 4 second pause
            break;
        case HAL_LED_STATUS_FAULT_OVP:
            mode = SY6970_LED_BLINK;
            pattern = (3 << 16) | 4; // 3 seconds on (3 blinks), 4 second pause
            break;
        case HAL_LED_STATUS_FAULT_TEMP:
            // SOS pattern: use 6 blinks to indicate temperature issue
            mode = SY6970_LED_BLINK;
            pattern = (6 << 16) | 4; // 6 seconds on (6 blinks), 4 second pause  
            break;
    }
    
    ESP_LOGI(TAG, "Calling sy6970_led_set_mode: mode=%d, pattern=0x%08lX", mode, pattern);
    esp_err_t ret = sy6970_led_set_mode(mode, pattern);
    ESP_LOGI(TAG, "sy6970_led_set_mode returned: %s", esp_err_to_name(ret));
}

// --- Polling task for status changes ---
static void hal_mgr_status_task(void *arg) {
    // Enable STAT LED on HAL start (Charging = Low/On, Done/Fault = High/Off if pulled up or blink if managed)
    // We let the hardware handle it by default (SY6970_LED_ON enables STAT pin output)
    // sy6970_led_set_mode(SY6970_LED_ON, 0); // Logic moved to loop below

	bool last_usb = false, last_charge = false, last_batt = false;
	static int charge_stable_count = 0;
	static bool last_reported_charge = false;
	const int debounce_ticks = 5; // e.g., 5*200ms = 1s
    int log_counter = 0;

	while (1) {
		bool usb = sy6970_is_vbus_connected();
		bool batt = sy6970_is_power_good();

        // --- LED Control Logic (Software Driven) ---
        hal_led_status_t target_status = HAL_LED_STATUS_OFF;

        uint8_t faults = sy6970_get_faults();
        sy6970_charge_status_t chg_status = sy6970_get_charge_status();

        if (!s_led_manual_lock) {
            if (faults != 0) {
                // sophisticated LED codes
                if (faults & SY6970_FAULT_WDT) {
                    target_status = HAL_LED_STATUS_FAULT_WDT;
                } else if (faults & SY6970_FAULT_BAT_OVP) {
                    target_status = HAL_LED_STATUS_FAULT_OVP;
                } else if (faults & SY6970_FAULT_NTC_MASK) {
                    target_status = HAL_LED_STATUS_FAULT_TEMP;
                } else {
                    target_status = HAL_LED_STATUS_FAULT_GENERIC;
                }
            } else if (chg_status == SY6970_CHG_FAST_CHARGE || chg_status == SY6970_CHG_PRE_CHARGE) {
                target_status = HAL_LED_STATUS_CHARGING;
            } else if (chg_status == SY6970_CHG_TERM_DONE) {
                target_status = HAL_LED_STATUS_FULL;
            } else {
                target_status = HAL_LED_STATUS_OFF;
            }

            if (target_status != s_last_led_status) {
                ESP_LOGI(TAG, "LED status change: %d -> %d (chg_status=%d, faults=0x%02X: %s)", 
                         s_last_led_status, target_status, chg_status, faults, 
                         sy6970_decode_faults(faults));
                hal_mgr_set_led_status(target_status);
                s_last_led_status = target_status;
            }
        }
        // -------------------------

        // Periodic logging (approx every 1s)
        if (++log_counter >= 4) {
            uint16_t vbat = sy6970_get_battery_voltage();
            bool chg = (sy6970_get_charge_status() == SY6970_CHG_FAST_CHARGE || 
                        sy6970_get_charge_status() == SY6970_CHG_PRE_CHARGE);
            
            // Show detailed fault info if faults present
            if (faults != 0) {
                ESP_LOGI(TAG, "Bat: %dmV %s%s Faults:0x%02X (%s)", vbat, usb ? "USB" : "", 
                         chg ? " (Chg)" : "", faults, sy6970_decode_faults(faults));
            } else {
                ESP_LOGI(TAG, "Bat: %dmV %s%s Faults:0x%02X", vbat, usb ? "USB" : "", 
                         chg ? " (Chg)" : "", faults);
            }
            log_counter = 0;
        }

		if (s_usb_cb && usb != last_usb) s_usb_cb(usb, s_usb_ctx);

		bool charging = (sy6970_get_charge_status() != SY6970_CHG_NOT_CHARGING);
		if (charging != last_charge) {
			charge_stable_count++;
			if (charge_stable_count >= debounce_ticks) {
				if (s_charge_cb && charging != last_reported_charge)
					s_charge_cb(charging, s_charge_ctx);
				last_reported_charge = charging;
				last_charge = charging;
				charge_stable_count = 0;
			}
		} else {
			charge_stable_count = 0;
		}
		if (s_batt_cb && batt != last_batt) s_batt_cb(batt, s_batt_ctx);
		last_usb = usb;
		last_batt = batt;
		vTaskDelay(pdMS_TO_TICKS(250));
	}
}

static void hal_mgr_btn_double_click_cb(void *arg, void *usr_data) {
    ESP_LOGI(TAG, "Button Double Click - Rotating Screen");
    rm690b0_rotation_t current = hal_mgr_get_rotation();
    rm690b0_rotation_t next = (rm690b0_rotation_t)((current + 1) % 4);
    hal_mgr_set_rotation(next);
}

esp_err_t hal_mgr_init(void) {
	esp_err_t ret;

	// Power/PMIC
	ret = sy6970_init();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize PMIC");
		return ret;
	}

    // Button (GPIO 0 - Boot)
    button_config_t btn_cfg = {
        .long_press_time = 1500,
        .short_press_time = 750, // User requested 750ms
    };
    button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = 0,
        .active_level = 0,
    };
    button_handle_t btn_handle = NULL;
    esp_err_t btn_ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn_handle);
    if (btn_ret == ESP_OK && btn_handle) {
        iot_button_register_cb(btn_handle, BUTTON_DOUBLE_CLICK, NULL, hal_mgr_btn_double_click_cb, NULL);
    } else {
        ESP_LOGE(TAG, "Failed to create button: %s", esp_err_to_name(btn_ret));
    }

	// Display
	ret = rm690b0_init();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize display");
		return ret;
	}

	// Install GPIO ISR service once for all components
	esp_err_t isr_ret = gpio_install_isr_service(0);
	if (isr_ret != ESP_OK && isr_ret != ESP_ERR_INVALID_STATE) {
		ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(isr_ret));
	}

	// Touch
	ret = cst226se_init();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize touch driver");
		return ret;
	}
	cst226se_register_callback(hal_mgr_touch_event_handler, NULL);

	// Set default orientation to Rotation 0 (Landscape 600x450)
	// This must be done AFTER touch init because touch init resets rotation
	hal_mgr_set_rotation(RM690B0_ROTATION_0); 
	
	rm690b0_set_brightness(128);
	rm690b0_clear_full_display(0x0000); // Black
	vTaskDelay(pdMS_TO_TICKS(100));
	rm690b0_draw_test_pattern();       // Show rainbow bars for edge testing
	vTaskDelay(pdMS_TO_TICKS(1000));   // Display rainbow for 1 second
	rm690b0_clear_full_display(0x0000); // Clear to black
	rm690b0_enable_te(true);
	rm690b0_register_vsync_callback(hal_mgr_vsync_handler, NULL);

	// Touch event task
	if (xTaskCreate(hal_mgr_touch_task, "hal_mgr_touch", 4096, NULL, 5, NULL) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create touch task");
		return ESP_FAIL;
	}

	// PMIC status polling
	if (xTaskCreate(hal_mgr_status_task, "hal_mgr_status", 4096, NULL, 5, NULL) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create status task");
		return ESP_FAIL;
	}

    // SD Card (Optional - do not fail HAL init if missing)
    if (sd_card_init() == ESP_OK) {
        ESP_LOGI(TAG, "SD Card initialized");
    } else {
        ESP_LOGW(TAG, "SD Card init failed (not inserted?)");
    }

	return ESP_OK;
}

esp_err_t hal_mgr_sd_init(void) {
    return sd_card_init();
}

bool hal_mgr_sd_is_mounted(void) {
    return sd_card_is_mounted();
}

