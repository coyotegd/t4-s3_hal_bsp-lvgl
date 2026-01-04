#include "ui_private.h"

void show_home_view(lv_event_t * e) {
    if(home_cont) lv_obj_remove_flag(home_cont, LV_OBJ_FLAG_HIDDEN);
    if(system_cont) lv_obj_add_flag(system_cont, LV_OBJ_FLAG_HIDDEN);
    if(media_cont) lv_obj_add_flag(media_cont, LV_OBJ_FLAG_HIDDEN);
}

void ui_home_create(lv_obj_t * parent) {
    // --- Home Container ---
    home_cont = lv_obj_create(parent);
    lv_obj_set_size(home_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(home_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(home_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(home_cont, 0, 0);
    lv_obj_set_style_pad_all(home_cont, 0, 0);
    lv_obj_set_style_pad_bottom(home_cont, 10, 0); 
    lv_obj_set_style_pad_left(home_cont, 10, 0);
    lv_obj_set_style_pad_top(home_cont, 10, 0);
    lv_obj_set_style_pad_right(home_cont, 10, 0);
    lv_obj_set_flex_flow(home_cont, LV_FLEX_FLOW_COLUMN);
    
    // Create a container for the header
    lv_obj_t * header_cont = lv_obj_create(home_cont);
    lv_obj_set_size(header_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_cont, 0, 0);
    lv_obj_set_flex_flow(header_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header_cont, 0, 0);
    lv_obj_set_style_margin_top(header_cont, 0, 0);

    // 1. Left Button Wrapper
    lv_obj_t * left_wrapper = lv_obj_create(header_cont);
    lv_obj_set_height(left_wrapper, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(left_wrapper, 1);
    lv_obj_set_style_bg_opa(left_wrapper, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_wrapper, 0, 0);
    lv_obj_remove_flag(left_wrapper, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(left_wrapper, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left_wrapper, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_left = lv_button_create(left_wrapper);
    lv_obj_set_width(btn_left, LV_PCT(90));
    lv_obj_set_height(btn_left, 80);
    lv_obj_remove_flag(btn_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_left, show_system_view, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_btn_left = lv_label_create(btn_left);
    lv_label_set_text(lbl_btn_left, "System Info");
    lv_obj_center(lbl_btn_left);

    // 2. Title Container (Center)
    lv_obj_t * title_cont = lv_obj_create(header_cont);
    lv_obj_set_size(title_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(title_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_cont, 0, 0);
    lv_obj_set_flex_flow(title_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(title_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(title_cont, 0, 0);

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
    lv_obj_set_style_margin_bottom(subtitle, -5, 0); // Pull content below it closer

    // 3. Right Button Wrapper
    lv_obj_t * right_wrapper = lv_obj_create(header_cont);
    lv_obj_set_height(right_wrapper, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(right_wrapper, 1);
    lv_obj_set_style_bg_opa(right_wrapper, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_wrapper, 0, 0);
    lv_obj_remove_flag(right_wrapper, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(right_wrapper, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_wrapper, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * btn_right = lv_button_create(right_wrapper);
    lv_obj_set_width(btn_right, LV_PCT(90));
    lv_obj_set_height(btn_right, 80);
    lv_obj_remove_flag(btn_right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_right, show_media_view, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_btn_right = lv_label_create(btn_right);
    lv_label_set_text(lbl_btn_right, "Media & Display");
    lv_obj_center(lbl_btn_right);
    
    // Border Frame (Matches GIF view style)
    lv_obj_t * frame = lv_obj_create(home_cont);
    lv_obj_set_size(frame, LV_PCT(100), LV_FLEX_GROW);
    lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame, 2, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(0x404040), 0);
    lv_obj_set_style_radius(frame, 10, 0);
}
