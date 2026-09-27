add_executable(integration_test.test)
emil_build_for(integration_test.test HOST All BOOL HAL_TI_BUILD_TESTS)
emil_add_test(integration_test.test)

target_include_directories(integration_test.test PRIVATE
    "${PROJECT_SOURCE_DIR}"
)

target_link_libraries(integration_test.test PUBLIC
    gmock_main
    infra.util
)

target_sources(integration_test.test PRIVATE
    CanBitTimingTest.cpp
    SpiClockDivisorTest.cpp
)