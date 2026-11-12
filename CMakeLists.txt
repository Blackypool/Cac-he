cmake_minimum_required(VERSION 3.14)

project(my_cache VERSION 1.0.0 LANGUAGES CXX)

add_library(my_cache INTERFACE)
add_library(my_cache::my_cache ALIAS my_cache)

option(MY_CACHE_BUILD_MAIN ON)
option(MY_CACHE_JSON_PARSER OFF)
option(MY_CACHE_TESTS OFF)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(MY_CACHE_TESTS ON)
endif()

target_include_directories(my_cache INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)

target_compile_features(my_cache INTERFACE 
    cxx_std_20
)

if(MY_CACHE_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(test1 tests/test1.cpp)
    target_link_libraries(
        test1 PRIVATE
        GTest::gtest_main
        my_cache::my_cache
    )

    include(GoogleTest)
    gtest_discover_tests(test1)
endif()

if(MY_CACHE_BUILD_MAIN)
    add_executable(main main.cpp)
    target_link_libraries(main PRIVATE my_cache::my_cache)
endif()