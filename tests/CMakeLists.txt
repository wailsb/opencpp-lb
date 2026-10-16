include(GoogleTest)

add_executable(lb_unit_tests
    unit/balancer_test.cpp
    unit/config_test.cpp
)
target_link_libraries(lb_unit_tests PRIVATE lb::core GTest::gtest_main)
gtest_discover_tests(lb_unit_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS unit)

add_executable(lb_integration_tests
    integration/proxy_test.cpp
)
target_link_libraries(lb_integration_tests PRIVATE lb::core GTest::gtest_main)
gtest_discover_tests(lb_integration_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS integration TIMEOUT 60)
