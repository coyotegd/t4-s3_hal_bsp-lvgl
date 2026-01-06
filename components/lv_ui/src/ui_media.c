#include "ui_private.h"
#include <dirent.h>
#include <sys/stat.h>
#include "sd_card.h" // Assuming this is available via REQUIRES sd_card
#include <stdio.h> // For snprintf

LV_IMG_DECLARE(swipeL34);

static void media_swipe_event_cb(lv_event_t * e) {
    if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) {
        show_home_view(e);
    }
}

static void file_btn_event_handler(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    // Child 0 is file name, Child 1 is size (if added correctly)
    lv_obj_t * lbl = lv_obj_get_child(btn, 0); 
    const char * txt = lv_label_get_text(lbl);
    LV_LOG_USER("Clicked file: %s", txt);
    (void)txt; /* Silence warning if logging is disabled */
    // TODO: Implement file action (e.g. play GIF)
}

void populate_sd_files_list(void) {
    if (!cont_sd_files) return;
    
    lv_obj_clean(cont_sd_files);

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

static void create_back_btn_display(lv_obj_t * parent) {
    lv_obj_t * btn_back = lv_button_create(parent);
    lv_obj_set_size(btn_back, 60, 60);
    lv_obj_add_flag(btn_back, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(btn_back, show_home_view, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back);
}

void show_display_view(lv_event_t * e) {
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
