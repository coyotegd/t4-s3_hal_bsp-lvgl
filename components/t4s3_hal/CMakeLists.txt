idf_component_register(
    SRCS "hal_mgr.c"
    INCLUDE_DIRS "."
    REQUIRES rm690b0 cst226se sy6970 freertos espressif__button sd_card nvs_flash
)
