#include "ui_private.h"
#include "esp_log.h"
#include "sy6970.h"
#include <stdio.h> // for snprintf
#include "nvs_flash.h"
#include "nvs.h"
#include "ota_mgr.h" // Include OTA manager
#include "lvgl_mgr.h" // For lvgl_mgr_lock/unlock

LV_IMG_DECLARE(swipeL34);
LV_IMG_DECLARE(swipeR34);

static lv_obj_t * cont_chg_settings = NULL; // Container for charging parameters
static lv_obj_t * roller_boost_volt = NULL; // Global handle for boost voltage roller
static lv_obj_t * lbl_ota_status = NULL;
static lv_obj_t * bar_ota_progress = NULL;
static lv_obj_t * ota_modal = NULL;
static lv_obj_t * btn_ota_close = NULL;

static void ota_close_event_cb(lv_event_t * e) {
    if (ota_modal) {
        lv_obj_add_flag(ota_modal, LV_OBJ_FLAG_HIDDEN);
    }
}

// --- OTA Callbacks ---
static void ota_progress_cb(int percent, void *user_ctx) {
    if (bar_ota_progress) {
        // Need to run LVGL update on GUI thread?
        // Luckily we are mostly in GUI thread context or can use lock if needed
        // but FreeRTOS task usually runs separate.
        // We should protect this.
        lvgl_mgr_lock();
        lv_bar_set_value(bar_ota_progress, percent, LV_ANIM_ON);
        char buf[32];
        snprintf(buf, sizeof(buf), "Downloading... %d%%", percent);
        if (lbl_ota_status) lv_label_set_text(lbl_ota_status, buf);
        lvgl_mgr_unlock();
    }
}

static void ota_complete_cb(esp_err_t err, void *user_ctx) {
    lvgl_mgr_lock();
    if (err == ESP_OK) {
        if (lbl_ota_status) lv_label_set_text(lbl_ota_status, "Success! Rebooting...");
    } else {
        if (err == ESP_ERR_OTA_UP_TO_DATE) {
            if (lbl_ota_status) lv_label_set_text(lbl_ota_status, "System is Up To Date");
        } else {
            if (lbl_ota_status) lv_label_set_text_fmt(lbl_ota_status, "Failed: %s", esp_err_to_name(err));
        }
        
        // Show close button
        if (btn_ota_close) {
            lv_obj_remove_flag(btn_ota_close, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lvgl_mgr_unlock();
}

static void btn_start_update_cb(lv_event_t * e) {
    const char * url = "https://github.com/coyotegd/t4-s3_hal_bsp-lvgl/releases/latest/download/t4-s3_hal_bsp-lvgl.bin"; // GitHub Release URL
    
    if (ota_mgr_is_busy()) return;
    
    // Show Modal
    if (!ota_modal) {
        ota_modal = lv_obj_create(lv_layer_top());
        lv_obj_set_size(ota_modal, LV_PCT(85), LV_PCT(60));
        lv_obj_center(ota_modal);
        lv_obj_set_flex_flow(ota_modal, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(ota_modal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_color(ota_modal, lv_color_hex(0x101010), 0);
        lv_obj_set_style_border_color(ota_modal, lv_color_hex(0xCD00CD), 0);
        lv_obj_set_style_border_width(ota_modal, 2, 0);
        lv_obj_set_style_shadow_width(ota_modal, 20, 0);
        lv_obj_set_style_shadow_color(ota_modal, lv_color_hex(0xCD00CD), 0);
        
        lbl_ota_status = lv_label_create(ota_modal);
        lv_label_set_text(lbl_ota_status, "Starting Update...");
        lv_obj_set_style_text_color(lbl_ota_status, lv_color_white(), 0);
        lv_obj_set_style_text_align(lbl_ota_status, LV_TEXT_ALIGN_CENTER, 0);
        
        bar_ota_progress = lv_bar_create(ota_modal);
        lv_obj_set_width(bar_ota_progress, LV_PCT(90));
        lv_obj_set_height(bar_ota_progress, 20);
        lv_bar_set_range(bar_ota_progress, 0, 100);
        lv_obj_set_style_bg_color(bar_ota_progress, lv_color_hex(0x303030), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar_ota_progress, lv_color_hex(0xCD00CD), LV_PART_INDICATOR);

        btn_ota_close = lv_btn_create(ota_modal);
        lv_obj_set_width(btn_ota_close, 120);
        lv_obj_set_height(btn_ota_close, 50);
        lv_obj_add_event_cb(btn_ota_close, ota_close_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_color(btn_ota_close, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(btn_ota_close, lv_color_hex(0xFF0000), 0); // Red for Close
        lv_obj_set_style_border_width(btn_ota_close, 2, 0);
        lv_obj_set_style_shadow_width(btn_ota_close, 10, 0);
        lv_obj_set_style_shadow_color(btn_ota_close, lv_color_hex(0xFF0000), 0);

        lv_obj_t * lbl_close = lv_label_create(btn_ota_close);
        lv_label_set_text(lbl_close, "Close");
        lv_obj_center(lbl_close);
        lv_obj_set_style_text_color(lbl_close, lv_color_white(), 0);
        
        lv_obj_add_flag(btn_ota_close, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_obj_remove_flag(ota_modal, LV_OBJ_FLAG_HIDDEN);
    if(btn_ota_close) lv_obj_add_flag(btn_ota_close, LV_OBJ_FLAG_HIDDEN); // Ensure hidden
    lv_bar_set_value(bar_ota_progress, 0, LV_ANIM_OFF);
    lv_label_set_text(lbl_ota_status, "Connecting...");
    
    esp_err_t ret = ota_mgr_start_update(url, ota_progress_cb, ota_complete_cb, NULL);
    if (ret != ESP_OK) {
        lv_label_set_text(lbl_ota_status, "Failed to start task");
    }
}

// --- NVS Helpers ---
static void save_nvs_value(const char* key, uint16_t val) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_u16(my_handle, key, val);
        nvs_commit(my_handle);
        nvs_close(my_handle);
    } else {
        ESP_LOGE("ui_system", "Error (%s) opening NVS handle!", esp_err_to_name(err));
    }
}

// Restore settings from NVS to Chip on boot/init
void ui_pmic_restore_settings(void) {
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Load and Apply (defaults are what the chip has, but if NVS exists, we overwrite)
    // Note: If NVS is empty, we DON'T touch the chip (preserving hardcoded defaults from main or sy6970_init)
    // UNLESS we want to enforce NVS defaults? 
    // The user says "default and my own functional settings". 
    // If we only apply if NVS exists, we respect main's settings until user changes UI.
    
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        uint16_t val;
        if(nvs_get_u16(my_handle, "in_curr", &val) == ESP_OK) sy6970_set_input_current_limit(val);
        if(nvs_get_u16(my_handle, "in_volt", &val) == ESP_OK) sy6970_set_input_voltage_limit(val);
        if(nvs_get_u16(my_handle, "chg_curr", &val) == ESP_OK) sy6970_set_charge_current(val);
        if(nvs_get_u16(my_handle, "pre_curr", &val) == ESP_OK) sy6970_set_precharge_current(val);
        if(nvs_get_u16(my_handle, "term_curr", &val) == ESP_OK) sy6970_set_termination_current(val);
        if(nvs_get_u16(my_handle, "chg_volt", &val) == ESP_OK) sy6970_set_charge_voltage(val);
        if(nvs_get_u16(my_handle, "sys_min", &val) == ESP_OK) sy6970_set_min_system_voltage(val);
        if(nvs_get_u16(my_handle, "otg_en", &val) == ESP_OK) sy6970_enable_otg(val);
        if(nvs_get_u16(my_handle, "boost_volt", &val) == ESP_OK) sy6970_set_boost_voltage(val);
        if(nvs_get_u16(my_handle, "hiz_en", &val) == ESP_OK) sy6970_enable_hiz_mode(val);
        
        nvs_close(my_handle);
        ESP_LOGI("ui_system", "Restored PMIC settings from NVS");
    }
}

static void pmic_swipe_event_cb(lv_event_t * e) {
    if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) {
        show_home_view(NULL);
    }
}

static void settings_swipe_event_cb(lv_event_t * e) {
    if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        show_home_view(NULL);
    }
}

// These are now only called from swipe gestures
void show_pmic_view(lv_event_t * e) {
    (void)e;
    // Swipe gestures go through show_home_view which uses the timer
}

void show_settings_view(lv_event_t * e) {
    (void)e;
}

void show_sys_info_view(lv_event_t * e) {
    (void)e;
}

static void disable_led_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    if (lv_obj_has_state(sw, LV_STATE_CHECKED)) {
        sy6970_enable_stat_led(false);
        ESP_LOGI("ui_system", "STAT LED Disabled");
    } else {
        sy6970_enable_stat_led(true);
        ESP_LOGI("ui_system", "STAT LED Enabled");
    }
}

static void enable_adc_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bool enable = lv_obj_has_state(sw, LV_STATE_CHECKED);
    sy6970_enable_adc(enable, enable); 
    if(enable) {
         ESP_LOGI("ui_system", "ADC Monitoring Enabled");
    } else {
         ESP_LOGI("ui_system", "ADC Monitoring Disabled");
    }
}

static void enable_charging_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bool enable = lv_obj_has_state(sw, LV_STATE_CHECKED);
    sy6970_enable_charging(enable);
    if(enable) {
         ESP_LOGI("ui_system", "Charging Enabled");
         if (cont_chg_settings) lv_obj_remove_state(cont_chg_settings, LV_STATE_DISABLED);
    } else {
         ESP_LOGI("ui_system", "Charging Disabled");
         if (cont_chg_settings) lv_obj_add_state(cont_chg_settings, LV_STATE_DISABLED);
    }
}

// Helper to create roller rows
static lv_obj_t* create_roller_row(lv_obj_t * parent, const char * label, const char * options, lv_event_cb_t cb, uint16_t current_val) {
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 5, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t * lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    
    lv_obj_t * roller = lv_roller_create(row);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, 3);
    lv_obj_set_width(roller, 150);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_22, 0);
    lv_obj_set_style_bg_color(roller, lv_color_black(), 0);
    lv_obj_set_style_text_color(roller, lv_color_white(), 0);

    // Select current value
    char val_str[16];
    snprintf(val_str, sizeof(val_str), "%d", current_val);
    const char * start = options;
    const char * end;
    int idx = 0;
    while((end = strchr(start, '\n')) != NULL) {
        int len = end - start;
        if(strncmp(start, val_str, len) == 0 && val_str[len] == '\0') {
             lv_roller_set_selected(roller, idx, LV_ANIM_OFF);
             break;
        }
        start = end + 1;
        idx++;
    }
    if(strcmp(start, val_str) == 0) {
         lv_roller_set_selected(roller, idx, LV_ANIM_OFF);
    }
    
    lv_obj_add_event_cb(roller, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return roller;
}

// Callbacks for Charge Settings
static void input_curr_cb(lv_event_t * e) {
    lv_obj_t * roller = lv_event_get_target(e);
    char buf[16];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    uint16_t val = (uint16_t)atoi(buf);
    sy6970_set_input_current_limit(val);
    save_nvs_value("in_curr", val);
    ESP_LOGI("ui_system", "Input Curr Limit set: %d mA", val);
}

static void input_volt_cb(lv_event_t * e) {
    lv_obj_t * roller = lv_event_get_target(e);
    char buf[16];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    uint16_t val = (uint16_t)atoi(buf);
    sy6970_set_input_voltage_limit(val);
    save_nvs_value("in_volt", val);
    ESP_LOGI("ui_system", "Input Volt Limit set: %d mV", val);
}

static void chg_curr_cb(lv_event_t * e) {
    lv_obj_t * roller = lv_event_get_target(e);
    char buf[16];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    uint16_t val = (uint16_t)atoi(buf);
    sy6970_set_charge_current(val);
    save_nvs_value("chg_curr", val);
    ESP_LOGI("ui_system", "Charge Curr set: %d mA", val);
}

static void pre_curr_cb(lv_event_t * e) {
    lv_obj_t * roller = lv_event_get_target(e);
    char buf[16];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    uint16_t val = (uint16_t)atoi(buf);
    sy6970_set_precharge_current(val);
    save_nvs_value("pre_curr", val);
    ESP_LOGI("ui_system", "Pre-Charge Curr set: %d mA", val);
}

static void term_curr_cb(lv_event_t * e) {
    lv_obj_t * roller = lv_event_get_target(e);
    char buf[16];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    uint16_t val = (uint16_t)atoi(buf);
    sy6970_set_termination_current(val);
    save_nvs_value("term_curr", val);
    ESP_LOGI("ui_system", "Term Curr set: %d mA", val);
}

static void chg_volt_cb(lv_event_t * e) {
    lv_obj_t * roller = lv_event_get_target(e);
    char buf[16];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    uint16_t val = (uint16_t)atoi(buf);
    sy6970_set_charge_voltage(val);
    save_nvs_value("chg_volt", val);
    ESP_LOGI("ui_system", "Charge Volt set: %d mV", val);
}

static void sys_min_cb(lv_event_t * e) {
    lv_obj_t * roller = lv_event_get_target(e);
    char buf[16];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    uint16_t val = (uint16_t)atoi(buf);
    sy6970_set_min_system_voltage(val);
    save_nvs_value("sys_min", val);
    ESP_LOGI("ui_system", "Sys Min Volt set: %d mV", val);
}

static void otg_switch_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bool enable = lv_obj_has_state(sw, LV_STATE_CHECKED);
    sy6970_enable_otg(enable);
    save_nvs_value("otg_en", enable ? 1 : 0);
    ESP_LOGI("ui_system", "OTG Enabled: %d", enable);
    
    if(roller_boost_volt) {
        if(enable) {
            lv_obj_remove_state(roller_boost_volt, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(roller_boost_volt, LV_STATE_DISABLED);
        }
    }
}

static void boost_volt_cb(lv_event_t * e) {
    lv_obj_t * roller = lv_event_get_target(e);
    char buf[16];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    uint16_t val = (uint16_t)atoi(buf);
    sy6970_set_boost_voltage(val);
    save_nvs_value("boost_volt", val);
    ESP_LOGI("ui_system", "Boost Voltage set: %d mV", val);
}

static void hiz_switch_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bool enable = lv_obj_has_state(sw, LV_STATE_CHECKED);
    sy6970_enable_hiz_mode(enable);
    save_nvs_value("hiz_en", enable ? 1 : 0);
    ESP_LOGI("ui_system", "HIZ Mode: %d", enable);
}

static void shutdown_switch_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bool enable = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    if (enable) {
        ESP_LOGW("ui_system", "Requesting Hard Shutdown (BATFET Disable)");
        sy6970_disable_batfet(true);
    }
}

// Forward declaration
void ui_pmic_create(lv_obj_t * parent);
void ui_settings_create(lv_obj_t * parent); // Forward declaration

static lv_obj_t * pmic_parent_obj = NULL; // Store parent for recreation
static lv_obj_t * settings_parent_obj = NULL; // Store parent for settings recreation

// Timer callback to scroll efficiently after layout is ready
static void scroll_timer_cb(lv_timer_t * t) {
    if(cont_chg_settings) {
         // Find the scrollable parent
        lv_obj_t * scrollable_parent = cont_chg_settings;
        while(scrollable_parent) {
            if(lv_obj_has_flag(scrollable_parent, LV_OBJ_FLAG_SCROLLABLE)) {
                break;
            }
            scrollable_parent = lv_obj_get_parent(scrollable_parent);
        }

        if(scrollable_parent) {
            // Scroll to the absolute bottom (Y=10000 is safe)
            lv_obj_scroll_to_y(scrollable_parent, 10000, LV_ANIM_ON);
        }
    }
}

static void defaults_btn_cb(lv_event_t * e) {
    // 1. Input Current Limit: 3000mA (Max available from weak sources via VINDPM throttling)
    sy6970_set_input_current_limit(3000);
    save_nvs_value("in_curr", 3000);

    // 2. Input Voltage Limit: 4400mV (Safer for bad cables than 4.5V)
    sy6970_set_input_voltage_limit(4400);
    save_nvs_value("in_volt", 4400);

    // 3. Fast Charge Current: 1024mA (1A - Safer default for smaller batteries)
    sy6970_set_charge_current(1024);
    save_nvs_value("chg_curr", 1024);

    // 4. Pre-Charge Current: 128mA (Balanced default for deep discharge)
    sy6970_set_precharge_current(128);
    save_nvs_value("pre_curr", 128);

    // 5. Termination Current: 128mA (10-15% of Fast Charge)
    sy6970_set_termination_current(128);
    save_nvs_value("term_curr", 128);

    // 6. Max Charge Voltage: 4208mV (4.2V)
    sy6970_set_charge_voltage(4208);
    save_nvs_value("chg_volt", 4208);

    // 7. Min System Voltage: 3500mV
    sy6970_set_min_system_voltage(3500);
    save_nvs_value("sys_min", 3500);

    // 8. OTG: Disabled
    sy6970_enable_otg(false);
    save_nvs_value("otg_en", 0);

    // 9. Boost Voltage: 5126mV (~5.1V)
    sy6970_set_boost_voltage(5126);
    save_nvs_value("boost_volt", 5126);

    // 10. HIZ Mode: Disabled
    sy6970_enable_hiz_mode(false);
    save_nvs_value("hiz_en", 0);

    // 11. Ensure Shutdown is Cancelled (BATFET Enabled)
    sy6970_disable_batfet(false);
    
    // Hard refresh of the UI
    // We reload the Settings page to reflect the new default values in the widgets
    if(settings_cont && settings_parent_obj) {
        lv_obj_del(settings_cont);
        settings_cont = NULL;
        cont_chg_settings = NULL; 
        roller_boost_volt = NULL;
        
        ui_settings_create(settings_parent_obj);
    
        // Trigger a scroll update slightly later to ensure layout is calculated
        lv_timer_t * t = lv_timer_create(scroll_timer_cb, 50, NULL);
        lv_timer_set_repeat_count(t, 1);
    }
    
    ESP_LOGI("ui_system", "SY6970 Defaults Applied & UI Settings Page Reloaded");
}

void ui_pmic_create(lv_obj_t * parent) {
    pmic_parent_obj = parent; // Save for reload
    // restore_pmic_settings() is now called in lv_ui_init() globally
    // but no harm checking again or just relying on global init.
    // Let's remove it here to avoid redundant I2C/NVS traffic on swipe.
    
    // --- PMIC Container ---
    pmic_cont = lv_obj_create(parent);
    lv_obj_set_size(pmic_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(pmic_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(pmic_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pmic_cont, 0, 0);
    lv_obj_set_style_pad_all(pmic_cont, 10, 0);
    lv_obj_set_flex_flow(pmic_cont, LV_FLEX_FLOW_ROW);
    lv_obj_add_flag(pmic_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pmic_cont, pmic_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    
    lv_obj_t * img_swipe = lv_image_create(pmic_cont);
    lv_image_set_src(img_swipe, &swipeL34);
    lv_obj_add_flag(img_swipe, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(img_swipe, LV_ALIGN_TOP_RIGHT, 0, 15);

    lv_obj_t * lbl_pmic_title = lv_label_create(pmic_cont);
    lv_label_set_text(lbl_pmic_title, "PM Status");
    lv_obj_set_style_text_color(lbl_pmic_title, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(lbl_pmic_title, &lv_font_montserrat_28, 0);
    lv_obj_add_flag(lbl_pmic_title, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(lbl_pmic_title, LV_ALIGN_TOP_MID, 0, 15);

    // Inner Panel
    lv_obj_t * pmic_panel = lv_obj_create(pmic_cont);
    lv_obj_set_size(pmic_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(pmic_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(pmic_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pmic_panel, 1, 0);
    lv_obj_set_style_border_color(pmic_panel, lv_color_hex(0x404040), 0);
    lv_obj_set_flex_flow(pmic_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(pmic_panel, 10, 0);
    lv_obj_set_style_margin_top(pmic_panel, 70, 0);
    lv_obj_add_event_cb(pmic_panel, pmic_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(pmic_panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    cont_pmic_details = lv_obj_create(pmic_panel);
    lv_obj_set_width(cont_pmic_details, LV_PCT(100));
    lv_obj_set_height(cont_pmic_details, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_pmic_details, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_pmic_details, 0, 0);
    lv_obj_set_flex_flow(cont_pmic_details, LV_FLEX_FLOW_COLUMN);

    // PMIC Labels
    lbl_sys_volts = lv_label_create(cont_pmic_details);
    lv_label_set_text(lbl_sys_volts, "System Volts:\n-- V");
    lv_obj_set_style_text_color(lbl_sys_volts, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_sys_volts, &lv_font_montserrat_22, 0);

    lbl_batt = lv_label_create(cont_pmic_details);
    lv_label_set_text(lbl_batt, "Battery Volts:\n-- V");
    lv_obj_set_style_text_color(lbl_batt, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_batt, &lv_font_montserrat_22, 0);

    lbl_chg_stat = lv_label_create(cont_pmic_details);
    lv_label_set_text(lbl_chg_stat, "Charge Status:\n--");
    lv_obj_set_style_text_color(lbl_chg_stat, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_chg_stat, &lv_font_montserrat_22, 0);

    lbl_chg_curr = lv_label_create(cont_pmic_details);
    lv_label_set_text(lbl_chg_curr, "Charging Current:\n-- mA");
    lv_obj_set_style_text_color(lbl_chg_curr, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_chg_curr, &lv_font_montserrat_22, 0);

    lbl_usb = lv_label_create(cont_pmic_details);
    lv_label_set_text(lbl_usb, "USB:\n--");
    lv_obj_set_style_text_color(lbl_usb, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_usb, &lv_font_montserrat_22, 0);

    lbl_usb_volts = lv_label_create(cont_pmic_details);
    lv_label_set_text(lbl_usb_volts, "USB Volts:\n-- V");
    lv_obj_set_style_text_color(lbl_usb_volts, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_usb_volts, &lv_font_montserrat_22, 0);

    lbl_usb_pg = lv_label_create(cont_pmic_details);
    lv_label_set_text(lbl_usb_pg, "USB Power:\n--");
    lv_obj_set_style_text_color(lbl_usb_pg, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_usb_pg, &lv_font_montserrat_22, 0);

    lbl_ntc = lv_label_create(cont_pmic_details);
    lv_label_set_text(lbl_ntc, "Temperature:\n-- %");
    lv_obj_set_style_text_color(lbl_ntc, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_ntc, &lv_font_montserrat_22, 0);

    // Fault Row (Label + Switch)
    lv_obj_t * fault_row = lv_obj_create(cont_pmic_details);
    lv_obj_set_width(fault_row, LV_PCT(100));
    lv_obj_set_height(fault_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(fault_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fault_row, 0, 0);
    lv_obj_set_style_pad_all(fault_row, 0, 0);
    lv_obj_set_flex_flow(fault_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fault_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lbl_fault = lv_label_create(fault_row);
    lv_label_set_text(lbl_fault, "Fault:\nNone");
    lv_obj_set_style_text_color(lbl_fault, lv_color_hex(0x00FF00), 0); // Green for no faults
    lv_obj_set_style_text_font(lbl_fault, &lv_font_montserrat_22, 0);
    lv_obj_set_flex_grow(lbl_fault, 1); // Let label take available space

    // Label for Disable LED Switch
    lv_obj_t * lbl_disable_led = lv_label_create(fault_row);
    lv_label_set_text(lbl_disable_led, "Disable Fault LED");
    lv_obj_set_style_text_color(lbl_disable_led, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_disable_led, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_right(lbl_disable_led, 5, 0);

    // Disable LED Switch
    sw_disable_led = lv_switch_create(fault_row);
    // Make it the same size as "Driver Test Pattern" switch on Display Information page (80x40)
    lv_obj_set_size(sw_disable_led, 80, 40); 
    lv_obj_add_event_cb(sw_disable_led, disable_led_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_state(sw_disable_led, LV_STATE_DISABLED); // Disabled by default until fault
    
    // Switch Label (Optional, maybe "Disable LED" text next to it? User said "Disable LED switch")
    // "On PM Status page, right justified ,on the "Fault: . . ." multiline, include a "Disable LED" switch"
    // It's implicit the switch itself represents "Disable LED". 
    // Or maybe we should add a small label above/next to it? 
    // Let's stick to the switch for now, maybe add a label container if space permits.
    // Actually, "Disable LED" text implies a label. But standard switch is just toggle.
    // Let's add a small label "Disable LED" above the switch or inside the row?
    // User just said "include a 'Disable LED' switch".
}

void ui_settings_create(lv_obj_t * parent) {
    settings_parent_obj = parent; // Capture parent for reload
    // --- Settings Container ---
    settings_cont = lv_obj_create(parent);
    lv_obj_set_size(settings_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(settings_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(settings_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(settings_cont, 0, 0);
    lv_obj_set_style_pad_all(settings_cont, 10, 0);
    lv_obj_set_flex_flow(settings_cont, LV_FLEX_FLOW_ROW);
    lv_obj_add_flag(settings_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(settings_cont, settings_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    
    lv_obj_t * img_swipe = lv_image_create(settings_cont);
    lv_image_set_src(img_swipe, &swipeR34);
    lv_obj_add_flag(img_swipe, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(img_swipe, LV_ALIGN_TOP_LEFT, 0, 15);

    lv_obj_t * lbl_settings_title = lv_label_create(settings_cont);
    lv_label_set_text(lbl_settings_title, "PM Settings");
    lv_obj_set_style_text_color(lbl_settings_title, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(lbl_settings_title, &lv_font_montserrat_28, 0);
    lv_obj_add_flag(lbl_settings_title, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(lbl_settings_title, LV_ALIGN_TOP_MID, 0, 15);

    // Inner Panel
    lv_obj_t * settings_panel = lv_obj_create(settings_cont);
    lv_obj_set_size(settings_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(settings_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(settings_panel, 1, 0);
    lv_obj_set_style_border_color(settings_panel, lv_color_hex(0x404040), 0);
    lv_obj_set_flex_flow(settings_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(settings_panel, 10, 0);
    lv_obj_set_style_margin_top(settings_panel, 70, 0);
    lv_obj_add_event_cb(settings_panel, settings_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(settings_panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    cont_settings_list = lv_obj_create(settings_panel);
    lv_obj_set_width(cont_settings_list, LV_PCT(100));
    lv_obj_set_height(cont_settings_list, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_settings_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_settings_list, 0, 0);
    lv_obj_set_flex_flow(cont_settings_list, LV_FLEX_FLOW_COLUMN);

    // ADC Monitoring Row
    lv_obj_t * adc_row = lv_obj_create(cont_settings_list);
    lv_obj_set_width(adc_row, LV_PCT(100));
    lv_obj_set_height(adc_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(adc_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(adc_row, 0, 0);
    lv_obj_set_style_pad_all(adc_row, 5, 0);
    lv_obj_set_flex_flow(adc_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(adc_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl_adc = lv_label_create(adc_row);
    lv_label_set_text(lbl_adc, "Enable ADC Monitoring");
    lv_obj_set_style_text_color(lbl_adc, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_adc, &lv_font_montserrat_20, 0);

    lv_obj_t * sw_adc = lv_switch_create(adc_row);
    lv_obj_set_size(sw_adc, 80, 40);
    lv_obj_add_state(sw_adc, LV_STATE_CHECKED); // Default ON
    lv_obj_add_event_cb(sw_adc, enable_adc_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Charging Control Row
    lv_obj_t * chg_row = lv_obj_create(cont_settings_list);
    lv_obj_set_width(chg_row, LV_PCT(100));
    lv_obj_set_height(chg_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(chg_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chg_row, 0, 0);
    lv_obj_set_style_pad_all(chg_row, 5, 0);
    lv_obj_set_flex_flow(chg_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chg_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl_chg = lv_label_create(chg_row);
    lv_label_set_text(lbl_chg, "Enable Charging");
    lv_obj_set_style_text_color(lbl_chg, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_chg, &lv_font_montserrat_20, 0);

    lv_obj_t * sw_chg = lv_switch_create(chg_row);
    lv_obj_set_size(sw_chg, 80, 40);
    lv_obj_add_state(sw_chg, LV_STATE_CHECKED); // Default ON
    lv_obj_add_event_cb(sw_chg, enable_charging_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Container for Charging Settings (Disabled if Charging is Off)
    cont_chg_settings = lv_obj_create(cont_settings_list);
    lv_obj_set_width(cont_chg_settings, LV_PCT(100));
    lv_obj_set_height(cont_chg_settings, LV_SIZE_CONTENT); 
    lv_obj_set_style_bg_opa(cont_chg_settings, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_chg_settings, 0, 0);
    lv_obj_set_style_pad_all(cont_chg_settings, 0, 0); // No padding to align with list
    lv_obj_set_flex_flow(cont_chg_settings, LV_FLEX_FLOW_COLUMN);

    // --- Generate Options Strings ---
    // Note: In a real constrained environment we might avoid large stack buffers, 
    // but here we allocate them temporarily to build the options.

    // 1. Input Current Limit: 100-3250 step 50
    char * opt_in_curr = (char*)malloc(1024);
    if(opt_in_curr) {
        opt_in_curr[0] = '\0';
        for(int i=100; i<=3250; i+=50) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "%d\n", i);
            strcat(opt_in_curr, tmp);
        }
        // Remove last newline
        if(strlen(opt_in_curr) > 0) opt_in_curr[strlen(opt_in_curr)-1] = '\0';
        create_roller_row(cont_chg_settings, "Input Current Limit (mA)\n(maximum current drawn from USB)", opt_in_curr, input_curr_cb, sy6970_get_input_current_limit());
        free(opt_in_curr);
    }

    // 2. Input Voltage Limit (VINDPM): 3900-15300 step 100
    // Range is large (115 items).
    char * opt_in_volt = (char*)malloc(1024);
    if(opt_in_volt) {
        opt_in_volt[0] = '\0';
        for(int i=3900; i<=15300; i+=100) {
             char tmp[8];
             // Check remaining space? 1024 bytes / ~6 chars per entry = ~170 entries. 115 is safe.
             snprintf(tmp, sizeof(tmp), "%d\n", i);
             strcat(opt_in_volt, tmp);
        }
        if(strlen(opt_in_volt) > 0) opt_in_volt[strlen(opt_in_volt)-1] = '\0';
        create_roller_row(cont_chg_settings, "Min USB Charger Voltage (mV)\n(Throttles current on voltage drop)", opt_in_volt, input_volt_cb, sy6970_get_input_voltage_limit());
        free(opt_in_volt);
    }

    // 3. Fast Charge Current: 0-5056 step 64
    char * opt_chg_curr = (char*)malloc(1024);
    if(opt_chg_curr) {
        opt_chg_curr[0] = '\0';
        for(int i=0; i<=5056; i+=64) {
             char tmp[8];
             snprintf(tmp, sizeof(tmp), "%d\n", i);
             strcat(opt_chg_curr, tmp);
        }
        if(strlen(opt_chg_curr) > 0) opt_chg_curr[strlen(opt_chg_curr)-1] = '\0';
        create_roller_row(cont_chg_settings, "Fast Charge Current (mA)\n(Battery Volts > 3.0V)", opt_chg_curr, chg_curr_cb, sy6970_get_charge_current_limit());
        free(opt_chg_curr);
    }

    // 4. Pre-Charge Current: 64-1024 step 64
    // 5. Termination Current: 64-1024 step 64
    char * opt_small_curr = (char*)malloc(256);
    if(opt_small_curr) {
        opt_small_curr[0] = '\0';
        for(int i=64; i<=1024; i+=64) {
             char tmp[8];
             snprintf(tmp, sizeof(tmp), "%d\n", i);
             strcat(opt_small_curr, tmp);
        }
        if(strlen(opt_small_curr) > 0) opt_small_curr[strlen(opt_small_curr)-1] = '\0';
        
        create_roller_row(cont_chg_settings, "Pre-Charge Current (mA)\n(Battery Volts < 3.0V)", opt_small_curr, pre_curr_cb, sy6970_get_precharge_current_limit());
        create_roller_row(cont_chg_settings, "Termination Current (mA)\n(as Max Battery Voltage Reached)", opt_small_curr, term_curr_cb, sy6970_get_termination_current_limit());
        
        free(opt_small_curr);
    }

    // 6. Charge Voltage: 3840-4608 step 16
    char * opt_chg_volt = (char*)malloc(512);
    if(opt_chg_volt) {
        opt_chg_volt[0] = '\0';
        for(int i=3840; i<=4608; i+=16) {
             char tmp[8];
             snprintf(tmp, sizeof(tmp), "%d\n", i);
             strcat(opt_chg_volt, tmp);
        }
        if(strlen(opt_chg_volt) > 0) opt_chg_volt[strlen(opt_chg_volt)-1] = '\0';
        create_roller_row(cont_chg_settings, "Max Battery Charge Voltage (mV)", opt_chg_volt, chg_volt_cb, sy6970_get_charge_voltage_limit());
        free(opt_chg_volt);
    }

    // 7. Min System Voltage: 3000-3700 step 100
    char * opt_sys_min = (char*)malloc(128);
    if(opt_sys_min) {
        opt_sys_min[0] = '\0';
        for(int i=3000; i<=3700; i+=100) {
             char tmp[8];
             snprintf(tmp, sizeof(tmp), "%d\n", i);
             strcat(opt_sys_min, tmp);
        }
        if(strlen(opt_sys_min) > 0) opt_sys_min[strlen(opt_sys_min)-1] = '\0';
        create_roller_row(cont_chg_settings, "Min System Voltage (mV)\n(When Charging)", opt_sys_min, sys_min_cb, sy6970_get_min_system_voltage_limit());
        free(opt_sys_min);
    }

    // 8. Enable OTG Switch
    lv_obj_t * row_otg = lv_obj_create(cont_chg_settings);
    lv_obj_set_width(row_otg, LV_PCT(100));
    lv_obj_set_height(row_otg, 70);
    lv_obj_set_style_bg_opa(row_otg, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_otg, 0, 0);
    lv_obj_set_style_pad_all(row_otg, 5, 0);
    lv_obj_set_flex_flow(row_otg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_otg, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl_otg = lv_label_create(row_otg);
    lv_label_set_text(lbl_otg, "Enable On The Go (OTG)");
    lv_obj_set_style_text_color(lbl_otg, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_otg, &lv_font_montserrat_18, 0);

    lv_obj_t * sw_otg = lv_switch_create(row_otg);
    lv_obj_set_size(sw_otg, 80, 40);
    
    // Set initial state
    bool otg_en = sy6970_get_otg_status();
    if(otg_en) lv_obj_add_state(sw_otg, LV_STATE_CHECKED);
    else lv_obj_remove_state(sw_otg, LV_STATE_CHECKED);
    
    lv_obj_add_event_cb(sw_otg, otg_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 9. Boost Voltage Roller: 4550-5510 step 64
    char * opt_boost_volt = (char*)malloc(512);
    if(opt_boost_volt) {
        opt_boost_volt[0] = '\0';
        for(int i=4550; i<=5510; i+=64) {
             char tmp[8];
             snprintf(tmp, sizeof(tmp), "%d\n", i);
             strcat(opt_boost_volt, tmp);
        }
        if(strlen(opt_boost_volt) > 0) opt_boost_volt[strlen(opt_boost_volt)-1] = '\0';
        
        roller_boost_volt = create_roller_row(cont_chg_settings, "OTG Boost Voltage (mV)", opt_boost_volt, boost_volt_cb, sy6970_get_boost_voltage());
        
        // Initial state logic dependency
        if(!otg_en) {
            lv_obj_add_state(roller_boost_volt, LV_STATE_DISABLED);
        }
        
        free(opt_boost_volt);
    }

    // 10. HIZ Mode Switch (Disable USB Power)
    lv_obj_t * row_hiz = lv_obj_create(cont_chg_settings);
    lv_obj_set_width(row_hiz, LV_PCT(100));
    lv_obj_set_height(row_hiz, 70);
    lv_obj_set_style_bg_opa(row_hiz, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_hiz, 0, 0);
    lv_obj_set_style_pad_all(row_hiz, 5, 0);
    lv_obj_set_flex_flow(row_hiz, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_hiz, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl_hiz = lv_label_create(row_hiz);
    lv_label_set_text(lbl_hiz, "Disable USB Power not data\nBattery Power Only");
    lv_obj_set_style_text_color(lbl_hiz, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_hiz, &lv_font_montserrat_18, 0);

    lv_obj_t * sw_hiz = lv_switch_create(row_hiz);
    lv_obj_set_size(sw_hiz, 80, 40);
    
    // Set initial state
    bool hiz_en = sy6970_get_hiz_status();
    if(hiz_en) lv_obj_add_state(sw_hiz, LV_STATE_CHECKED);
    else lv_obj_remove_state(sw_hiz, LV_STATE_CHECKED);
    
    lv_obj_add_event_cb(sw_hiz, hiz_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 11. Hard Shutdown
    lv_obj_t * row_off = lv_obj_create(cont_chg_settings);
    lv_obj_set_width(row_off, LV_PCT(100));
    lv_obj_set_height(row_off, 70);
    lv_obj_set_style_bg_opa(row_off, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_off, 0, 0);
    lv_obj_set_style_pad_all(row_off, 5, 0);
    lv_obj_set_flex_flow(row_off, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_off, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl_off = lv_label_create(row_off);
    lv_label_set_text(lbl_off, "10 Seconds to Hard Shutdown Battery Saver\nUSB or battery switch to reset");
    lv_obj_set_style_text_color(lbl_off, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_off, &lv_font_montserrat_18, 0);

    lv_obj_t * sw_off = lv_switch_create(row_off);
    lv_obj_set_size(sw_off, 80, 40);
    // Default off
    lv_obj_remove_state(sw_off, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_off, shutdown_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 12. Defaults Button
    // We wrap it in a container to center it properly in the flex list
    lv_obj_t * row_defaults = lv_obj_create(cont_chg_settings);
    lv_obj_set_width(row_defaults, LV_PCT(100));
    lv_obj_set_height(row_defaults, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row_defaults, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_defaults, 0, 0);
    lv_obj_set_style_pad_all(row_defaults, 5, 0);
    lv_obj_set_flex_flow(row_defaults, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_defaults, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_defaults = lv_button_create(row_defaults);
    lv_obj_set_width(btn_defaults, LV_SIZE_CONTENT);
    lv_obj_set_height(btn_defaults, LV_SIZE_CONTENT); // Auto height for multiline
    lv_obj_set_style_bg_color(btn_defaults, lv_color_black(), 0); // Black Background
    lv_obj_set_style_border_color(btn_defaults, lv_color_hex(0xFF4500), 0); // Orange Border
    lv_obj_set_style_bg_color(btn_defaults, lv_color_hex(0xFF4500), LV_STATE_PRESSED); // Pressed: Orange Background
    lv_obj_set_style_border_width(btn_defaults, 2, 0);
    lv_obj_set_style_pad_all(btn_defaults, 10, 0); // Padding for content
    lv_obj_set_style_radius(btn_defaults, 10, 0);
    lv_obj_set_style_margin_top(btn_defaults, 10, 0);
    lv_obj_set_style_margin_bottom(btn_defaults, 10, 0);
    
    lv_obj_t * lbl_defaults = lv_label_create(btn_defaults);
    lv_label_set_text(lbl_defaults, "PMIC Defaults\n(Best Practices)");
    lv_obj_set_style_text_align(lbl_defaults, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_defaults, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_defaults, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_defaults);
    
    lv_obj_add_event_cb(btn_defaults, defaults_btn_cb, LV_EVENT_CLICKED, NULL);
}

void ui_sys_info_create(lv_obj_t * parent) {
    // --- System Info Container ---
    sys_info_cont = lv_obj_create(parent);
    lv_obj_set_size(sys_info_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(sys_info_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(sys_info_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sys_info_cont, 0, 0);
    lv_obj_set_style_pad_all(sys_info_cont, 10, 0);
    lv_obj_set_flex_flow(sys_info_cont, LV_FLEX_FLOW_ROW);
    lv_obj_add_flag(sys_info_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sys_info_cont, settings_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    
    lv_obj_t * img_swipe = lv_image_create(sys_info_cont);
    lv_image_set_src(img_swipe, &swipeR34);
    lv_obj_add_flag(img_swipe, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(img_swipe, LV_ALIGN_TOP_LEFT, 0, 15);

    lv_obj_t * lbl_sys_title = lv_label_create(sys_info_cont);
    lv_label_set_text(lbl_sys_title, "System Information");
    lv_obj_set_style_text_color(lbl_sys_title, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(lbl_sys_title, &lv_font_montserrat_28, 0);
    lv_obj_add_flag(lbl_sys_title, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(lbl_sys_title, LV_ALIGN_TOP_MID, 0, 15);

    // Inner Panel
    lv_obj_t * sys_panel = lv_obj_create(sys_info_cont);
    lv_obj_set_size(sys_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(sys_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(sys_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sys_panel, 1, 0);
    lv_obj_set_style_border_color(sys_panel, lv_color_hex(0x404040), 0);
    lv_obj_set_flex_flow(sys_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sys_panel, 10, 0);
    lv_obj_set_style_margin_top(sys_panel, 70, 0);
    lv_obj_add_event_cb(sys_panel, settings_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(sys_panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t * cont_sys_details = lv_obj_create(sys_panel);
    lv_obj_set_width(cont_sys_details, LV_PCT(100));
    lv_obj_set_height(cont_sys_details, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_sys_details, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_sys_details, 0, 0);
    lv_obj_set_flex_flow(cont_sys_details, LV_FLEX_FLOW_COLUMN);

    lbl_sys_info = lv_label_create(cont_sys_details);
    lv_label_set_text(lbl_sys_info, "System Info:\nLoading...");
    lv_obj_set_style_text_color(lbl_sys_info, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_sys_info, &lv_font_montserrat_22, 0);

    // OTA Update Button Container
    lv_obj_t * row_ota = lv_obj_create(cont_sys_details);
    lv_obj_set_width(row_ota, LV_PCT(100));
    lv_obj_set_height(row_ota, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row_ota, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_ota, 0, 0);
    lv_obj_set_style_margin_top(row_ota, 20, 0);
    lv_obj_set_flex_flow(row_ota, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_ota, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_ota = lv_button_create(row_ota);
    lv_obj_set_width(btn_ota, LV_SIZE_CONTENT);
    lv_obj_set_height(btn_ota, 50);
    
    // Consistent Neon Style
    lv_color_t color = lv_color_hex(0x800080); // Purple
    lv_obj_set_style_bg_opa(btn_ota, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_ota, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_ota, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_ota, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_ota, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    // Pressed
    lv_obj_set_style_bg_opa(btn_ota, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn_ota, color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn_ota, 30, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(btn_ota, color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(btn_ota, 20, 0); // Padding for text

    lv_obj_add_event_cb(btn_ota, btn_start_update_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lbl_ota = lv_label_create(btn_ota);
    lv_label_set_text(lbl_ota, "Update Firmware");
    lv_obj_set_style_text_font(lbl_ota, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl_ota, lv_color_white(), 0);
    lv_obj_center(lbl_ota);
}
