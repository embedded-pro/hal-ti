add_executable(integration_test.test)
emil_build_for(integration_test.test HOST All BOOL HAL_TI_BUILD_TESTS)
emil_add_test(integration_test.test)

target_link_libraries(integration_test.test PUBLIC
    gmock_main
)

target_sources(integration_test.test PRIVATE
    Test.cpp
)