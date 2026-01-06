#include "ui_private.h"

LV_IMG_DECLARE(img_watermelon);
LV_IMG_DECLARE(img_venezuela);

void show_home_view(lv_event_t * e) {
    clear_current_view();
    ui_home_create(lv_screen_active());
}

static void delayed_switch_timer_cb(lv_timer_t * timer) {
    lv_event_cb_t target_cb = (lv_event_cb_t)lv_timer_get_user_data(timer);
    if(target_cb) {
        target_cb(NULL);
    }
}

static void delayed_switch_event_handler(lv_event_t * e) {
    lv_event_cb_t target_cb = (lv_event_cb_t)lv_event_get_user_data(e);
    lv_timer_t * timer = lv_timer_create(delayed_switch_timer_cb, 150, (void*)target_cb);
    lv_timer_set_repeat_count(timer, 1);
}

static void create_neon_btn(lv_obj_t * parent, const char * icon, const char * text, lv_color_t color, lv_event_cb_t event_cb) {
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_set_height(btn, 100);
    lv_obj_set_width(btn, LV_PCT(30));
    lv_obj_add_event_cb(btn, delayed_switch_event_handler, LV_EVENT_CLICKED, (void*)event_cb);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn, 5, 0);
    lv_obj_set_style_pad_gap(btn, 5, 0);

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
    
    // 1. Title Container (Top)
    lv_obj_t * title_cont = lv_obj_create(home_cont);
    lv_obj_set_size(title_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(title_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_cont, 0, 0);
    lv_obj_set_flex_flow(title_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(title_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(title_cont, 10, 0);

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

    // 2. Button Container (Row 1)
    lv_obj_t * btn_row1 = lv_obj_create(home_cont);
    lv_obj_set_width(btn_row1, LV_PCT(100));
    lv_obj_set_height(btn_row1, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row1, 0, 0);
    lv_obj_set_flex_flow(btn_row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row1, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_row1, 10, 0);

    // Button 1: PM Status (Neon Red/Orange)
    create_neon_btn(btn_row1, LV_SYMBOL_CHARGE, "PM Status", lv_color_hex(0xFF3300), show_pmic_view);

    // Button 2: Set PM (Neon Blue)
    create_neon_btn(btn_row1, LV_SYMBOL_SETTINGS, "Set PM", lv_color_hex(0x007FFF), show_settings_view);

    // Button 3: SD Card (Neon Cyan)
    create_neon_btn(btn_row1, LV_SYMBOL_SD_CARD, "SD Card", lv_color_hex(0x00FFFF), show_media_view);

    // 3. Button Container (Row 2)
    lv_obj_t * btn_row2 = lv_obj_create(home_cont);
    lv_obj_set_width(btn_row2, LV_PCT(100));
    lv_obj_set_height(btn_row2, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row2, 0, 0);
    lv_obj_set_flex_flow(btn_row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_row2, 10, 0);

    // Watermelon Image
    lv_obj_t * icon_watermelon = lv_image_create(btn_row2);
    lv_image_set_src(icon_watermelon, &img_watermelon);
    lv_obj_set_style_align(icon_watermelon, LV_ALIGN_CENTER, 0);

    // Display (Neon Green)
    create_neon_btn(btn_row2, LV_SYMBOL_EYE_OPEN, "Display", lv_color_hex(0x39FF14), show_display_view);

    // Button 5: System (Neon Purple)
    create_neon_btn(btn_row2, LV_SYMBOL_FILE, "System", lv_color_hex(0x9D00FF), show_sys_info_view);

    // Venezuela Flag Image
    lv_obj_t * icon_venezuela = lv_image_create(btn_row2);
    lv_image_set_src(icon_venezuela, &img_venezuela);
    lv_obj_set_style_align(icon_venezuela, LV_ALIGN_CENTER, 0);
}
