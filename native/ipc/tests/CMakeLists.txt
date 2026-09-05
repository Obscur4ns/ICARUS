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

if(WIN32)
    target_compile_definitions(
        icarus_ipc_tests
        PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
endif()

icarus_enable_warnings(icarus_ipc_tests)

add_test(NAME icarus_ipc_bridge COMMAND icarus_ipc_tests)

add_executable(
    icarus_session_state_tests
    session_state_tests.cpp
)

target_link_libraries(
    icarus_session_state_tests
    PRIVATE
        icarus::ipc
)

target_compile_features(icarus_session_state_tests PRIVATE cxx_std_20)

if(WIN32)
    target_compile_definitions(
        icarus_session_state_tests
        PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
endif()

icarus_enable_warnings(icarus_session_state_tests)

add_test(NAME icarus_session_state COMMAND icarus_session_state_tests)

add_executable(
    icarus_spatial_scene_tests
    spatial_scene_tests.cpp
)

target_link_libraries(
    icarus_spatial_scene_tests
    PRIVATE
        icarus::ipc
)

target_compile_features(icarus_spatial_scene_tests PRIVATE cxx_std_20)

if(WIN32)
    target_compile_definitions(
        icarus_spatial_scene_tests
        PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
endif()

icarus_enable_warnings(icarus_spatial_scene_tests)

add_test(NAME icarus_spatial_scene COMMAND icarus_spatial_scene_tests)
