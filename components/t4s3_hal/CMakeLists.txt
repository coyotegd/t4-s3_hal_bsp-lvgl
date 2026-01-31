idf_component_register(
    SRCS "src/hal_mgr.c" "src/wifi_mgr.c" "src/ota_mgr.c"
    INCLUDE_DIRS "include"
    REQUIRES rm690b0 cst226se sy6970 freertos espressif__button sd_card nvs_flash esp_wifi esp_event lwip esp_http_client json esp_https_ota app_update mbedtls
)
