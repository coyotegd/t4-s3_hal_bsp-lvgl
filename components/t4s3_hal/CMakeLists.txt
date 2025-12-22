idf_component_register(
    SRCS "hal_mgr.c" "rm690b0.c" "sy6970.c" "cst226se.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos
)
