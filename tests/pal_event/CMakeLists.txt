set( DEMO_NAME "pal_event_unit_test" )

# ==============================================================================
# pal sync event library

set( PAL_SYNC_EVENT_PATH "${CMAKE_SOURCE_DIR}/platform/posix/pal_event")

add_library( pal_sync_event
            "${PAL_SYNC_EVENT_PATH}/pal_event.c"
)

target_include_directories(
    pal_sync_event
    PUBLIC
       "${PAL_SYNC_EVENT_PATH}"
)

# ==============================================================================

# Demo target.
add_executable( ${DEMO_NAME}
                ${CMAKE_SOURCE_DIR}/platform/posix/clock_posix.c
                pal_event_test.c )

target_link_libraries( ${DEMO_NAME} PRIVATE
                       unity
                       pal_sync_event )

target_include_directories( ${DEMO_NAME}
                            PUBLIC
                              "${CMAKE_SOURCE_DIR}/platform/include"
                              "${CMAKE_SOURCE_DIR}/platform/posix/pal_event"
                              "${CMAKE_CURRENT_LIST_DIR}" )
