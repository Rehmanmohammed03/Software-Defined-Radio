cmake_minimum_required(VERSION 3.14)
cmake_policy(SET CMP0135 NEW)
project(3dy4_project_bench_test)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Default to Release build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build: Debug or Release." FORCE)
endif()

# Ensure valid build type
if(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
  message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}. Allowed values are: Debug, Release.")
endif()

message(STATUS "Build type set to: ${CMAKE_BUILD_TYPE}")

# Add compile options and definitions based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-Wall -g -O0)  # Debug options
else()
  add_compile_options(-Wall -O3)     # Release options
endif()
add_compile_options(-Wno-psabi)

# Enable the DOUBLE macro to work with reals on 8 bytes 
#add_compile_definitions(DOUBLE)
# Remove the above DOUBLE macro if you wish to work with reals on 4 bytes

# Define available targets and set default to build all
set(AVAILABLE_TARGETS project test_primitives bench_primitives)
set(TARGET_TO_BUILD "all" CACHE STRING "Choose which target to build: all, project, test_primitives, bench_primitives.")

# Validate TARGET_TO_BUILD
list(FIND AVAILABLE_TARGETS ${TARGET_TO_BUILD} TARGET_INDEX)
if(TARGET_INDEX EQUAL -1 AND NOT TARGET_TO_BUILD STREQUAL "all")
  message(FATAL_ERROR "Invalid TARGET_TO_BUILD: ${TARGET_TO_BUILD}. Allowed values are: all, project, test_primitives, bench_primitives.")
endif()

message(STATUS "Selected target to build: ${TARGET_TO_BUILD}")

# Conditionally include dependencies and define targets
if(TARGET_TO_BUILD STREQUAL "project" OR TARGET_TO_BUILD STREQUAL "all")
  message(STATUS "Adding target: project")

  include_directories(./include)
  file(GLOB PROJECT_SOURCES "src/*.c*")
  list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*[Bb]ench\\.c.*$")
  list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*[Tt]est\\.c.*$")

  add_executable(project ${PROJECT_SOURCES})
  target_link_libraries(project pthread)
endif()

if(TARGET_TO_BUILD STREQUAL "test_primitives" OR TARGET_TO_BUILD STREQUAL "all")
  message(STATUS "Adding target: test_primitives and fetching GoogleTest dependencies")

  # Fetch GoogleTest only if this target is built
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(googletest)

  include_directories(./include)
  file(GLOB TEST_SOURCES "src/*.c*" "test/*.c*")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*[Bb]ench\\.c.*$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*[Pp]roject\\.c.*$")

  add_executable(test_primitives ${TEST_SOURCES})
  target_link_libraries(test_primitives pthread gtest_main)
  include(GoogleTest)
  gtest_discover_tests(test_primitives)
endif()

if(TARGET_TO_BUILD STREQUAL "bench_primitives" OR TARGET_TO_BUILD STREQUAL "all")
  message(STATUS "Adding target: bench_primitives and fetching Google Benchmark dependencies")

  # Fetch Google Benchmark only if this target is built
  include(FetchContent)
  FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  # Disable Google Benchmark's dependency on Google Test
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable testing for Google Benchmark" FORCE)

  FetchContent_MakeAvailable(googlebenchmark)

  include_directories(./include)
  file(GLOB BENCH_SOURCES "src/*.c*" "bench/*.c*")
  list(FILTER BENCH_SOURCES EXCLUDE REGEX ".*[Tt]est\\.c.*$")
  list(FILTER BENCH_SOURCES EXCLUDE REGEX ".*[Pp]roject\\.c.*$")

  add_executable(bench_primitives ${BENCH_SOURCES})
  target_link_libraries(bench_primitives pthread benchmark::benchmark)
endif()
