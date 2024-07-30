# Set the platform named based on the host OS if not defined.
set( PLATFORM_NAME "posix" CACHE STRING "Port to use for building the SDK." )

set(AWS_DEMO_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include)

# Include each subdirectory that has a CMakeLists.txt file in it
file(GLOB demo_dirs "${DEMOS_DIR}/*/*")
foreach(demo_dir IN LISTS demo_dirs)
    if(IS_DIRECTORY "${demo_dir}" AND EXISTS "${demo_dir}/CMakeLists.txt")
        add_subdirectory(${demo_dir})
    endif()
endforeach()
