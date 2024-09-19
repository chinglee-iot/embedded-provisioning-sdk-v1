# This file is to add source files and include directories
# into variables so that it can be reused from different repositories
# in their Cmake based build system by including this file.
#
# Files specific to the repository such as test runner, platform tests
# are not added to the variables.


# MQTT Agent library Public Include directories.
set( SDK_MQTT_AGENT_INCLUDE_PUBLIC_DIRS
     "${CMAKE_CURRENT_LIST_DIR}/source/include" )

# MQTT Agent library source files.
set( SDK_MQTT_AGENT_SOURCES
     "${CMAKE_CURRENT_LIST_DIR}/source/mqtt_agent.c"
     "${CMAKE_CURRENT_LIST_DIR}/source/subscription_manager.c"
    "${CMAKE_CURRENT_LIST_DIR}/source/freertos_agent_message.c"
    "${CMAKE_CURRENT_LIST_DIR}/source/freertos_command_pool.c" )

