idf_component_register(
    SRCS "src/lvgl_mgr.c" "src/avi_mjpg_mgr.c"
    INCLUDE_DIRS "include"
    REQUIRES t4s3_hal lvgl esp_timer driver vfs esp_lcd fatfs wear_levelling sdmmc espressif__libjpeg-turbo
)

function(_t4s3_bsp_lvgl_libjpeg_glue)
    if(TARGET idf::espressif__libjpeg-turbo AND NOT TARGET idf::libjpeg-turbo)
        add_library(_t4s3_libjpeg_turbo_compat INTERFACE)
        add_library(idf::libjpeg-turbo ALIAS _t4s3_libjpeg_turbo_compat)

        idf_build_get_property(_t4s3_idf_includes INCLUDE_DIRECTORIES)
        if(_t4s3_idf_includes)
            target_include_directories(_t4s3_libjpeg_turbo_compat INTERFACE ${_t4s3_idf_includes})
        endif()

        target_link_libraries(_t4s3_libjpeg_turbo_compat INTERFACE idf::espressif__libjpeg-turbo)
    endif()

    # LVGL's libjpeg-turbo wrapper is built inside the managed LVGL component.
    # We can't edit managed component manifests, so express the dependency by
    # linking the LVGL component target against the libjpeg-turbo component.
    if(TARGET idf::espressif__libjpeg-turbo)
        idf_component_get_property(_t4s3_lvgl_component_lib lvgl__lvgl COMPONENT_LIB)
        if(_t4s3_lvgl_component_lib AND TARGET ${_t4s3_lvgl_component_lib})
            target_link_libraries(${_t4s3_lvgl_component_lib} PRIVATE idf::espressif__libjpeg-turbo)

            # LVGL may also include libjpeg-turbo internal headers.
            idf_component_get_property(_t4s3_libjpeg_component_dir espressif__libjpeg-turbo COMPONENT_DIR)
            if(_t4s3_libjpeg_component_dir)
                set(_t4s3_libjpeg_internal_src "${_t4s3_libjpeg_component_dir}/libjpeg-turbo/src")
                if(EXISTS "${_t4s3_libjpeg_internal_src}/jpegint.h")
                    target_include_directories(${_t4s3_lvgl_component_lib} PRIVATE "${_t4s3_libjpeg_internal_src}")
                endif()
            endif()

            set(_t4s3_libjpeg_internal_build "${CMAKE_BINARY_DIR}/esp-idf/espressif__libjpeg-turbo/libjpeg-build")
            if(EXISTS "${_t4s3_libjpeg_internal_build}/jconfigint.h")
                target_include_directories(${_t4s3_lvgl_component_lib} PRIVATE "${_t4s3_libjpeg_internal_build}")
            endif()
        endif()
    endif()
endfunction()

if(COMMAND cmake_language)
    cmake_language(DEFER CALL _t4s3_bsp_lvgl_libjpeg_glue)
else()
    _t4s3_bsp_lvgl_libjpeg_glue()
endif()
