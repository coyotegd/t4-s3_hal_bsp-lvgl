#include "ui_private.h"
#include "esp_log.h"
#include "wifi_mgr.h"
#include "lvgl_mgr.h"
#include <string.h>

static const char *TAG = "ui_network";

LV_IMG_DECLARE(swipeR34); // Reuse swipe image

// -- UI Objects --
static lv_obj_t * wifi_list; // List for WiFi networks
static lv_obj_t * ta_log;    // Log textarea for WiFi messages
static lv_obj_t * modal_cont;
static lv_obj_t * ta_pass;
static lv_obj_t * kb;
static lv_obj_t * lbl_status;
static lv_obj_t * lbl_modal_title;
static lv_obj_t * btn_scan;

// -- State --
static wifi_scan_item_t *s_scan_results = NULL;
static int s_scan_count = 0;
static bool s_scan_ready = false;
static lv_timer_t * s_wifi_timer = NULL;
static char s_target_ssid[33];

// Helper to log to UI
static void ui_log(const char * fmt, ...) {
    if (!ta_log) return;
    
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    lvgl_mgr_lock();
    lv_textarea_add_text(ta_log, buf);
    lv_textarea_add_text(ta_log, "\n");
    lvgl_mgr_unlock();
}

static void wifi_timer_cb(lv_timer_t * t);

// -- WiFi Callbacks --

static int compare_rssi_desc(const void *a, const void *b) {
    const wifi_scan_item_t *wa = (const wifi_scan_item_t *)a;
    const wifi_scan_item_t *wb = (const wifi_scan_item_t *)b;
    return wb->rssi - wa->rssi; // Descending order (Larger/less negative first)
}

static void scan_result_cb(wifi_scan_item_t *networks, int count) {
    lvgl_mgr_lock();
    if (s_scan_results) {
        free(s_scan_results);
        s_scan_results = NULL;
    }
    if (count > 0 && networks) {
        s_scan_results = malloc(sizeof(wifi_scan_item_t) * count);
        if (s_scan_results) {
            memcpy(s_scan_results, networks, sizeof(wifi_scan_item_t) * count);
            s_scan_count = count;
            qsort(s_scan_results, s_scan_count, sizeof(wifi_scan_item_t), compare_rssi_desc);
        }
        ui_log("Scan done. Found %d APs.", count);
    } else {
        s_scan_count = 0;
        ui_log("Scan done. No APs found.");
    }
    s_scan_ready = true;
    lvgl_mgr_unlock();
}

static void connect_result_cb(bool connected) {
    if (connected) {
        ESP_LOGI(TAG, "UI: Connected!");
        ui_log("Connection Successful.");
    } else {
        ESP_LOGW(TAG, "UI: Connect Failed");
        ui_log("Connection Failed.");
    }
}


// -- UI Event Callbacks --

static void network_swipe_event_cb(lv_event_t * e) {
    if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        show_home_view(NULL);
    }
}

static void switch_wifi_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    if(lv_obj_has_state(obj, LV_STATE_CHECKED)) {
        ui_log("Enabling Wi-Fi...");
        wifi_mgr_enable();
        if(btn_scan) lv_obj_remove_state(btn_scan, LV_STATE_DISABLED);
        lv_label_set_text(lbl_status, "Ready to Scan");
    } else {
        ui_log("Disabling Wi-Fi...");
        wifi_mgr_disable();
        if (wifi_list) lv_obj_clean(wifi_list);
        lv_label_set_text(lbl_status, "Wi-Fi Disabled");
        if(btn_scan) lv_obj_add_state(btn_scan, LV_STATE_DISABLED);
    }
}

static void btn_scan_cb(lv_event_t * e) {
    lv_label_set_text(lbl_status, "Scanning...");
    ui_log("Starting Scan...");
    lv_obj_clean(wifi_list);
    wifi_mgr_start_scan(scan_result_cb);
}

static void btn_connect_click_cb(lv_event_t * e) {
    const char * pass = lv_textarea_get_text(ta_pass);
    lv_label_set_text(lbl_status, "Connecting...");
    ui_log("Connecting to %s...", s_target_ssid);
    
    wifi_mgr_connect(s_target_ssid, pass, connect_result_cb);
    
    // Hide modal
    lv_obj_add_flag(modal_cont, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(ta_pass, "");
    lv_obj_remove_state(ta_pass, LV_STATE_FOCUSED);
}

static void btn_cancel_click_cb(lv_event_t * e) {
    lv_obj_add_flag(modal_cont, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(ta_pass, "");
    lv_obj_remove_state(ta_pass, LV_STATE_FOCUSED);
}

static void wifi_list_btn_cb(lv_event_t * e) {
    const char * ssid = (const char *)lv_event_get_user_data(e);
    if (ssid) {
        strncpy(s_target_ssid, ssid, 32);
        s_target_ssid[32] = '\0';
        
        // Show password modal
        lv_obj_remove_flag(modal_cont, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(lbl_modal_title, "Connect to %s", s_target_ssid);
        lv_textarea_set_text(ta_pass, "");
        lv_obj_add_state(ta_pass, LV_STATE_FOCUSED);
    }
}

static void wifi_timer_cb(lv_timer_t * t) {
    if (s_scan_ready) {
        s_scan_ready = false;
        if (s_scan_count == 0) {
             lv_label_set_text(lbl_status, "No networks found.");
        } else {
             lv_label_set_text_fmt(lbl_status, "Found %d networks", s_scan_count);
        }
        
        lv_obj_clean(wifi_list);
        
        for (int i = 0; i < s_scan_count; i++) {
             // Skip nameless stations
             if (strlen(s_scan_results[i].ssid) == 0) continue;

             lv_obj_t * btn = lv_button_create(wifi_list);
             lv_obj_set_width(btn, LV_PCT(100));
             lv_obj_set_height(btn, LV_SIZE_CONTENT);
             lv_obj_add_event_cb(btn, wifi_list_btn_cb, LV_EVENT_CLICKED, s_scan_results[i].ssid);

             // Style: Match SD Media list (Transparent default, Dark Grey pressed)
             lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
             lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
             lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
             lv_obj_set_style_radius(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
             lv_obj_set_style_pad_all(btn, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
             
             // Pressed Style
             lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
             lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN | LV_STATE_PRESSED);

             // SSID Label (Left)
             lv_obj_t * lbl_ssid = lv_label_create(btn);
             lv_label_set_text_fmt(lbl_ssid, "%s  %s", LV_SYMBOL_WIFI, s_scan_results[i].ssid);
             lv_obj_set_style_text_font(lbl_ssid, &lv_font_montserrat_24, 0);
             lv_obj_set_style_text_color(lbl_ssid, lv_color_white(), 0);
             lv_obj_set_width(lbl_ssid, LV_PCT(60));
             lv_label_set_long_mode(lbl_ssid, LV_LABEL_LONG_DOT);
             lv_obj_align(lbl_ssid, LV_ALIGN_LEFT_MID, 0, 0);

             // Lock/Unlock Symbol (Center-Right)
             lv_obj_t * lbl_lock = lv_label_create(btn);
             lv_obj_set_style_text_font(lbl_lock, &lv_font_montserrat_24, 0);
             
             if (s_scan_results[i].auth_mode != 0) {
                 lv_label_set_text(lbl_lock, LV_SYMBOL_WARNING); // Locked
                 lv_obj_set_style_text_color(lbl_lock, lv_color_hex(0xFFD700), 0); // Gold
             } else {
                 lv_label_set_text(lbl_lock, ""); // Unlocked
             }
             lv_obj_align(lbl_lock, LV_ALIGN_RIGHT_MID, -120, 0);

             // RSSI Label (Right)
             lv_obj_t * lbl_rssi = lv_label_create(btn);
             lv_label_set_text_fmt(lbl_rssi, "%d dBm", s_scan_results[i].rssi);
             lv_obj_set_style_text_font(lbl_rssi, &lv_font_montserrat_24, 0);
             lv_obj_set_style_text_color(lbl_rssi, lv_color_white(), 0);
             lv_obj_align(lbl_rssi, LV_ALIGN_RIGHT_MID, 0, 0);
        }
    }
    
    // Check connection status poll
    static bool was_connected = false;
    if (wifi_mgr_is_connected()) {
        if (!was_connected) {
            const char* ip = wifi_mgr_get_ip();
            lv_label_set_text_fmt(lbl_status, "Connected: %s", ip);
            ui_log("Obtained IP: %s", ip);
            was_connected = true;
        }
    } else {
        was_connected = false; // Reset if disconnected
    }
}

void ui_network_create(lv_obj_t * parent) {
    // --- Network Container ---
    network_cont = lv_obj_create(parent);
    lv_obj_set_size(network_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(network_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(network_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(network_cont, 0, 0);
    lv_obj_set_style_pad_all(network_cont, 10, 0);
    lv_obj_set_flex_flow(network_cont, LV_FLEX_FLOW_ROW);
    lv_obj_add_flag(network_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(network_cont, network_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    
    // Swipe & Title
    lv_obj_t * img_swipe = lv_image_create(network_cont);
    lv_image_set_src(img_swipe, &swipeR34);
    lv_obj_add_flag(img_swipe, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(img_swipe, LV_ALIGN_TOP_LEFT, 0, 15);

    lv_obj_t * lbl_title = lv_label_create(network_cont);
    lv_label_set_text(lbl_title, "Connectivity");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_28, 0);
    lv_obj_add_flag(lbl_title, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 15);

    // Inner Panel
    lv_obj_t * panel = lv_obj_create(network_cont);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x404040), 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_set_style_margin_top(panel, 70, 0);
    lv_obj_add_event_cb(panel, network_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Wi-Fi Enable Switch
    lv_obj_t * row_sw = lv_obj_create(panel);
    lv_obj_set_width(row_sw, LV_PCT(100));
    lv_obj_set_height(row_sw, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row_sw, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_sw, 0, 0);
    lv_obj_set_style_pad_all(row_sw, 0, 0);
    lv_obj_set_flex_flow(row_sw, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_sw, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * lbl_sw = lv_label_create(row_sw);
    lv_label_set_text(lbl_sw, "Enable Wi-Fi");
    lv_obj_set_style_text_color(lbl_sw, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_sw, &lv_font_montserrat_22, 0);

    lv_obj_t * sw_wifi = lv_switch_create(row_sw);
    lv_obj_set_size(sw_wifi, 80, 40);
    if(wifi_mgr_is_enabled()) lv_obj_add_state(sw_wifi, LV_STATE_CHECKED);
    else lv_obj_remove_state(sw_wifi, LV_STATE_CHECKED);

    lv_obj_add_event_cb(sw_wifi, switch_wifi_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // Removed custom color to match ADC switch style

    // WiFi Scan Header (Row)
    lv_obj_t * row_scan = lv_obj_create(panel);
    lv_obj_set_width(row_scan, LV_PCT(100));
    lv_obj_set_height(row_scan, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row_scan, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_scan, 0, 0);
    lv_obj_set_style_pad_all(row_scan, 0, 0);
    lv_obj_set_flex_flow(row_scan, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_scan, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lbl_status = lv_label_create(row_scan);
    lv_label_set_text(lbl_status, "Ready to Scan");
    lv_obj_set_style_text_color(lbl_status, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_22, 0);

    btn_scan = lv_btn_create(row_scan);
    lv_obj_set_size(btn_scan, 120, 50);
    // Neon Style (based on ui_home)
    lv_color_t neon_color = lv_palette_main(LV_PALETTE_CYAN); // Using Cyan for Scan
    
    // Default Style
    lv_obj_set_style_bg_opa(btn_scan, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_scan, neon_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_scan, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_scan, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_scan, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Pressed Style
    lv_obj_set_style_bg_opa(btn_scan, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn_scan, neon_color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn_scan, 30, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(btn_scan, neon_color, LV_PART_MAIN | LV_STATE_PRESSED);

    // Disabled Style
    lv_obj_set_style_bg_opa(btn_scan, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_border_color(btn_scan, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_text_color(btn_scan, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN | LV_STATE_DISABLED);

    // Set initial state
    if (!wifi_mgr_is_enabled()) {
        lv_obj_add_state(btn_scan, LV_STATE_DISABLED);
        lv_label_set_text(lbl_status, "Wi-Fi Disabled");
    }

    lv_obj_t * lbl_btn_scan = lv_label_create(btn_scan);
    lv_label_set_text(lbl_btn_scan, "SCAN");
    lv_obj_set_style_text_font(lbl_btn_scan, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl_btn_scan, lv_color_white(), 0);
    lv_obj_center(lbl_btn_scan);
    
    lv_obj_add_event_cb(btn_scan, btn_scan_cb, LV_EVENT_CLICKED, NULL);

    // WiFi List Container (Transparent)
    wifi_list = lv_obj_create(panel);
    lv_obj_set_width(wifi_list, LV_PCT(100));
    lv_obj_set_flex_grow(wifi_list, 2); 
    lv_obj_set_style_bg_opa(wifi_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_list, 0, 0);
    lv_obj_set_style_pad_all(wifi_list, 0, 0);
    lv_obj_set_flex_flow(wifi_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(wifi_list, LV_OBJ_FLAG_SCROLLABLE);

    // Log Area
    ta_log = lv_textarea_create(panel);
    lv_obj_set_width(ta_log, LV_PCT(100));
    lv_obj_set_height(ta_log, 120); // Decreased from 150 to give 30px to list
    lv_obj_set_style_bg_color(ta_log, lv_color_make(10, 10, 10), 0);
    lv_obj_set_style_text_color(ta_log, lv_color_make(0, 255, 0), 0); // Green terminal text
    lv_obj_set_style_text_font(ta_log, &lv_font_montserrat_16, 0);
    lv_obj_remove_flag(ta_log, LV_OBJ_FLAG_CLICKABLE); // Read-only: prevent focus
    lv_obj_set_style_margin_top(ta_log, 10, 0);

    // --- Modal for Password ---
    // Create in top layer to block everything
    modal_cont = lv_obj_create(lv_layer_top()); 
    lv_obj_set_size(modal_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(modal_cont, lv_color_make(20, 20, 20), 0);
    lv_obj_set_style_bg_opa(modal_cont, LV_OPA_COVER, 0);
    lv_obj_add_flag(modal_cont, LV_OBJ_FLAG_HIDDEN); 
    lv_obj_set_flex_flow(modal_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(modal_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Header for Modal (Title + Cancel Button)
    lv_obj_t * modal_header = lv_obj_create(modal_cont);
    lv_obj_set_width(modal_header, LV_PCT(90));
    lv_obj_set_height(modal_header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(modal_header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(modal_header, 0, 0);
    lv_obj_set_flex_flow(modal_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(modal_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lbl_modal_title = lv_label_create(modal_header);
    lv_label_set_text(lbl_modal_title, "Connect to Network");
    lv_obj_set_style_text_font(lbl_modal_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_modal_title, lv_color_white(), 0);

    lv_obj_t * btn_cancel = lv_btn_create(modal_header);
    lv_obj_set_size(btn_cancel, 120, 50);
    
    // Neon Style for Cancel (Purple accent)
    lv_color_t cancel_color = lv_color_hex(0x800080); // Purple
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_cancel, cancel_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_cancel, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_cancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_cancel, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    // Pressed Style
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn_cancel, cancel_color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn_cancel, 30, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(btn_cancel, cancel_color, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(btn_cancel, btn_cancel_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_cancel, lv_color_white(), 0);
    lv_obj_center(lbl_cancel);
    
    ta_pass = lv_textarea_create(modal_cont);
    lv_textarea_set_password_mode(ta_pass, true);
    lv_textarea_set_one_line(ta_pass, true);
    lv_textarea_set_placeholder_text(ta_pass, "Password");
    lv_obj_set_style_text_font(ta_pass, &lv_font_montserrat_24, 0);
    lv_obj_set_width(ta_pass, LV_PCT(90));
    lv_obj_set_style_margin_bottom(ta_pass, 10, 0);

    // Dark style for Text Area (Dim brightness)
    lv_obj_set_style_bg_color(ta_pass, lv_color_make(30, 30, 30), 0);
    lv_obj_set_style_text_color(ta_pass, lv_color_white(), 0);
    lv_obj_set_style_border_color(ta_pass, lv_color_make(80, 80, 80), 0);
    
    // Set cursor color (part cursor)
    lv_obj_set_style_bg_color(ta_pass, lv_color_white(), LV_PART_CURSOR);
    lv_obj_set_style_border_color(ta_pass, lv_color_white(), LV_PART_CURSOR);
    
    // Set placeholder color
    lv_obj_set_style_text_color(ta_pass, lv_color_make(120, 120, 120), LV_PART_TEXTAREA_PLACEHOLDER);
    
    kb = lv_keyboard_create(modal_cont);
    lv_obj_set_width(kb, LV_PCT(100));
    lv_obj_set_flex_grow(kb, 1); // Make keyboard convert remaining space
    
    // Dim the keyboard (Dark mode) - Background
    lv_obj_set_style_bg_color(kb, lv_color_make(20, 20, 20), LV_PART_MAIN);
    
    // Keys style (Dark keys, white text)
    lv_obj_set_style_bg_color(kb, lv_color_make(40, 40, 40), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, lv_color_make(20, 20, 20), LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS); // Remove shadow for flatter look
    
    // Keyboard buttons font size
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_keyboard_set_textarea(kb, ta_pass);
    lv_obj_add_event_cb(kb, btn_cancel_click_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_event_cb(kb, btn_connect_click_cb, LV_EVENT_READY, NULL);

    // Initial Timer
    if (!s_wifi_timer) {
        s_wifi_timer = lv_timer_create(wifi_timer_cb, 500, NULL);
    }

    // Show current connection status immediately if already connected
    if (wifi_mgr_is_connected()) {
        const char* ip = wifi_mgr_get_ip();
        const char* ssid = wifi_mgr_get_ssid();
        ui_log("Connected to %s", ssid);
        ui_log("IP: %s", ip);
        // Also update the label status
        lv_label_set_text_fmt(lbl_status, "Connected: %s", ip);
    }
}