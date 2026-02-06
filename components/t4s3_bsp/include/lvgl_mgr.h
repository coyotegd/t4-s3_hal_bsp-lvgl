#ifndef LVGL_MGR_H
#define LVGL_MGR_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the BSP (Board Support Package)
 * This will initialize the HAL, setup LVGL, and start the LVGL timer task.
 * @return ESP_OK on success
 */
esp_err_t bsp_init(void);

/**
 * @brief Lock the LVGL mutex for thread-safe operations
 */
void lvgl_mgr_lock(void);

/**
 * @brief Unlock the LVGL mutex
 */
void lvgl_mgr_unlock(void);

#ifdef __cplusplus
}
#endif

#endif // LVGL_MGR_H
