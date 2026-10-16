cmake_minimum_required(VERSION 3.24)
project(opencpp_lb VERSION 0.1.0 LANGUAGES CXX)

option(LB_BUILD_TESTS "Build unit and integration tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(CompilerWarnings)
include(Sanitizers)
include(Dependencies)

# Shared build settings for every first-party target
add_library(lb_options INTERFACE)
add_library(lb::options ALIAS lb_options)
lb_set_warnings(lb_options)
lb_enable_sanitizers(lb_options)

add_subdirectory(src)

if(LB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
