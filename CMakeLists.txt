cmake_minimum_required(VERSION 3.20)

project(red_black_tree
  VERSION 0.1.0
  DESCRIPTION "MPSC Wait free queue in C++"
  LANGUAGES CXX
)

find_package(GTest CONFIG REQUIRED)

# Header-only library target for red_black_tree
add_library(wait_free_queue INTERFACE)
target_include_directories(wait_free_queue INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_compile_features(wait_free_queue INTERFACE cxx_std_23)
# Suppress the "interference size" warning, as we use a hardware defined cache-line.
# This causes a warning as it can differ at the ABI level, but this queue ISN'T designed
# to be used across different ABIs, so we can safely ignore this warning.
target_compile_options(wait_free_queue INTERFACE -Wno-interference-size)

# Tests
enable_testing()
add_executable(test_wait_free_queue tests/test_wfqueue.cpp)
target_link_libraries(test_wait_free_queue PRIVATE wait_free_queue GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(test_wait_free_queue)