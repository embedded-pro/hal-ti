add_executable(integration_test.test)
emil_build_for(integration_test.test BOOL HAL_TI_BUILD_TESTS)
emil_add_test(integration_test.test)

target_link_libraries(integration_test.test PUBLIC
    gmock_main
    integration_test.logic
    protobuf.test_doubles
)

target_sources(integration_test.test PRIVATE
    Test.cpp
)