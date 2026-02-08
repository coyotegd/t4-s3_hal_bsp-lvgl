idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES t4s3_bsp lv_ui
)

function(_t4s3_create_libjpeg_alias)
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
    # We can't edit managed component manifests, so we express the dependency
    # here by linking the LVGL component target against the libjpeg-turbo
    # component target. This propagates include directories so LVGL can find
    # <jpeglib.h> during compilation.
    if(TARGET idf::espressif__libjpeg-turbo)
        idf_component_get_property(_t4s3_lvgl_component_lib lvgl__lvgl COMPONENT_LIB)
        if(_t4s3_lvgl_component_lib AND TARGET ${_t4s3_lvgl_component_lib})
            target_link_libraries(${_t4s3_lvgl_component_lib} PRIVATE idf::espressif__libjpeg-turbo)

            # LVGL's wrapper also includes libjpeg-turbo *internal* headers
            # (e.g. <jpegint.h>), which are not installed as part of the public
            # libjpeg API. Provide include paths for those headers from the
            # managed libjpeg-turbo component.
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
    cmake_language(DEFER CALL _t4s3_create_libjpeg_alias)
else()
    _t4s3_create_libjpeg_alias()
endif()