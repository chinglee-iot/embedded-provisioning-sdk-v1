add_library(tinycbor
    "${ROOT_DIR}/libraries/3rdparty/tinycbor/src/cborpretty.c"
    "${ROOT_DIR}/libraries/3rdparty/tinycbor/src/cborpretty_stdio.c"
    "${ROOT_DIR}/libraries/3rdparty/tinycbor/src/cborencoder.c"
    "${ROOT_DIR}/libraries/3rdparty/tinycbor/src/cborencoder_close_container_checked.c"
    "${ROOT_DIR}/libraries/3rdparty/tinycbor/src/cborerrorstrings.c"
    "${ROOT_DIR}/libraries/3rdparty/tinycbor/src/cborparser.c"
    "${ROOT_DIR}/libraries/3rdparty/tinycbor/src/cborparser_dup_string.c"
)

set_target_properties(tinycbor PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    POSITION_INDEPENDENT_CODE ON
)

# Use C99 for tinycbor as it is incompatible with C90
if(CMAKE_C_STANDARD LESS 99)
    set_target_properties(tinycbor PROPERTIES C_STANDARD 99)
endif()

target_include_directories(tinycbor PUBLIC
    "${ROOT_DIR}/libraries/3rdparty/tinycbor/src"
)

file(GLOB MBEDTLS_FILES CONFIGURE_DEPENDS "${ROOT_DIR}/libraries/3rdparty/mbedtls/library/*.c")

add_library(mbedtls
    ${MBEDTLS_FILES}
)

set_target_properties(mbedtls PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    POSITION_INDEPENDENT_CODE ON
)

# Use C99 for mbedtls as v2.26.0 is incompatible with C90
if(CMAKE_C_STANDARD LESS 99)
    set_target_properties(mbedtls PROPERTIES C_STANDARD 99)
endif()

target_include_directories(mbedtls
    PUBLIC
        "${ROOT_DIR}/libraries/3rdparty/mbedtls/include"
        "${ROOT_DIR}/platform/posix/include"
)

target_compile_definitions(
    mbedtls
    PUBLIC
        -DMBEDTLS_CONFIG_FILE="mbedtls_config.h"
)

set_source_files_properties(
    ${MBEDTLS_FILES}
    PROPERTIES COMPILE_FLAGS
    "-Wno-pedantic"
)
