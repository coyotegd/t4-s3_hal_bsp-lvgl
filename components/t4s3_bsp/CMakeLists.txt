idf_component_register(
    SRCS "src/lvgl_mgr.c" "src/avi_mjpg_mgr.c"
    INCLUDE_DIRS "include"
    REQUIRES t4s3_hal lvgl esp_timer driver vfs esp_lcd fatfs wear_levelling sdmmc espressif__libjpeg-turbo
)
