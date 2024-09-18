# Set the platform named based on the host OS if not defined.
set( PLATFORM_NAME "posix" CACHE STRING "Port to use for building the SDK." )

add_subdirectory( pal_queue ) 
