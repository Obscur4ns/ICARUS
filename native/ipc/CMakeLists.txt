add_library(
    icarus_ipc
    STATIC
        src/bridge.cpp
        src/session_state.cpp
)

add_library(icarus::ipc ALIAS icarus_ipc)

target_compile_features(icarus_ipc PUBLIC cxx_std_20)

target_include_directories(
    icarus_ipc
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(
    icarus_ipc
    PUBLIC
        icarus::core
)

if(WIN32)
    target_compile_definitions(
        icarus_ipc
        PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )

    target_link_libraries(
        icarus_ipc
        PRIVATE
            kernel32
    )
endif()

icarus_enable_warnings(icarus_ipc)