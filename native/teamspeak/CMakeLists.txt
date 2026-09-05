if(NOT EXISTS "${ICARUS_TS3_SDK_ROOT}/include/ts3_functions.h")
    message(
        FATAL_ERROR
        "TeamSpeak 3 Plugin SDK not found at '${ICARUS_TS3_SDK_ROOT}'. Run tools/fetch-ts3-sdk.ps1 first."
    )
endif()

add_library(
    icarus_ts3_plugin
    MODULE
        src/plugin.cpp
)

target_link_libraries(icarus_ts3_plugin PRIVATE icarus::core)
target_compile_features(icarus_ts3_plugin PRIVATE cxx_std_20)

target_include_directories(
    icarus_ts3_plugin
    SYSTEM PRIVATE
        "${ICARUS_TS3_SDK_ROOT}/include"
)

set_target_properties(
    icarus_ts3_plugin
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "icarus"
)

icarus_enable_warnings(icarus_ts3_plugin)
