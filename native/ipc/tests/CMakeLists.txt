add_executable(
    icarus_ipc_tests
    bridge_tests.cpp
)

target_link_libraries(
    icarus_ipc_tests
    PRIVATE
        icarus::ipc
)

target_compile_features(icarus_ipc_tests PRIVATE cxx_std_20)

icarus_enable_warnings(icarus_ipc_tests)

add_test(NAME icarus_ipc_bridge COMMAND icarus_ipc_tests)
