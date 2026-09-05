cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ProjectVersion)
icarus_read_project_version(
    "${CMAKE_CURRENT_SOURCE_DIR}/addons/main/script_version.hpp"
    ICARUS_PROJECT_VERSION
)

project(ICARUS VERSION ${ICARUS_PROJECT_VERSION} LANGUAGES CXX)

include(CompilerWarnings)

option(ICARUS_BUILD_TESTS "Build ICARUS native tests" ON)
option(ICARUS_BUILD_EXTENSION "Build the ArmA 3 extension" ON)
option(ICARUS_BUILD_TS3_PLUGIN "Build the TeamSpeak 3 plugin" OFF)

set(
    ICARUS_TS3_SDK_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/ts3client-pluginsdk"
    CACHE PATH
    "Path to the TeamSpeak 3 Client Plugin SDK"
)

if((ICARUS_BUILD_EXTENSION OR ICARUS_BUILD_TS3_PLUGIN) AND NOT WIN32)
    message(FATAL_ERROR "The ArmA extension and TeamSpeak plugin currently target Windows only.")
endif()

if((ICARUS_BUILD_EXTENSION OR ICARUS_BUILD_TS3_PLUGIN) AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The ArmA extension and TeamSpeak plugin currently target x64 only.")
endif()

add_subdirectory(native/core)
add_subdirectory(native/ipc)

if(ICARUS_BUILD_EXTENSION)
    add_subdirectory(native/extension)
endif()

if(ICARUS_BUILD_TS3_PLUGIN)
    add_subdirectory(native/teamspeak)
endif()

if(ICARUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(native/core/tests)
    add_subdirectory(native/ipc/tests)
endif()
