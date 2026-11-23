add_executable(tests
    policies/common_policies_tests.cpp
    policies/lru_test.cpp
    policies/lfu_test.cpp
    policies/lirs_test.cpp
)

target_link_libraries(
    tests PRIVATE 
    GTest::gtest_main
    my_cache::my_cache
)

include(GoogleTest)
gtest_discover_tests(tests)
