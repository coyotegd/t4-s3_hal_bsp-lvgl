#include "lv_ui.h"
#include "ui_private.h"
#include "esp_log.h"

static const char *TAG = "lv_ui";

// Global Definitions
lv_obj_t * home_cont = NULL;
lv_obj_t * pmic_cont = NULL;
lv_obj_t * settings_cont = NULL;
lv_obj_t * sys_info_cont = NULL;
lv_obj_t * media_cont = NULL;
lv_obj_t * play_cont = NULL;
lv_obj_t * display_cont = NULL;

lv_obj_t * lbl_batt = NULL;
lv_obj_t * lbl_chg_stat = NULL;
lv_obj_t * lbl_chg_curr = NULL;
lv_obj_t * lbl_usb = NULL;
lv_obj_t * lbl_usb_volts = NULL;
lv_obj_t * lbl_usb_pg = NULL;
lv_obj_t * lbl_sys_volts = NULL;
lv_obj_t * lbl_ntc = NULL;
lv_obj_t * lbl_sd = NULL;
lv_obj_t * lbl_disp_info = NULL;
lv_obj_t * lbl_sys_info = NULL;
lv_obj_t * lbl_fault = NULL;

lv_obj_t * cont_sd_files = NULL;
lv_obj_t * cont_pmic_details = NULL;
lv_obj_t * cont_settings_list = NULL;
lv_obj_t * cont_display_info = NULL;

void clear_current_view(void) {
    if (home_cont) { lv_obj_delete(home_cont); home_cont = NULL; }
    if (pmic_cont) { lv_obj_delete(pmic_cont); pmic_cont = NULL; }
    if (settings_cont) { lv_obj_delete(settings_cont); settings_cont = NULL; }
    if (sys_info_cont) { lv_obj_delete(sys_info_cont); sys_info_cont = NULL; }
    if (media_cont) { lv_obj_delete(media_cont); media_cont = NULL; }
    if (play_cont) { lv_obj_delete(play_cont); play_cont = NULL; }
    if (display_cont) { lv_obj_delete(display_cont); display_cont = NULL; }

    // Clear pointers to avoid invalid access in timer
    lbl_batt = NULL;
    lbl_chg_stat = NULL;
    lbl_chg_curr = NULL;
    lbl_usb = NULL;
    lbl_usb_volts = NULL;
    lbl_usb_pg = NULL;
    lbl_sys_volts = NULL;
    lbl_ntc = NULL;
    lbl_sd = NULL;
    lbl_disp_info = NULL;
    lbl_sys_info = NULL;
    
    cont_sd_files = NULL;
    cont_pmic_details = NULL;
    cont_settings_list = NULL;
    cont_display_info = NULL;
}

void lv_ui_init(void) {
    ESP_LOGI(TAG, "Initializing UI...");
    
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101010), 0); // Darker Grey

    // Create Home Screen initially
    ui_home_create(scr);

    // Start Stats Timer
    lv_timer_create(update_stats_timer_cb, 500, NULL);
    
    ESP_LOGI(TAG, "UI Initialized");
}
