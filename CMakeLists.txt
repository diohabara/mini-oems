cmake_minimum_required(VERSION 3.28)
project(mini-oems LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(mini-oems src/main.cc)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(sanity_test test/sanity_test.cc)
target_link_libraries(sanity_test GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(sanity_test)
