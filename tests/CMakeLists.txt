add_executable(tests
    policies/lru_test.cpp
)

target_link_libraries(
    tests PRIVATE 
    GTest::gtest_main
    my_cache::my_cache
)

include(GoogleTest)
gtest_discover_tests(tests)