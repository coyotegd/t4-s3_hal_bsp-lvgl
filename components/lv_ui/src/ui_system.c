#include "ui_private.h"

LV_IMG_DECLARE(swipeL34);
LV_IMG_DECLARE(swipeR34);

static void pmic_swipe_event_cb(lv_event_t * e) {
    if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) {
        show_home_view(e);
    }
}

static void settings_swipe_event_cb(lv_event_t * e) {
    if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        show_home_view(e);
    }
}

void show_pmic_view(lv_event_t * e) {
    clear_current_view();
    ui_pmic_create(lv_screen_active());
}

void show_settings_view(lv_event_t * e) {
    clear_current_view();
    ui_settings_create(lv_screen_active());
}

void show_sys_info_view(lv_event_t * e) {
    clear_current_view();
    ui_sys_info_create(lv_screen_active());
}

static void create_back_btn(lv_obj_t * parent) {
    lv_obj_t * btn_back = lv_button_create(parent);
    lv_obj_set_size(btn_back, 60, 60);
    lv_obj_add_flag(btn_back, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(btn_back, show_home_view, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back);
}

void ui_pmic_create(lv_obj_t * parent) {
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
}

void ui_settings_create(lv_obj_t * parent) {
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

    lv_obj_t * lbl_dummy = lv_label_create(cont_settings_list);
    lv_label_set_text(lbl_dummy, "Option 1\nOption 2\nOption 3\nOption 4\nOption 5\n"
                                 "Option 6\nOption 7\nOption 8\nOption 9\nOption 10\n"
                                 "Option 11\nOption 12\nOption 13\nOption 14\nOption 15\n"
                                 "Option 16\nOption 17\nOption 18\nOption 19\nOption 20");
    lv_obj_set_style_text_color(lbl_dummy, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_dummy, &lv_font_montserrat_22, 0);
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
}
