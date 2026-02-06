#ifndef UI_AVI_H
#define UI_AVI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Define the AVI object creation function
lv_obj_t * ui_avi_create(lv_obj_t * parent);

// Control functions
void ui_avi_set_src(lv_obj_t * obj, const char * src);
void ui_avi_play(lv_obj_t * obj);
void ui_avi_pause(lv_obj_t * obj);
void ui_avi_stop(lv_obj_t * obj);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
