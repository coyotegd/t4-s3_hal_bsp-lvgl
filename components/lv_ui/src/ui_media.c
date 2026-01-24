#include "ui_private.h"
#include <dirent.h>
#include <sys/stat.h>
#include "sd_card.h" // Assuming this is available via REQUIRES sd_card
#include <stdio.h> // For snprintf
#include <string.h>
#include "esp_log.h"
#include "ui_avi.h"

LV_IMG_DECLARE(swipeL34);
LV_IMG_DECLARE(swipeR34);

static void media_swipe_event_cb(lv_event_t * e) {
    if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) {
        show_home_view(NULL);
    }
}

static void play_swipe_event_cb(lv_event_t * e) {
    if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        show_media_view(e);
    }
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
        
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "S:/sdcard/%s", filename); // LVGL drive prefix 'S:' might be needed or just /sdcard depending on fs
        // In previous code populate used /sdcard/name for stat, but LVGL file system might need drive letter if registered.
        // Assuming standard POSIX if no LVGL driver used, but lv_gif uses LVGL file system abstraction?
        // Actually lv_gif uses lv_image_decoder which uses lv_fs. 
        // If LVGL fs is not set up for /sdcard, we might have issues. 
        // However, if we assume standard stdio is hooked or similar.
        // Let's use the path as constructed in populate: "/sdcard/%s".
        // If LVGL is configured to use stdio (CONFIG_LV_USE_FS_STDIO), we might need to prefix "A:" or similar?
        // Use LVGL path with Drive letter 'S:' mapping to stdio (fopen)
        // This is required for lv_image/tjpgd
        snprintf(full_path, sizeof(full_path), "S:%s", filename);
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
                lv_obj_align(lbl_name, LV_ALIGN_LEFT_MID, 0, 0);

                // File Size Label (Right)
                lv_obj_t * lbl_size = lv_label_create(btn);
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
        lv_label_set_text_fmt(lbl_disp_info, "Display Info:\nResolution: %" LV_PRId32 "x%" LV_PRId32 "\nDriver: RM690B0\nInterface: QSPI", w, h);
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
    lv_label_set_text(lbl_disp_title, "Display Info");
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

    lbl_disp_info = lv_label_create(cont_display_info);
    lv_label_set_text(lbl_disp_info, "Display Info:\nResolution: --x--\nDriver: RM690B0\nInterface: QSPI");
    lv_obj_set_style_text_color(lbl_disp_info, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_disp_info, &lv_font_montserrat_22, 0);
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
    lv_obj_add_event_cb(play_cont, play_swipe_event_cb, LV_EVENT_GESTURE, NULL);
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

    // Generic Image Display (GIF/AVI/JPG)
    if(file_path) {
        ESP_LOGI("ui_media", "Opening media file: %s", file_path);

        lv_obj_t * img = NULL;
        
        // Simple extension check
        const char * ext = strrchr(file_path, '.');
        int is_avi = 0;
        
        if(ext) {
            if(strcasecmp(ext, ".avi") == 0) is_avi = 1;
        }

        if(is_avi) {
            // New AVI (MJPEG) handling
            ESP_LOGI("ui_media", "Creating AVI player for %s", file_path);
            img = ui_avi_create(play_cont);
            if (img) {
                ui_avi_set_src(img, file_path);
            } else {
                ESP_LOGE("ui_media", "Failed to create AVI object");
            }
        } else {
            // Assume JPG or others handled by lv_image
            img = lv_image_create(play_cont);
            lv_image_set_src(img, file_path);
        }

        if(img) {
            lv_obj_center(img);
            // Clean up gesture bubble
            lv_obj_clear_flag(img, LV_OBJ_FLAG_GESTURE_BUBBLE);
            lv_obj_add_event_cb(img, play_swipe_event_cb, LV_EVENT_GESTURE, NULL);
        }
    }
}
