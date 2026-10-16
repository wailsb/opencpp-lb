include(GoogleTest)

add_executable(lb_unit_tests
    unit/backend_pool_test.cpp
    unit/task_queue_test.cpp
)
target_link_libraries(lb_unit_tests PRIVATE lb::core GTest::gtest_main)
gtest_discover_tests(lb_unit_tests DISCOVERY_MODE PRE_TEST)
