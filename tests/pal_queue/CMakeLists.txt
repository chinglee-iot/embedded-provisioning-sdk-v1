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
                           )
