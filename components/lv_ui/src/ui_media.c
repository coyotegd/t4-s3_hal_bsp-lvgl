#include "ui_private.h"
#include <dirent.h>
#include <sys/stat.h>
#include "sd_card.h" // Assuming this is available via REQUIRES sd_card
#include <stdio.h> // For snprintf
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "ui_avi.h"
#include "hal_mgr.h"
#include "rm690b0.h"

LV_IMG_DECLARE(swipeL34);
LV_IMG_DECLARE(swipeR34);

// Store current brightness value to persist across screen changes
// Initialize from NVS on first access
static uint8_t s_current_brightness = 0;
static bool s_brightness_initialized = false;
static lv_obj_t * brightness_slider = NULL;

// Metadata structures
typedef struct {
    uint32_t frame_rate;
    bool valid;
} avi_metadata_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    bool valid;
} jpg_metadata_t;

/**
 * Extract frame rate from AVI file header
 */
static avi_metadata_t extract_avi_metadata(const char *file_path) {
    avi_metadata_t meta = {0};
    
    FILE* f = fopen(file_path, "rb");
    if (!f) return meta;
    
    // Read AVI header to get microseconds per frame
    fseek(f, 0x20, SEEK_SET); // Skip to avih chunk location (approximate)
    uint32_t usec_per_frame;
    if (fread(&usec_per_frame, 4, 1, f) == 1 && usec_per_frame > 0) {
        meta.frame_rate = 1000000 / usec_per_frame;
        meta.valid = true;
    }
    fclose(f);
    
    return meta;
}

/**
 * Extract dimensions from JPEG file header
 */
static jpg_metadata_t extract_jpg_metadata(const char *file_path) {
    jpg_metadata_t meta = {0};
    
    FILE* f = fopen(file_path, "rb");
    if (!f) return meta;
    
    // Read JPEG header to get dimensions
    uint8_t buf[256];
    if (fread(buf, 1, 256, f) != 256) {
        fclose(f);
        return meta;
    }
    fclose(f);
    
    // Simple JPEG marker scan for SOF0 (0xFFC0)
    for (int i = 0; i < 240; i++) {
        if (buf[i] == 0xFF && buf[i+1] == 0xC0) {
            meta.height = (buf[i+5] << 8) | buf[i+6];
            meta.width = (buf[i+7] << 8) | buf[i+8];
            meta.valid = true;
            break;
        }
    }
    
    return meta;
}

static void init_brightness(void) {
    if (!s_brightness_initialized) {
        s_current_brightness = hal_mgr_get_brightness();
        s_brightness_initialized = true;
    }
}

static void media_swipe_event_cb(lv_event_t * e) {
    if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) {
        show_home_view(NULL);
    }
}

static void play_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * avi_obj = (lv_obj_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSED) {
        if (avi_obj) {
            ui_avi_pause(avi_obj);
            ESP_LOGI("ui_media", "Playback paused for interaction");
        }
    }
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        // Only resume if we are still on the playback screen (simple check: if obj is valid)
        // Note: If gesture triggered, screen might change. 
        // But typically Release happens before deletion if we handle gesture carefully.
        // Actually, if gesture triggers show_media_view, the cleanup happens.
        // So we might be calling play on a dying object.
        // However, ui_avi_pause/play checks if avi struct is valid.
        if (avi_obj && lv_obj_is_valid(avi_obj)) {
             ui_avi_play(avi_obj);
             ESP_LOGI("ui_media", "Playback resumed");
        }
    }
    else if (code == LV_EVENT_GESTURE) {
        if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
            show_media_view(e); 
        }
    }
}

static void rainbow_test_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    if (lv_obj_has_state(sw, LV_STATE_CHECKED)) {
        hal_mgr_show_rainbow_test();
        // Force LVGL to redraw the screen after hardware test
        lv_obj_invalidate(lv_screen_active());
        // Auto turn off switch after test completes
        lv_obj_remove_state(sw, LV_STATE_CHECKED);
    }
}

static void brightness_slider_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);
    int32_t value = lv_slider_get_value(slider);
    
    // Save current brightness value
    s_current_brightness = (uint8_t)value;
    
    // Update label
    if (label) {
        lv_label_set_text_fmt(label, "Brightness: %" LV_PRId32, value);
    }
    
    // Set hardware brightness
    rm690b0_set_brightness((uint8_t)value);
    
    // Save to NVS
    hal_mgr_save_brightness((uint8_t)value);
}

static void rotation_dropdown_cb(lv_event_t * e) {
    lv_obj_t * dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    
    // Set hardware rotation (0-3 maps directly to rotation enum)
    rm690b0_rotation_t rotation = (rm690b0_rotation_t)selected;
    hal_mgr_set_rotation(rotation);
    
    // Save to NVS
    hal_mgr_save_rotation(rotation);
    
    ESP_LOGI("ui_media", "Rotation changed to %d", rotation);
}

static void file_btn_event_handler(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    // Child 0 is file name, Child 1 is size (if added correctly)
    lv_obj_t * lbl = lv_obj_get_child(btn, 0); 
    const char * txt = lv_label_get_text(lbl);
    
    // Parse filename from format "%s  %s", LV_SYMBOL_FILE, dir->d_name
    // Skip the symbol (3 bytes usually for unicode, plus 2 spaces)
    // Actually LV_SYMBOL_FILE is equivalent to "\xEF\x85\x9B" (3 bytes)
    // Based on populate_sd_files_list: "%s  %s" -> Symbol + "  " + name
    // We can just find the first double space or just offset.
    // Safer: strstr(txt, "  ") + 2.
    
    const char * filename = strstr(txt, "  ");
    if (!filename) {
        ESP_LOGW("ui_media", "Failed to parse filename from button text");
        return;
    }
    filename += 2; // Skip "  "
    if (*filename) {
        // LVGL filesystem requires 'S:' prefix for stdio (letter code 83)
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "S:/sdcard/%s", filename);
        LV_LOG_USER("Playing file: %s", full_path);
        
        show_play_view(full_path);
    }
}

void populate_sd_files_list(void) {
    if (!cont_sd_files) return;
    
    lv_obj_clean(cont_sd_files);
    
    // Check if SD card is mounted
    if (!sd_card_is_mounted()) {
        lv_obj_t * lbl = lv_label_create(cont_sd_files);
        lv_label_set_text(lbl, LV_SYMBOL_SD_CARD "  SD Card Not Found");
        lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_center(lbl);
        return;
    }

    DIR *d;
    struct dirent *dir;
    struct stat st;
    char path[512];

    d = opendir("/sdcard");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) { // Regular file
                // Get file size
                snprintf(path, sizeof(path), "/sdcard/%s", dir->d_name);
                size_t file_size = 0;
                if (stat(path, &st) == 0) {
                    file_size = st.st_size;
                }

                lv_obj_t * btn = lv_button_create(cont_sd_files);
                lv_obj_set_width(btn, LV_PCT(100));
                lv_obj_set_height(btn, LV_SIZE_CONTENT);
                lv_obj_add_event_cb(btn, file_btn_event_handler, LV_EVENT_CLICKED, NULL);

                // Style: List item style (Transparent, Left aligned, No border)
                lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_radius(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_all(btn, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

                // Pressed style: Dark Grey background
                lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
                lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN | LV_STATE_PRESSED);
                lv_obj_set_style_text_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_PRESSED);

                // File Name Label (Left)
                lv_obj_t * lbl_name = lv_label_create(btn);
                lv_label_set_text_fmt(lbl_name, "%s  %s", LV_SYMBOL_FILE, dir->d_name);
                lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_24, 0);
                lv_obj_align(lbl_name, LV_ALIGN_LEFT_MID, 0, 0);

                // File Size Label (Right)
                lv_obj_t * lbl_size = lv_label_create(btn);
                lv_obj_set_style_text_font(lbl_size, &lv_font_montserrat_24, 0);
                if (file_size < 1024) {
                    lv_label_set_text_fmt(lbl_size, "%lu B", (unsigned long)file_size);
                } else if (file_size < 1024 * 1024) {
                    unsigned long k = (unsigned long)file_size / 1024;
                    unsigned long d = ((unsigned long)file_size * 10 / 1024) % 10;
                    lv_label_set_text_fmt(lbl_size, "%lu.%lu KB", k, d);
                } else {
                    unsigned long m = (unsigned long)file_size / (1024 * 1024);
                    unsigned long d = ((unsigned long)file_size * 10 / (1024 * 1024)) % 10;
                    lv_label_set_text_fmt(lbl_size, "%lu.%lu MB", m, d);
                }
                lv_obj_align(lbl_size, LV_ALIGN_RIGHT_MID, 0, 0);
            }
        }
        closedir(d);
    } else {
        lv_obj_t * lbl = lv_label_create(cont_sd_files);
        lv_label_set_text(lbl, "Failed to open directory");
        lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_RED), 0);
    }
}

void show_media_view(lv_event_t * e) {
    (void)e;  // Unused
    clear_current_view();
    ui_media_create(lv_screen_active());
    
    populate_sd_files_list();
}

void ui_media_create(lv_obj_t * parent) {
    // --- Media Container (SD + Display) ---
    media_cont = lv_obj_create(parent);
    lv_obj_set_size(media_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(media_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(media_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(media_cont, 0, 0);
    lv_obj_set_style_pad_all(media_cont, 10, 0);
    lv_obj_set_flex_flow(media_cont, LV_FLEX_FLOW_ROW);
    lv_obj_add_flag(media_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(media_cont, media_swipe_event_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t * img_swipe = lv_image_create(media_cont);
    lv_image_set_src(img_swipe, &swipeL34);
    lv_obj_add_flag(img_swipe, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(img_swipe, LV_ALIGN_TOP_RIGHT, 0, 15);

    lv_obj_t * lbl_sd_title = lv_label_create(media_cont);
    lv_label_set_text(lbl_sd_title, "SD Card");
    lv_obj_set_style_text_color(lbl_sd_title, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(lbl_sd_title, &lv_font_montserrat_28, 0);
    lv_obj_add_flag(lbl_sd_title, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(lbl_sd_title, LV_ALIGN_TOP_MID, 0, 15);

    // SD Card Panel (Left)
    lv_obj_t * sd_panel = lv_obj_create(media_cont);
    lv_obj_set_height(sd_panel, LV_PCT(100));
    lv_obj_set_width(sd_panel, LV_PCT(100));
    lv_obj_add_flag(sd_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(sd_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sd_panel, 1, 0);
    lv_obj_set_style_border_color(sd_panel, lv_color_hex(0x404040), 0);
    lv_obj_set_flex_flow(sd_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sd_panel, 10, 0);
    lv_obj_set_style_margin_top(sd_panel, 70, 0);
    lv_obj_add_event_cb(sd_panel, media_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(sd_panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    cont_sd_files = lv_obj_create(sd_panel);
    lv_obj_set_width(cont_sd_files, LV_PCT(100));
    lv_obj_set_height(cont_sd_files, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_sd_files, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_sd_files, 0, 0);
    lv_obj_set_style_pad_all(cont_sd_files, 0, 0);
    lv_obj_set_flex_flow(cont_sd_files, LV_FLEX_FLOW_COLUMN);

    lbl_sd = lv_label_create(cont_sd_files);
    lv_label_set_text(lbl_sd, "SD Card:\n--");
    lv_obj_set_style_text_color(lbl_sd, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_sd, &lv_font_montserrat_22, 0);
}

void show_display_view(lv_event_t * e) {
    (void)e;  // Unused
    clear_current_view();
    ui_display_create(lv_screen_active());

    // Update resolution text dynamically
    if (lv_display_get_default() && lbl_disp_info) {
        int32_t w = lv_display_get_horizontal_resolution(lv_display_get_default());
        int32_t h = lv_display_get_vertical_resolution(lv_display_get_default());
        lv_label_set_text_fmt(lbl_disp_info, "Driver Resolution: 450x600\nActual Pixel Resolution: %" LV_PRId32 "x%" LV_PRId32 "\nDriver: RM690B0\nInterface: QSPI", w, h);
    }
}

void ui_display_create(lv_obj_t * parent) {
    display_cont = lv_obj_create(parent);
    lv_obj_set_size(display_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(display_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(display_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(display_cont, 0, 0);
    lv_obj_set_style_pad_all(display_cont, 10, 0);
    lv_obj_set_flex_flow(display_cont, LV_FLEX_FLOW_ROW);
    lv_obj_add_flag(display_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(display_cont, media_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    
    lv_obj_t * img_swipe = lv_image_create(display_cont);
    lv_image_set_src(img_swipe, &swipeL34);
    lv_obj_add_flag(img_swipe, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(img_swipe, LV_ALIGN_TOP_RIGHT, 0, 15);

    lv_obj_t * lbl_disp_title = lv_label_create(display_cont);
    lv_label_set_text(lbl_disp_title, "Display Information");
    lv_obj_set_style_text_color(lbl_disp_title, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(lbl_disp_title, &lv_font_montserrat_28, 0);
    lv_obj_add_flag(lbl_disp_title, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(lbl_disp_title, LV_ALIGN_TOP_MID, 0, 15);

    // Inner Panel
    lv_obj_t * display_panel = lv_obj_create(display_cont);
    lv_obj_set_size(display_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(display_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(display_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(display_panel, 1, 0);
    lv_obj_set_style_border_color(display_panel, lv_color_hex(0x404040), 0);
    lv_obj_set_flex_flow(display_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(display_panel, 10, 0);
    lv_obj_set_style_margin_top(display_panel, 70, 0);
    lv_obj_add_event_cb(display_panel, media_swipe_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(display_panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    cont_display_info = lv_obj_create(display_panel);
    lv_obj_set_width(cont_display_info, LV_PCT(100));
    lv_obj_set_height(cont_display_info, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_display_info, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_display_info, 0, 0);
    lv_obj_set_style_pad_all(cont_display_info, 0, 0);
    lv_obj_set_flex_flow(cont_display_info, LV_FLEX_FLOW_COLUMN);
    
    // Rotation Dropdown
    lv_obj_t * rotation_cont = lv_obj_create(cont_display_info);
    lv_obj_set_width(rotation_cont, LV_PCT(100));
    lv_obj_set_height(rotation_cont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rotation_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rotation_cont, 0, 0);
    lv_obj_set_style_pad_all(rotation_cont, 5, 0);
    lv_obj_set_style_margin_top(rotation_cont, 20, 0);
    lv_obj_set_flex_flow(rotation_cont, LV_FLEX_FLOW_ROW); // Row layout for label + dropdown
    lv_obj_set_flex_align(rotation_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * rotation_label = lv_label_create(rotation_cont);
    lv_label_set_text(rotation_label, "Rotation:");
    lv_obj_set_style_text_color(rotation_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(rotation_label, &lv_font_montserrat_22, 0);

    lv_obj_t * rotation_dropdown = lv_dropdown_create(rotation_cont);
    lv_dropdown_set_options(rotation_dropdown, "0° USB Bottom\n90° USB Right\n180° USB Top\n270° USB Left");
    lv_obj_set_width(rotation_dropdown, LV_PCT(50)); // Limit to half screen width
    lv_obj_set_style_text_font(rotation_dropdown, &lv_font_montserrat_22, 0); // Larger font on main box
    
    // Style: Black background, Silver text/arrow
    lv_obj_set_style_bg_color(rotation_dropdown, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_color(rotation_dropdown, lv_color_hex(0xC0C0C0), LV_PART_MAIN);
    lv_obj_set_style_text_color(rotation_dropdown, lv_color_hex(0xC0C0C0), LV_PART_INDICATOR);
    
    // Style List
    lv_obj_t * list = lv_dropdown_get_list(rotation_dropdown);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_22, 0); // Larger font on list
    lv_obj_set_style_bg_color(list, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_color(list, lv_color_hex(0xC0C0C0), LV_PART_MAIN);
    lv_obj_set_style_border_color(list, lv_color_hex(0x808080), LV_PART_MAIN); // Gray border to see it
    
    // Set current rotation from hardware
    rm690b0_rotation_t current_rot = hal_mgr_get_rotation();
    lv_dropdown_set_selected(rotation_dropdown, (uint16_t)current_rot);
    
    lv_obj_add_event_cb(rotation_dropdown, rotation_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Brightness Slider
    lv_obj_t * brightness_cont = lv_obj_create(cont_display_info);
    lv_obj_set_width(brightness_cont, LV_PCT(100));
    lv_obj_set_height(brightness_cont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(brightness_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(brightness_cont, 0, 0);
    lv_obj_set_style_pad_all(brightness_cont, 5, 0);
    lv_obj_set_style_margin_top(brightness_cont, 20, 0);
    lv_obj_set_flex_flow(brightness_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(brightness_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Initialize brightness from NVS on first use
    init_brightness();

    lv_obj_t * brightness_label = lv_label_create(brightness_cont);
    lv_label_set_text_fmt(brightness_label, "Brightness: %d", s_current_brightness);
    lv_obj_set_style_text_color(brightness_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_22, 0);

    brightness_slider = lv_slider_create(brightness_cont);
    lv_obj_set_width(brightness_slider, LV_PCT(50));
    lv_obj_set_height(brightness_slider, 20); // Matches Driver Test Switch height
    lv_slider_set_range(brightness_slider, 10, 255);
    lv_slider_set_value(brightness_slider, s_current_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, brightness_label);

    // Driver Test Toggle
    lv_obj_t * rainbow_cont = lv_obj_create(cont_display_info);
    lv_obj_set_width(rainbow_cont, LV_PCT(100));
    lv_obj_set_height(rainbow_cont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rainbow_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rainbow_cont, 0, 0);
    lv_obj_set_style_pad_all(rainbow_cont, 5, 0);
    lv_obj_set_style_margin_top(rainbow_cont, 20, 0);
    lv_obj_set_flex_flow(rainbow_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rainbow_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * rainbow_label = lv_label_create(rainbow_cont);
    lv_label_set_text(rainbow_label, "Driver Test Pattern 2 Seconds");
    lv_obj_set_style_text_color(rainbow_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(rainbow_label, &lv_font_montserrat_18, 0);

    lv_obj_t * rainbow_switch = lv_switch_create(rainbow_cont);
    lv_obj_set_size(rainbow_switch, 80, 40); // Make switch twice as big (default is ~40x20)
    lv_obj_add_event_cb(rainbow_switch, rainbow_test_cb, LV_EVENT_VALUE_CHANGED, NULL);
}


void show_play_view(const char * path) {
    clear_current_view();
    ui_play_create(lv_screen_active(), path);
}

void ui_play_create(lv_obj_t * parent, const char * file_path) {
    play_cont = lv_obj_create(parent);
    lv_obj_set_size(play_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(play_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(play_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(play_cont, 0, 0);
    lv_obj_set_style_pad_all(play_cont, 0, 0);
    // Event callback added later to capture AVI object
    lv_obj_clear_flag(play_cont, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(play_cont, LV_OBJ_FLAG_CLICKABLE);

    // Swipe Right Icon to indicate return
    lv_obj_t * img_swipe = lv_image_create(play_cont);
    lv_image_set_src(img_swipe, &swipeR34);
    lv_obj_add_flag(img_swipe, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(img_swipe, LV_ALIGN_TOP_LEFT, 0, 15);

    // Title (Filename)
    lv_obj_t * title = lv_label_create(play_cont);
    // Extract filename from path
    const char * fname = NULL; 
    if(file_path) {
        // Find last slash or colon (for S:file.avi)
        const char * slash = strrchr(file_path, '/');
        const char * colon = strrchr(file_path, ':');
        
        if (slash && colon) {
            fname = (slash > colon) ? slash + 1 : colon + 1;
        } else if (slash) {
            fname = slash + 1;
        } else if (colon) {
            fname = colon + 1;
        } else {
            fname = file_path;
        }
    } else {
        fname = "Unknown";
    }

    lv_label_set_text(title, fname);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFD700), 0);

    // Create media container (left 60%) - starts below title
    lv_obj_t * media_cont = lv_obj_create(play_cont);
    lv_obj_set_size(media_cont, LV_PCT(60), LV_PCT(85));
    lv_obj_align(media_cont, LV_ALIGN_TOP_LEFT, 0, 50);
    lv_obj_set_style_bg_opa(media_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(media_cont, 0, 0);
    lv_obj_set_style_pad_all(media_cont, 0, 0);
    
    // Create info panel (right 40%) - starts at same Y as media
    lv_obj_t * info_cont = lv_obj_create(play_cont);
    lv_obj_set_size(info_cont, LV_PCT(38), LV_PCT(85));
    lv_obj_align(info_cont, LV_ALIGN_TOP_RIGHT, 0, 50);
    lv_obj_set_style_bg_color(info_cont, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(info_cont, LV_OPA_80, 0);
    lv_obj_set_style_border_width(info_cont, 1, 0);
    lv_obj_set_style_border_color(info_cont, lv_color_hex(0x444444), 0);
    lv_obj_set_style_pad_all(info_cont, 10, 0);
    lv_obj_set_flex_flow(info_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(info_cont, 8, 0);
    
    // Info panel title
    lv_obj_t * info_title = lv_label_create(info_cont);
    lv_label_set_text(info_title, "File Information");
    lv_obj_set_style_text_font(info_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(info_title, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_pad_bottom(info_title, 10, 0);
    lv_obj_set_style_text_align(info_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(info_title, LV_PCT(100));

    // Generic Image Display (AVI/JPG)
    lv_obj_t * img = NULL;
    
    if(file_path) {
        ESP_LOGI("ui_media", "Opening media file: %s", file_path);
        
        // Get file size
        const char* fs_path = file_path + 2; // Skip "S:" prefix
        struct stat st;
        long file_size = 0;
        if (stat(fs_path, &st) == 0) {
            file_size = st.st_size;
        }
        
        // Simple extension check
        const char * ext = strrchr(file_path, '.');
        int is_avi = 0;
        
        if(ext) {
            if(strcasecmp(ext, ".avi") == 0) is_avi = 1;
        }

        if(is_avi) {
            // New AVI (MJPEG) handling
            ESP_LOGI("ui_media", "Creating AVI player for %s", file_path);
            img = ui_avi_create(media_cont);
            if (img) {
                ui_avi_set_src(img, file_path);
                
                // Extract AVI metadata (use fs_path without "S:" prefix)
                avi_metadata_t avi_meta = extract_avi_metadata(fs_path);
                
                ESP_LOGI("ui_media", "AVI metadata: valid=%d, fps=%lu", avi_meta.valid, (unsigned long)avi_meta.frame_rate);
                
                // Display AVI info
                lv_obj_t * lbl_type = lv_label_create(info_cont);
                lv_label_set_text(lbl_type, "Type: AVI Video");
                lv_obj_set_style_text_font(lbl_type, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(lbl_type, lv_color_white(), 0);
                lv_obj_set_size(lbl_type, LV_PCT(100), LV_SIZE_CONTENT);
                
                lv_obj_t * lbl_size = lv_label_create(info_cont);
                lv_obj_set_size(lbl_size, LV_PCT(100), LV_SIZE_CONTENT);
                if (file_size < 1024) {
                    lv_label_set_text_fmt(lbl_size, "Size: %ld B", file_size);
                } else if (file_size < 1024 * 1024) {
                     lv_label_set_text_fmt(lbl_size, "Size: %ld.%ld KB", file_size / 1024, ((file_size % 1024) * 10) / 1024);
                } else {
                    lv_label_set_text_fmt(lbl_size, "Size: %ld.%ld MB", file_size / (1024 * 1024), ((file_size % (1024 * 1024)) * 10) / (1024 * 1024));
                }
                lv_obj_set_style_text_font(lbl_size, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(lbl_size, lv_color_white(), 0);
                
                if (avi_meta.valid && avi_meta.frame_rate > 0) {
                    lv_obj_t * lbl_fps = lv_label_create(info_cont);
                    lv_label_set_text_fmt(lbl_fps, "Frame Rate: ~%lu fps", (unsigned long)avi_meta.frame_rate);
                    lv_obj_set_style_text_font(lbl_fps, &lv_font_montserrat_14, 0);
                    lv_obj_set_style_text_color(lbl_fps, lv_color_white(), 0);
                    lv_obj_set_size(lbl_fps, LV_PCT(100), LV_SIZE_CONTENT);
                }
                
                lv_obj_t * lbl_codec = lv_label_create(info_cont);
                lv_label_set_text(lbl_codec, "Codec: MJPEG");
                lv_obj_set_style_text_font(lbl_codec, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(lbl_codec, lv_color_white(), 0);
                lv_obj_set_size(lbl_codec, LV_PCT(100), LV_SIZE_CONTENT);
            } else {
                ESP_LOGE("ui_media", "Failed to create AVI object");
            }
        } else {
            // JPG files handled by LVGL's libjpeg-turbo decoder
            ESP_LOGI("ui_media", "Creating image viewer for %s", file_path);
            
            img = lv_image_create(media_cont);
            if (img) {
                lv_obj_set_size(img, LV_PCT(100), LV_PCT(100));
                lv_image_set_src(img, file_path);
                
                // Extract JPG metadata (use fs_path without "S:" prefix)
                jpg_metadata_t jpg_meta = extract_jpg_metadata(fs_path);
                
                ESP_LOGI("ui_media", "JPG metadata: valid=%d, %lux%lu", jpg_meta.valid, 
                    (unsigned long)jpg_meta.width, (unsigned long)jpg_meta.height);
                
                // Display JPG info
                lv_obj_t * lbl_type = lv_label_create(info_cont);
                lv_label_set_text(lbl_type, "Type: JPEG Image");
                lv_obj_set_style_text_font(lbl_type, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(lbl_type, lv_color_white(), 0);
                lv_obj_set_size(lbl_type, LV_PCT(100), LV_SIZE_CONTENT);
                ESP_LOGI("ui_media", "Created JPG type label");
                
                lv_obj_t * lbl_size = lv_label_create(info_cont);
                lv_obj_set_size(lbl_size, LV_PCT(100), LV_SIZE_CONTENT);
                if (file_size < 1024) {
                    lv_label_set_text_fmt(lbl_size, "Size: %ld B", file_size);
                } else if (file_size < 1024 * 1024) {
                     lv_label_set_text_fmt(lbl_size, "Size: %ld.%ld KB", file_size / 1024, ((file_size % 1024) * 10) / 1024);
                } else {
                    lv_label_set_text_fmt(lbl_size, "Size: %ld.%ld MB", file_size / (1024 * 1024), ((file_size % (1024 * 1024)) * 10) / (1024 * 1024));
                }
                lv_obj_set_style_text_font(lbl_size, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(lbl_size, lv_color_white(), 0);
                ESP_LOGI("ui_media", "Created JPG size label: %ld bytes", file_size);
                
                if (jpg_meta.valid && jpg_meta.width > 0 && jpg_meta.height > 0) {
                    lv_obj_t * lbl_dim = lv_label_create(info_cont);
                    lv_label_set_text_fmt(lbl_dim, "Dimensions: %lux%lu", 
                        (unsigned long)jpg_meta.width, (unsigned long)jpg_meta.height);
                    lv_obj_set_style_text_font(lbl_dim, &lv_font_montserrat_14, 0);
                    lv_obj_set_style_text_color(lbl_dim, lv_color_white(), 0);
                    lv_obj_set_size(lbl_dim, LV_PCT(100), LV_SIZE_CONTENT);
                }
                
                lv_obj_t * lbl_fmt = lv_label_create(info_cont);
                lv_label_set_text(lbl_fmt, "Format: RGB565");
                lv_obj_set_style_text_font(lbl_fmt, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(lbl_fmt, lv_color_white(), 0);
                lv_obj_set_size(lbl_fmt, LV_PCT(100), LV_SIZE_CONTENT);
                ESP_LOGI("ui_media", "Created JPG format label");
            } else {
                ESP_LOGE("ui_media", "Failed to create image object");
            }
        }

        if(img) {
            lv_obj_center(img);
            // Clean up gesture bubble
            lv_obj_clear_flag(img, LV_OBJ_FLAG_GESTURE_BUBBLE);
            // Add event handler for press/release to pause/resume playback
            lv_obj_add_event_cb(img, play_event_cb, LV_EVENT_ALL, img);
        }
        
        // Add event to container too, passing the image object (which might be AVI)
        // This ensures touching the background also pauses playback
        lv_obj_add_event_cb(play_cont, play_event_cb, LV_EVENT_ALL, img);
    }
}
