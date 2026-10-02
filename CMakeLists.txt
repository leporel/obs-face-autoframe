cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

include(compilerconfig)
include(defaults)
include(helpers)

if(NOT WIN32)
  message(FATAL_ERROR "${CMAKE_PROJECT_NAME} supports Windows only (NVIDIA AR SDK + Direct3D 11).")
endif()

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# The template pins C++17 globally; this plugin needs C++20 for designated initializers and std::span.
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /Zc:char8_t-)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/filter/autoframe-filter.cpp
    src/filter/autoframe-filter.hpp
    src/filter/autoframe-properties.cpp
    src/filter/autoframe-settings.cpp
    src/filter/autoframe-settings.hpp
    src/filter/detection-stats.hpp
    src/filter/settings-keys.hpp
    src/detect/async-detector.cpp
    src/detect/async-detector.hpp
    src/detect/detection-backend.hpp
    src/detect/detection.hpp
    src/detect/detector.hpp
    src/detect/nvar/gpu-check.cpp
    src/detect/nvar/gpu-check.hpp
    src/detect/nvar/nvar-api.cpp
    src/detect/nvar/nvar-api.hpp
    src/detect/nvar/nvar-defs.hpp
    src/detect/nvar/nvar-detector.cpp
    src/detect/nvar/nvar-detector.hpp
    src/detect/nvar/nvar-factory.cpp
    src/detect/nvar/nvar-factory.hpp
    src/detect/nvar/nvcv-image.cpp
    src/detect/nvar/nvcv-image.hpp
    src/framing/autoframer.cpp
    src/framing/autoframer.hpp
    src/framing/framer.cpp
    src/framing/framer.hpp
    src/framing/motion.cpp
    src/framing/motion.hpp
    src/framing/target-selector.cpp
    src/framing/target-selector.hpp
    src/framing/tracker.cpp
    src/framing/tracker.hpp
    src/framing/types.hpp
    src/render/color.hpp
    src/render/debug-draw.cpp
    src/render/debug-draw.hpp
    src/render/detect-frame.cpp
    src/render/detect-frame.hpp
    src/render/source-capture.cpp
    src/render/source-capture.hpp
    src/util/graphics-guard.hpp
    src/util/localize.hpp
    src/util/log.hpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
