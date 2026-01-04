#include "ui_private.h"

void show_system_view(lv_event_t * e) {
    if(home_cont) lv_obj_add_flag(home_cont, LV_OBJ_FLAG_HIDDEN);
    if(system_cont) lv_obj_remove_flag(system_cont, LV_OBJ_FLAG_HIDDEN);
    if(media_cont) lv_obj_add_flag(media_cont, LV_OBJ_FLAG_HIDDEN);
}

void ui_system_create(lv_obj_t * parent) {
    // --- System Container (PMIC + Settings) ---
    system_cont = lv_obj_create(parent);
    lv_obj_set_size(system_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(system_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(system_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(system_cont, 0, 0);
    lv_obj_set_style_pad_all(system_cont, 10, 0);
    lv_obj_set_flex_flow(system_cont, LV_FLEX_FLOW_ROW);
    lv_obj_add_flag(system_cont, LV_OBJ_FLAG_HIDDEN);

    // PMIC Panel (Left)
    lv_obj_t * stats_cont = lv_obj_create(system_cont);
    lv_obj_set_height(stats_cont, LV_PCT(100));
    lv_obj_set_width(stats_cont, LV_PCT(49));
    lv_obj_add_flag(stats_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(stats_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stats_cont, 1, 0);
    lv_obj_set_style_border_color(stats_cont, lv_color_hex(0x404040), 0);
    lv_obj_set_flex_flow(stats_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(stats_cont, 10, 0);
    lv_obj_set_style_pad_top(stats_cont, 60, 0); // Space for back button

    lv_obj_t * lbl_stats_title = lv_label_create(stats_cont);
    lv_label_set_text(lbl_stats_title, "PMIC");
    lv_obj_set_style_text_color(lbl_stats_title, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(lbl_stats_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(lbl_stats_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_stats_title, LV_PCT(100));

    cont_pmic_details = lv_obj_create(stats_cont);
    lv_obj_set_width(cont_pmic_details, LV_PCT(100));
    lv_obj_set_height(cont_pmic_details, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_pmic_details, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_pmic_details, 0, 0);
    lv_obj_set_style_pad_all(cont_pmic_details, 0, 0);
    lv_obj_set_flex_flow(cont_pmic_details, LV_FLEX_FLOW_COLUMN);

    // Settings Panel (Right)
    lv_obj_t * settings_cont = lv_obj_create(system_cont);
    lv_obj_set_height(settings_cont, LV_PCT(100));
    lv_obj_set_width(settings_cont, LV_PCT(49));
    lv_obj_add_flag(settings_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(settings_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(settings_cont, 1, 0);
    lv_obj_set_style_border_color(settings_cont, lv_color_hex(0x404040), 0);
    lv_obj_set_flex_flow(settings_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(settings_cont, 10, 0);
    lv_obj_set_style_pad_top(settings_cont, 60, 0); // Match left side

    lv_obj_t * lbl_settings = lv_label_create(settings_cont);
    lv_label_set_text(lbl_settings, "Settings");
    lv_obj_set_style_text_color(lbl_settings, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_text_font(lbl_settings, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(lbl_settings, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_settings, LV_PCT(100));

    cont_settings_list = lv_obj_create(settings_cont);
    lv_obj_set_width(cont_settings_list, LV_PCT(100));
    lv_obj_set_height(cont_settings_list, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_settings_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_settings_list, 0, 0);
    lv_obj_set_style_pad_all(cont_settings_list, 0, 0);
    lv_obj_set_flex_flow(cont_settings_list, LV_FLEX_FLOW_COLUMN);

    // Back Button for System (Created LAST to be on top)
    lv_obj_t * btn_back_sys = lv_button_create(system_cont);
    lv_obj_set_size(btn_back_sys, 60, 60);
    lv_obj_add_flag(btn_back_sys, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(btn_back_sys, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(btn_back_sys, show_home_view, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_back_sys = lv_label_create(btn_back_sys);
    lv_label_set_text(lbl_back_sys, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back_sys);

    // Populate Content
    // Add dummy content to demonstrate scrolling if needed
    lv_obj_t * lbl_dummy = lv_label_create(cont_settings_list);
    lv_label_set_text(lbl_dummy, "Option 1\nOption 2\nOption 3\nOption 4\nOption 5\n"
                                 "Option 6\nOption 7\nOption 8\nOption 9\nOption 10\n"
                                 "Option 11\nOption 12\nOption 13\nOption 14\nOption 15\n"
                                 "Option 16\nOption 17\nOption 18\nOption 19\nOption 20");
    lv_obj_set_style_text_color(lbl_dummy, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_dummy, &lv_font_montserrat_22, 0);

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
    lv_label_set_text(lbl_usb_pg, "USB Wattage:\n--");
    lv_obj_set_style_text_color(lbl_usb_pg, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_usb_pg, &lv_font_montserrat_22, 0);

    lbl_ntc = lv_label_create(cont_pmic_details);
    lv_label_set_text(lbl_ntc, "Temperature:\n-- %");
    lv_obj_set_style_text_color(lbl_ntc, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_ntc, &lv_font_montserrat_22, 0);
}
