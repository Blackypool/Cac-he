add_executable(tests
    policies/lru_test.cpp
    policies/lfu_test.cpp
    policies/common_policies_tests.cpp
)

target_link_libraries(
    tests PRIVATE 
    GTest::gtest_main
    my_cache::my_cache
)

include(GoogleTest)
gtest_discover_tests(tests)