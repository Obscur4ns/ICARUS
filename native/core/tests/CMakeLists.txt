add_executable(
    icarus_core_tests
    version_tests.cpp
)

target_link_libraries(icarus_core_tests PRIVATE icarus::core)
target_compile_features(icarus_core_tests PRIVATE cxx_std_20)

icarus_enable_warnings(icarus_core_tests)

add_test(NAME icarus_core_version COMMAND icarus_core_tests)
