add_executable(
    icarus_core_tests
    version_tests.cpp
)

target_link_libraries(icarus_core_tests PRIVATE icarus::core)
target_compile_features(icarus_core_tests PRIVATE cxx_std_20)

icarus_enable_warnings(icarus_core_tests)

add_test(NAME icarus_core_version COMMAND icarus_core_tests)

add_executable(
    icarus_identity_protocol_tests
    identity_protocol_tests.cpp
)

target_link_libraries(icarus_identity_protocol_tests PRIVATE icarus::core)
target_compile_features(icarus_identity_protocol_tests PRIVATE cxx_std_20)

icarus_enable_warnings(icarus_identity_protocol_tests)

add_test(NAME icarus_identity_protocol COMMAND icarus_identity_protocol_tests)

add_executable(
    icarus_direct_voice_acoustics_tests
    direct_voice_acoustics_tests.cpp
)

target_link_libraries(icarus_direct_voice_acoustics_tests PRIVATE icarus::core)
target_compile_features(icarus_direct_voice_acoustics_tests PRIVATE cxx_std_20)

icarus_enable_warnings(icarus_direct_voice_acoustics_tests)

add_test(NAME icarus_direct_voice_acoustics COMMAND icarus_direct_voice_acoustics_tests)
