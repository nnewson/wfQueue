cmake_minimum_required(VERSION 3.13)

project(wfQueue)

# Find GTest
enable_testing()
find_package(GTest REQUIRED)

add_executable(wfQueueTest include/wfQueue.h tests/unittests.cpp)
include_directories(include)
target_link_libraries(wfQueueTest GTest::GTest GTest::Main)

gtest_discover_tests(wfQueueTest)

find_package(Boost 1.65 REQUIRED COMPONENTS system)
target_link_libraries(wfQueueTest Boost::system)

# Use C++17, without compiler specific extensions
target_compile_features(wfQueueTest PUBLIC cxx_std_17)
set_target_properties(wfQueueTest PROPERTIES CXX_EXTENSIONS OFF)