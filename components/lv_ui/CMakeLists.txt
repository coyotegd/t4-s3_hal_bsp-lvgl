idf_component_register(SRCS "src/lv_ui.c" "src/ui_home.c" "src/ui_system.c" "src/ui_media.c" "src/ui_avi.c" "src/ui_helpers.c" "src/ui_network.c" "src/img_watermelon.c" "src/img_venezuela.c" "src/swipeL34.c" "src/swipeR34.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl sy6970 sd_card esp_timer espressif__libjpeg-turbo t4s3_hal nvs_flash t4s3_bsp)
