# Unit tests for the framing core. Standalone project: it does not need OBS or the NVIDIA SDK.
#
#   cmake -S tests -B build_tests
#   cmake --build build_tests --config Release
#   ctest --test-dir build_tests -C Release --output-on-failure
cmake_minimum_required(VERSION 3.28)

project(obs-face-autoframe-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Catch2 3 REQUIRED)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_executable(
  framing-tests
  framing/test-types.cpp
  framing/test-tracker.cpp
  framing/test-target-selector.cpp
  framing/test-framer.cpp
  framing/test-motion.cpp
  framing/test-autoframer.cpp
  "${SRC_DIR}/framing/autoframer.cpp"
  "${SRC_DIR}/framing/framer.cpp"
  "${SRC_DIR}/framing/motion.cpp"
  "${SRC_DIR}/framing/target-selector.cpp"
  "${SRC_DIR}/framing/tracker.cpp"
)
target_include_directories(framing-tests PRIVATE "${SRC_DIR}")
target_link_libraries(framing-tests PRIVATE Catch2::Catch2WithMain)
if(MSVC)
  target_compile_options(framing-tests PRIVATE /W4 /utf-8)
endif()

include(CTest)
include(Catch)
catch_discover_tests(framing-tests)
