# Set the platform named based on the host OS if not defined.
set( PLATFORM_NAME "posix" CACHE STRING "Port to use for building the SDK." )


# ==============================================================================
# Unit library

set( UNITY_PATH "${CMAKE_SOURCE_DIR}/tests/common/Unity")

add_library(unity
            "${UNITY_PATH}/src/unity.c"
            "${UNITY_PATH}/extras/fixture/src/unity_fixture.c"
            "${UNITY_PATH}/extras/memory/src/unity_memory.c"
)

target_include_directories(
    unity
    PUBLIC
       "${UNITY_PATH}/src"
       "${UNITY_PATH}/extras/fixture/src"
       "${UNITY_PATH}/extras/memory/src"
)

# ==============================================================================

add_subdirectory( pal_queue )
add_subdirectory( pal_event )
