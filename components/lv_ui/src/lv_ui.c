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

lv_obj_t * cont_sd_files = NULL;
lv_obj_t * cont_pmic_details = NULL;
lv_obj_t * cont_settings_list = NULL;
lv_obj_t * cont_display_info = NULL;

void lv_ui_init(void) {
    ESP_LOGI(TAG, "Initializing UI...");
    
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0); // Dark Grey

    // Create Screens/Containers
    ui_home_create(scr);
    ui_system_create(scr);
    ui_media_create(scr);

    // Start Stats Timer
    lv_timer_create(update_stats_timer_cb, 500, NULL);
    
    // Show Home by default
    show_home_view(NULL);
    
    ESP_LOGI(TAG, "UI Initialized");
}
