#ifndef UI_AVI_H
#define UI_AVI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * Create an AVI player object
 * @param parent    pointer to an object, it will be the parent of the new player
 * @return          pointer to the new player object
 */
lv_obj_t * ui_avi_create(lv_obj_t * parent);

/**
 * Set the source of the AVI player
 * @param obj       pointer to the AVI player object
 * @param src       path to the AVI file (e.g. "S:/sdcard/video.avi")
 */
void ui_avi_set_src(lv_obj_t * obj, const char * src);

/**
 * Play/Resume the AVI
 * @param obj       pointer to the AVI player object
 */
void ui_avi_play(lv_obj_t * obj);

/**
 * Pause the AVI
 * @param obj       pointer to the AVI player object
 */
void ui_avi_pause(lv_obj_t * obj);

/**
 * Stop the AVI and reset to start
 * @param obj       pointer to the AVI player object
 */
void ui_avi_stop(lv_obj_t * obj);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*UI_AVI_H*/
