#pragma once
#include "lvgl.h"

// Global Containers
extern lv_obj_t * home_cont;
extern lv_obj_t * system_cont;
extern lv_obj_t * media_cont;

// Global Labels (for updates)
extern lv_obj_t * lbl_batt;
extern lv_obj_t * lbl_chg_stat;
extern lv_obj_t * lbl_chg_curr;
extern lv_obj_t * lbl_usb;
extern lv_obj_t * lbl_usb_volts;
extern lv_obj_t * lbl_usb_pg;
extern lv_obj_t * lbl_sys_volts;
extern lv_obj_t * lbl_ntc;
extern lv_obj_t * lbl_sd;
extern lv_obj_t * lbl_disp_info;

// Other containers needed for updates
extern lv_obj_t * cont_sd_files;
extern lv_obj_t * cont_pmic_details;
extern lv_obj_t * cont_settings_list;
extern lv_obj_t * cont_display_info;

// Functions
void ui_home_create(lv_obj_t * parent);
void ui_system_create(lv_obj_t * parent);
void ui_media_create(lv_obj_t * parent);

void show_home_view(lv_event_t * e);
void show_system_view(lv_event_t * e);
void show_media_view(lv_event_t * e);

void populate_sd_files_list(void);
void update_stats_timer_cb(lv_timer_t * timer);
