set( DEMO_NAME "pal_queue_unit_test" )

# ==============================================================================
# pal sync queue library

set( PAL_SYNC_QUEUE_PATH "${CMAKE_SOURCE_DIR}/platform/posix/pal_queue")

add_library( pal_sync_queue
            "${PAL_SYNC_QUEUE_PATH}/pal_queue.c"
)

target_include_directories(
    pal_sync_queue
    PUBLIC
       "${PAL_SYNC_QUEUE_PATH}"
)

# ==============================================================================
# Unit library

set( UNITY_PATH "${CMAKE_SOURCE_DIR}/tests/common/Unity")

add_library(unity
            "${UNITY_PATH}/src/unity.c"
            "${UNITY_PATH}/extras/fixture/src/unity_fixture.c"
            "${UNITY_PATH}/extras/memory/src/unity_memory.c"
)

target_compile_definitions(
    unity
    PUBLIC
        -DUNITY_INCLUDE_CONFIG_H
)

target_include_directories(
    unity
    PUBLIC
       "${CMAKE_SOURCE_DIR}/tests/pal_queue"
       "${UNITY_PATH}/src"
       "${UNITY_PATH}/extras/fixture/src"
       "${UNITY_PATH}/extras/memory/src"
)

# ==============================================================================

# Demo target.
add_executable( ${DEMO_NAME}
                ${CMAKE_SOURCE_DIR}/platform/posix/clock_posix.c
                pal_queue_test.c )

target_link_libraries( ${DEMO_NAME} PRIVATE
                       unity
                       pal_sync_queue )

target_include_directories( ${DEMO_NAME}
                            PUBLIC
                              "${CMAKE_SOURCE_DIR}/platform/include"
                              "${CMAKE_SOURCE_DIR}/platform/posix/pal_queue"
                              "${CMAKE_CURRENT_LIST_DIR}"
                            PRIVATE
                              "${CORE_PKCS11_3RDPARTY_LOCATION}/mbedtls_utils" )
