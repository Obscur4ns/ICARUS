add_library(
    icarus_extension
    SHARED
        src/extension.cpp
)

target_link_libraries(
    icarus_extension
    PRIVATE
        icarus::core
        icarus::ipc
)

target_compile_features(icarus_extension PRIVATE cxx_std_20)

set_target_properties(
    icarus_extension
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "icarus_x64"
)

icarus_enable_warnings(icarus_extension)
