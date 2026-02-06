# Local override for espressif__libjpeg-turbo
# Using standard libjpeg API only (not TurboJPEG)

idf_component_register(
    SRCS "dummy.c"
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}"
)

# Get build properties for flags
idf_build_get_property(idf_include_directories INCLUDE_DIRECTORIES)
set(includes "-I$<JOIN:${idf_include_directories}, -I>")

set(c_flags "${includes} ${extra_defines} ")
set(cxx_flags "${includes} ${extra_defines} ")
set(common_flags "-ggdb -ffunction-sections -fdata-sections -lm")

if(CONFIG_IDF_TARGET_ARCH_XTENSA)
    set(assert_flags "${assert_flags} -mlongcalls")
endif()

# Set optimization flags
if(CONFIG_COMPILER_OPTIMIZATION_DEFAULT)
    set(opt_flags "-Og ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=Release
                 -DCMAKE_C_FLAGS_RELEASE=${opt_flags})
elseif(CONFIG_COMPILER_OPTIMIZATION_SIZE)
    set(opt_flags "-Os ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=MinSizeRel
                 -DCMAKE_C_FLAGS_MINSIZEREL=${opt_flags})
elseif(CONFIG_COMPILER_OPTIMIZATION_PERF)
    set(opt_flags "-O3 ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=Release
                 -DCMAKE_C_FLAGS_RELEASE=${opt_flags})
elseif(COMPILER_OPTIMIZATION_NONE)
    set(opt_flags "-O0 ${common_flags} ${assert_flags}")
    set(opt_args -DCMAKE_BUILD_TYPE=Debug
                 -DCMAKE_C_FLAGS_DEBUG=${opt_flags})
else()
    message(FATAL_ERROR "Unsupported optimization level")
endif()

include(ExternalProject)

# Build jpeg-turbo in this directory:
set(SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo-src)
set(BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-build)

set(lib_path ${BINARY_DIR}/install/lib/libjpeg.a)
add_prebuilt_library(libjpeg-turbo ${lib_path})

# Add jpeg-turbo as a subproject with standard libjpeg only - download from GitHub
ExternalProject_Add(jpegturbo_proj
    GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
    GIT_TAG 3.0.4
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_DIR ${SOURCE_DIR}
    BINARY_DIR ${BINARY_DIR}
    BUILD_BYPRODUCTS ${lib_path}
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    CMAKE_POSITION_INDEPENDENT_CODE ON
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${c_flags}
        -DCMAKE_CXX_FLAGS=${cxx_flags}
        ${opt_args}
        -DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/install
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DWITH_TURBOJPEG=FALSE
        -DWITH_SIMD=FALSE
        -DCMAKE_SYSTEM_PROCESSOR=esp32
        -DWITH_ARITH_DEC=TRUE
        -DWITH_ARITH_ENC=TRUE
        -DWITH_JPEG8=TRUE
        -DWITH_JPEG7=TRUE
        -DENABLE_SHARED=FALSE
        -DENABLE_STATIC=TRUE
        -DENABLE_EXECUTABLES=FALSE
    BUILD_COMMAND ${CMAKE_COMMAND} --build . --target jpeg-static
    INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory ${BINARY_DIR}/install/lib
            COMMAND ${CMAKE_COMMAND} -E make_directory ${BINARY_DIR}/install/include
            COMMAND ${CMAKE_COMMAND} -E copy ${BINARY_DIR}/libjpeg.a ${BINARY_DIR}/install/lib/libjpeg.a
            COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_DIR}/jpeglib.h ${BINARY_DIR}/install/include/
            COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_DIR}/jmorecfg.h ${BINARY_DIR}/install/include/
            COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_DIR}/jerror.h ${BINARY_DIR}/install/include/
            COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_DIR}/jpegint.h ${BINARY_DIR}/install/include/
            COMMAND ${CMAKE_COMMAND} -E copy ${BINARY_DIR}/jconfig.h ${BINARY_DIR}/install/include/
)

# Attach header files to the component library
set_target_properties(${COMPONENT_LIB} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${BINARY_DIR}/install/include)

# Make sure the subproject is built before the component library
add_dependencies(${COMPONENT_LIB} jpegturbo_proj)

# Link the library to the component library
foreach(dep ${deps})
    target_link_libraries(libjpeg-turbo INTERFACE idf::${dep})
endforeach()
target_link_libraries(${COMPONENT_LIB} INTERFACE libjpeg-turbo)

# Link with standard C library
target_link_libraries(${COMPONENT_LIB} INTERFACE c)
