#include "ui_private.h"
#include "esp_log.h"
#include "wifi_mgr.h"
#include <time.h>
#include <sys/time.h>

static const char *TAG = "ui_home";

LV_IMG_DECLARE(img_watermelon);
LV_IMG_DECLARE(img_venezuela);

static lv_obj_t * lbl_header_time = NULL;
static lv_obj_t * lbl_header_wifi = NULL;
static lv_timer_t * status_timer = NULL;

static void status_bar_timer_cb(lv_timer_t * t) {
    // Update Time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Check if time is set (year > 2020) - otherwise show --:--
    if (timeinfo.tm_year > (2020 - 1900)) {
        lv_label_set_text_fmt(lbl_header_time, "%02d/%02d/%04d %02d:%02d", 
            timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_year + 1900,
            timeinfo.tm_hour, timeinfo.tm_min);
    } else {
        lv_label_set_text(lbl_header_time, "http d/t requested . . .");
    }
    
    // Update WiFi Icon
    if (wifi_mgr_is_connected()) {
        lv_label_set_text(lbl_header_wifi, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(lbl_header_wifi, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_label_set_text(lbl_header_wifi, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(lbl_header_wifi, lv_palette_main(LV_PALETTE_RED), 0);
    }
}

static void home_cleanup_cb(lv_event_t * e) {
    if (status_timer) {
        lv_timer_del(status_timer);
        status_timer = NULL;
    }
    lbl_header_time = NULL;
    lbl_header_wifi = NULL;
}

// View IDs for safe view switching
typedef enum {
    VIEW_NONE = 0,
    VIEW_HOME,
    VIEW_PMIC,
    VIEW_SETTINGS,
    VIEW_MEDIA,
    VIEW_DISPLAY,
    VIEW_NETWORK,
    VIEW_SYSINFO
} view_id_t;

static volatile view_id_t s_pending_view = VIEW_NONE;
static lv_timer_t * s_switch_timer = NULL;

// Timer callback to safely switch views outside of event processing
static void view_switch_timer_cb(lv_timer_t * timer) {
    (void)timer;
    view_id_t view = s_pending_view;
    s_pending_view = VIEW_NONE;
    s_switch_timer = NULL;
    
    if (view == VIEW_NONE) {
        ESP_LOGW(TAG, "view_switch_timer_cb called but view is NONE");
        return;
    }
    
    ESP_LOGI(TAG, "Switching to view %d", view);
    
    clear_current_view();
    ESP_LOGI(TAG, "clear_current_view done");
    
    lv_obj_t * scr = lv_screen_active();
    switch (view) {
        case VIEW_HOME:
            ui_home_create(scr);
            break;
        case VIEW_PMIC:
            ESP_LOGI(TAG, "Creating PMIC view");
            ui_pmic_create(scr);
            ESP_LOGI(TAG, "PMIC view created");
            break;
        case VIEW_SETTINGS:
            ui_settings_create(scr);
            break;
        case VIEW_MEDIA:
            ui_media_create(scr);
            populate_sd_files_list();
            break;
        case VIEW_DISPLAY:
            ui_display_create(scr);
            if (lv_display_get_default() && lbl_disp_info) {
                int32_t w = lv_display_get_horizontal_resolution(lv_display_get_default());
                int32_t h = lv_display_get_vertical_resolution(lv_display_get_default());
                lv_label_set_text_fmt(lbl_disp_info, "Driver Resolution: 450x600\nActual Pixel Resolution: %" LV_PRId32 "x%" LV_PRId32 "\nDriver: RM690B0\nInterface: QSPI", w, h);
            }
            break;
        case VIEW_NETWORK:
            ui_network_create(scr);
            break;
        case VIEW_SYSINFO:
            ui_sys_info_create(scr);
            break;
        default:
            ESP_LOGE(TAG, "Unknown view: %d", view);
            break;
    }
    ESP_LOGI(TAG, "View switch complete");
}

// Request a view change - actual switch happens in timer callback
static void request_view_switch(view_id_t view) {
    ESP_LOGI(TAG, "request_view_switch: %d", view);
    
    // Cancel any pending timer
    if (s_switch_timer) {
        ESP_LOGI(TAG, "Deleting existing switch timer");
        lv_timer_delete(s_switch_timer);
        s_switch_timer = NULL;
    }
    
    s_pending_view = view;
    // Create a one-shot timer to do the switch safely
    s_switch_timer = lv_timer_create(view_switch_timer_cb, 10, NULL);
    lv_timer_set_repeat_count(s_switch_timer, 1);
    ESP_LOGI(TAG, "Switch timer created");
}

void show_home_view(lv_event_t * e) {
    (void)e;
    request_view_switch(VIEW_HOME);
}

// Button event handlers - just request the view change
static void btn_pmic_cb(lv_event_t * e) {
    (void)e;
    ESP_LOGI(TAG, "PM Status button clicked");
    request_view_switch(VIEW_PMIC);
}

static void btn_settings_cb(lv_event_t * e) {
    (void)e;
    ESP_LOGI(TAG, "Settings button clicked");
    request_view_switch(VIEW_SETTINGS);
}

static void btn_media_cb(lv_event_t * e) {
    (void)e;
    ESP_LOGI(TAG, "SD Card button clicked");
    request_view_switch(VIEW_MEDIA);
}

static void btn_display_cb(lv_event_t * e) {
    (void)e;
    ESP_LOGI(TAG, "Display button clicked");
    request_view_switch(VIEW_DISPLAY);
}

static void btn_sysinfo_cb(lv_event_t * e) {
    (void)e;
    ESP_LOGI(TAG, "System button clicked");
    request_view_switch(VIEW_SYSINFO);
}

static void btn_ota_cb(lv_event_t * e) {
    (void)e;
    ESP_LOGI(TAG, "OTA button clicked");
    request_view_switch(VIEW_NETWORK);
}

static void create_neon_btn(lv_obj_t * parent, const char * icon, const char * text, lv_color_t color, lv_event_cb_t event_cb) {
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_set_height(btn, 95);
    lv_obj_set_width(btn, LV_PCT(30));
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn, 4, 0);
    lv_obj_set_style_pad_gap(btn, 4, 0);

    // Default Style
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Pressed Style
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 30, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(btn, color, LV_PART_MAIN | LV_STATE_PRESSED);
    
    // Icon
    lv_obj_t * lbl_icon = lv_label_create(btn);
    lv_label_set_text(lbl_icon, icon);
    lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_white(), 0);

    // Label
    lv_obj_t * lbl_text = lv_label_create(btn);
    lv_label_set_text(lbl_text, text);
    lv_obj_set_style_text_font(lbl_text, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_text, lv_color_white(), 0);
}

void ui_home_create(lv_obj_t * parent) {
    // --- Home Container ---
    home_cont = lv_obj_create(parent);
    lv_obj_set_size(home_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(home_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(home_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(home_cont, 0, 0);
    lv_obj_set_style_pad_all(home_cont, 20, 0);
    lv_obj_set_style_pad_row(home_cont, 10, 0);
    lv_obj_set_flex_flow(home_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(home_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // --- Status Bar (Floating) ---
    lv_obj_t * header_row = lv_obj_create(home_cont);
    lv_obj_set_width(header_row, LV_PCT(100));
    lv_obj_set_height(header_row, LV_SIZE_CONTENT);
    lv_obj_add_flag(header_row, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_align(header_row, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_row, 0, 0);
    lv_obj_set_style_pad_all(header_row, 5, 0); // Padding from edge
    
    // Time Label (Top Left)
    lbl_header_time = lv_label_create(header_row);
    lv_obj_set_align(lbl_header_time, LV_ALIGN_LEFT_MID);
    lv_label_set_text(lbl_header_time, "http d/t requested . . .");
    lv_obj_set_style_text_font(lbl_header_time, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl_header_time, lv_color_white(), 0);

    // WiFi Icon (Top Right)
    lbl_header_wifi = lv_label_create(header_row);
    lv_obj_set_align(lbl_header_wifi, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(lbl_header_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lbl_header_wifi, &lv_font_montserrat_24, 0); // Larger icon
    lv_obj_set_style_text_color(lbl_header_wifi, lv_palette_main(LV_PALETTE_RED), 0);

    // Cleanup callback
    lv_obj_add_event_cb(home_cont, home_cleanup_cb, LV_EVENT_DELETE, NULL);
    
    // Timer
    if (status_timer) {
        lv_timer_del(status_timer);
    }
    status_timer = lv_timer_create(status_bar_timer_cb, 1000, NULL);
    status_bar_timer_cb(NULL); // Initial update
    
    // 1. Title/Image Row Container
    lv_obj_t * title_row = lv_obj_create(home_cont);
    lv_obj_set_size(title_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(title_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(title_row, 10, 0);
    lv_obj_set_style_pad_gap(title_row, 15, 0);

    // Watermelon Image (Left)
    lv_obj_t * icon_watermelon = lv_image_create(title_row);
    lv_image_set_src(icon_watermelon, &img_watermelon);
    lv_obj_set_style_align(icon_watermelon, LV_ALIGN_CENTER, 0);

    // Title Container (Center)
    lv_obj_t * title_cont = lv_obj_create(title_row);
    lv_obj_set_size(title_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(title_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_cont, 0, 0);
    lv_obj_set_flex_flow(title_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(title_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Create a spangroup for the title
    lv_obj_t * spangroup = lv_spangroup_create(title_cont);
    lv_spangroup_set_mode(spangroup, LV_SPAN_MODE_EXPAND);
    lv_obj_set_width(spangroup, LV_SIZE_CONTENT);
    lv_obj_set_height(spangroup, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(spangroup, &lv_font_montserrat_30, 0);
    
    lv_span_t * span;
    lv_style_t * style;

    // L - Red
    span = lv_spangroup_add_span(spangroup);
    lv_spangroup_set_span_text(spangroup, span, "L");
    style = lv_span_get_style(span);
    lv_style_set_text_color(style, lv_palette_main(LV_PALETTE_RED));

    // V - Orange
    span = lv_spangroup_add_span(spangroup);
    lv_spangroup_set_span_text(spangroup, span, "V");
    style = lv_span_get_style(span);
    lv_style_set_text_color(style, lv_palette_main(LV_PALETTE_ORANGE));

    // G - Yellow
    span = lv_spangroup_add_span(spangroup);
    lv_spangroup_set_span_text(spangroup, span, "G");
    style = lv_span_get_style(span);
    lv_style_set_text_color(style, lv_palette_main(LV_PALETTE_YELLOW));

    // L - Green
    span = lv_spangroup_add_span(spangroup);
    lv_spangroup_set_span_text(spangroup, span, "L");
    style = lv_span_get_style(span);
    lv_style_set_text_color(style, lv_palette_main(LV_PALETTE_GREEN));

    // Space
    span = lv_spangroup_add_span(spangroup);
    lv_spangroup_set_span_text(spangroup, span, " ");

    // D - Cyan
    span = lv_spangroup_add_span(spangroup);
    lv_spangroup_set_span_text(spangroup, span, "D");
    style = lv_span_get_style(span);
    lv_style_set_text_color(style, lv_palette_main(LV_PALETTE_CYAN));

    // e - Blue
    span = lv_spangroup_add_span(spangroup);
    lv_spangroup_set_span_text(spangroup, span, "e");
    style = lv_span_get_style(span);
    lv_style_set_text_color(style, lv_palette_main(LV_PALETTE_BLUE));

    // m - Purple
    span = lv_spangroup_add_span(spangroup);
    lv_spangroup_set_span_text(spangroup, span, "m");
    style = lv_span_get_style(span);
    lv_style_set_text_color(style, lv_palette_main(LV_PALETTE_PURPLE));

    // o - Pink
    span = lv_spangroup_add_span(spangroup);
    lv_spangroup_set_span_text(spangroup, span, "o");
    style = lv_span_get_style(span);
    lv_style_set_text_color(style, lv_palette_main(LV_PALETTE_PINK));
    
    lv_spangroup_refresh(spangroup);

    // Subtitle
    lv_obj_t * subtitle = lv_label_create(title_cont);
    lv_label_set_text(subtitle, "Double tap the boot\nbutton to rotate");
    lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xFFD700), 0); // Gold
    lv_obj_set_style_margin_bottom(subtitle, -5, 0);

    // Venezuela Flag Image (Right)
    lv_obj_t * icon_venezuela = lv_image_create(title_row);
    lv_image_set_src(icon_venezuela, &img_venezuela);
    lv_obj_set_style_align(icon_venezuela, LV_ALIGN_CENTER, 0);

    // 2. Button Container (Row 1)
    lv_obj_t * btn_row1 = lv_obj_create(home_cont);
    lv_obj_set_width(btn_row1, LV_PCT(100));
    lv_obj_set_height(btn_row1, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row1, 0, 0);
    lv_obj_set_flex_flow(btn_row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_row1, 8, 0);
    lv_obj_set_style_pad_all(btn_row1, 0, 0);
    lv_obj_remove_flag(btn_row1, LV_OBJ_FLAG_CLICKABLE);  // Let clicks pass through to buttons

    // Button 1: PM Status (Neon Red/Orange)
    create_neon_btn(btn_row1, LV_SYMBOL_CHARGE, "PM Status", lv_color_hex(0xFF3300), btn_pmic_cb);

    // Button 2: Set PM (Neon Blue)
    create_neon_btn(btn_row1, LV_SYMBOL_SETTINGS, "Set PM", lv_color_hex(0x007FFF), btn_settings_cb);

    // Button 3: SD Card (Neon Cyan)
    create_neon_btn(btn_row1, LV_SYMBOL_SD_CARD, "SD Card", lv_color_hex(0x00FFFF), btn_media_cb);

    // 3. Button Container (Row 2)
    lv_obj_t * btn_row2 = lv_obj_create(home_cont);
    lv_obj_set_width(btn_row2, LV_PCT(100));
    lv_obj_set_height(btn_row2, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row2, 0, 0);
    lv_obj_set_flex_flow(btn_row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_row2, 8, 0);
    lv_obj_set_style_pad_all(btn_row2, 0, 0);
    lv_obj_remove_flag(btn_row2, LV_OBJ_FLAG_CLICKABLE);  // Let clicks pass through to buttons

    // Display (Neon Green)
    create_neon_btn(btn_row2, LV_SYMBOL_EYE_OPEN, "Display", lv_color_hex(0x39FF14), btn_display_cb);

    // Button 5: System (Neon Purple)
    create_neon_btn(btn_row2, LV_SYMBOL_FILE, "System OTA", lv_color_hex(0x9D00FF), btn_sysinfo_cb);

    // Button 6: WiFi (Neon Magenta)
    create_neon_btn(btn_row2, LV_SYMBOL_WIFI, "Wi-Fi", lv_color_hex(0xFF00FF), btn_ota_cb);
}
