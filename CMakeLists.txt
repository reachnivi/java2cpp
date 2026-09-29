cmake_minimum_required(VERSION 3.16)
project(java2cpp LANGUAGES CXX)

# Gringofts builds with C++17, so we do too.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# OFF: build tests against YOUR code in exercises/NN_*/src
# ON : build tests against the reference answers in solutions/NN_*
option(J2C_USE_SOLUTIONS "Build tests against the reference solutions" OFF)
# address | thread | undefined | "" (none)
set(J2C_SANITIZER "" CACHE STRING "Sanitizer to enable: address, thread, undefined")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)
if(J2C_SANITIZER)
  add_compile_options(-fsanitize=${J2C_SANITIZER} -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=${J2C_SANITIZER})
endif()

# GoogleTest (Gringofts uses gtest + gmock for its unit tests)
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()
find_package(Threads REQUIRED)

enable_testing()
include(GoogleTest)

file(GLOB EXERCISE_DIRS RELATIVE ${CMAKE_SOURCE_DIR}/exercises ${CMAKE_SOURCE_DIR}/exercises/[0-9][0-9]_*)
list(SORT EXERCISE_DIRS)

foreach(ex ${EXERCISE_DIRS})
  if(NOT EXISTS ${CMAKE_SOURCE_DIR}/exercises/${ex}/test.cpp)
    continue()  # work-in-progress exercise folder
  endif()
  if(J2C_USE_SOLUTIONS)
    set(code_dir ${CMAKE_SOURCE_DIR}/solutions/${ex})
  else()
    set(code_dir ${CMAKE_SOURCE_DIR}/exercises/${ex}/src)
  endif()
  file(GLOB code_srcs ${code_dir}/*.cpp)
  string(SUBSTRING ${ex} 0 2 num)
  set(target ex${num})

  add_executable(${target} ${CMAKE_SOURCE_DIR}/exercises/${ex}/test.cpp ${code_srcs})
  target_include_directories(${target} PRIVATE ${code_dir})
  target_link_libraries(${target} PRIVATE GTest::gtest_main GTest::gmock Threads::Threads)
  # Every test gets a timeout so a deadlock in your code fails instead of hanging.
  gtest_discover_tests(${target} TEST_PREFIX "${ex}." PROPERTIES TIMEOUT 20)
endforeach()

# Debugging practice: deliberately broken programs (see crash_lab/README.md)
add_subdirectory(crash_lab)
