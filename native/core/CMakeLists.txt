add_library(
    icarus_core
    STATIC
        src/version.cpp
)

add_library(icarus::core ALIAS icarus_core)

target_compile_features(icarus_core PUBLIC cxx_std_20)

target_compile_definitions(
    icarus_core
    PRIVATE
        ICARUS_VERSION_STRING="${PROJECT_VERSION}"
)

target_include_directories(
    icarus_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

set_target_properties(
    icarus_core
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

icarus_enable_warnings(icarus_core)
