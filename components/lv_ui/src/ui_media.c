#include "ui_private.h"
#include <dirent.h>
#include "sd_card.h" // Assuming this is available via REQUIRES sd_card

static void file_btn_event_handler(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * lbl = lv_obj_get_child(btn, 0);
    const char * txt = lv_label_get_text(lbl);
    LV_LOG_USER("Clicked file: %s", txt);
    // TODO: Implement file action (e.g. play GIF)
}

void populate_sd_files_list(void) {
    if (!cont_sd_files) return;
    
    lv_obj_clean(cont_sd_files);

    DIR *d;
    struct dirent *dir;
    d = opendir("/sdcard");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) { // Regular file
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

                lv_obj_t * lbl = lv_label_create(btn);
                lv_label_set_text_fmt(lbl, "%s  %s", LV_SYMBOL_FILE, dir->d_name);
                lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
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
    if(home_cont) lv_obj_add_flag(home_cont, LV_OBJ_FLAG_HIDDEN);
    if(pmic_cont) lv_obj_add_flag(pmic_cont, LV_OBJ_FLAG_HIDDEN);
    if(settings_cont) lv_obj_add_flag(settings_cont, LV_OBJ_FLAG_HIDDEN);
    if(sys_info_cont) lv_obj_add_flag(sys_info_cont, LV_OBJ_FLAG_HIDDEN);
    if(media_cont) lv_obj_remove_flag(media_cont, LV_OBJ_FLAG_HIDDEN);
    
    populate_sd_files_list();

    // Update resolution text dynamically
    if (lv_display_get_default() && lbl_disp_info) {
        int32_t w = lv_display_get_horizontal_resolution(lv_display_get_default());
        int32_t h = lv_display_get_vertical_resolution(lv_display_get_default());
        lv_label_set_text_fmt(lbl_disp_info, "Display Info:\nResolution: %" LV_PRId32 "x%" LV_PRId32 "\nDriver: RM690B0\nInterface: QSPI", w, h);
    }
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
    lv_obj_add_flag(media_cont, LV_OBJ_FLAG_HIDDEN);

    // SD Card Panel (Left)
    lv_obj_t * sd_panel = lv_obj_create(media_cont);
    lv_obj_set_height(sd_panel, LV_PCT(100));
    lv_obj_set_width(sd_panel, LV_PCT(49));
    lv_obj_add_flag(sd_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(sd_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sd_panel, 1, 0);
    lv_obj_set_style_border_color(sd_panel, lv_color_hex(0x404040), 0);
    lv_obj_set_flex_flow(sd_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sd_panel, 10, 0);
    lv_obj_set_style_pad_top(sd_panel, 60, 0);

    lv_obj_t * lbl_sd_title = lv_label_create(sd_panel);
    lv_label_set_text(lbl_sd_title, "SD Card");
    lv_obj_set_style_text_color(lbl_sd_title, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(lbl_sd_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(lbl_sd_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_sd_title, LV_PCT(100));

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

    // Display Panel (Right)
    lv_obj_t * display_panel = lv_obj_create(media_cont);
    lv_obj_set_height(display_panel, LV_PCT(100));
    lv_obj_set_width(display_panel, LV_PCT(49));
    lv_obj_add_flag(display_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(display_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(display_panel, 1, 0);
    lv_obj_set_style_border_color(display_panel, lv_color_hex(0x404040), 0);
    lv_obj_set_flex_flow(display_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(display_panel, 10, 0);
    lv_obj_set_style_pad_top(display_panel, 60, 0);

    lv_obj_t * lbl_disp_title = lv_label_create(display_panel);
    lv_label_set_text(lbl_disp_title, "Display");
    lv_obj_set_style_text_color(lbl_disp_title, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(lbl_disp_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(lbl_disp_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_disp_title, LV_PCT(100));

    cont_display_info = lv_obj_create(display_panel);
    lv_obj_set_width(cont_display_info, LV_PCT(100));
    lv_obj_set_height(cont_display_info, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_display_info, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_display_info, 0, 0);
    lv_obj_set_style_pad_all(cont_display_info, 0, 0);
    lv_obj_set_flex_flow(cont_display_info, LV_FLEX_FLOW_COLUMN);

    // Back Button for Media (Created LAST to be on top)
    lv_obj_t * btn_back_media = lv_button_create(media_cont);
    lv_obj_set_size(btn_back_media, 60, 60);
    lv_obj_add_flag(btn_back_media, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(btn_back_media, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(btn_back_media, show_home_view, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_back_media = lv_label_create(btn_back_media);
    lv_label_set_text(lbl_back_media, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back_media);

    // Add dummy content for Display Info
    lbl_disp_info = lv_label_create(cont_display_info);
    lv_label_set_text(lbl_disp_info, "Display Info:\nResolution: --x--\nDriver: RM690B0\nInterface: QSPI");
    lv_obj_set_style_text_color(lbl_disp_info, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_disp_info, &lv_font_montserrat_22, 0);
}
