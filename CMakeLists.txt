cmake_minimum_required(VERSION 3.12)
project(opencpp_lb VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Link threads library (required for std::thread on Linux)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Core logic shared by the executable and the tests
add_library(opencpp_lb_core STATIC
    src/backend.cpp
)
target_include_directories(opencpp_lb_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/includes)
target_link_libraries(opencpp_lb_core PUBLIC Threads::Threads)

if(WIN32)
    target_link_libraries(opencpp_lb_core PUBLIC ws2_32)
endif()

if(MSVC)
    target_compile_options(opencpp_lb_core PUBLIC /W4)
else()
    target_compile_options(opencpp_lb_core PUBLIC -Wall -Wextra)
endif()

add_executable(opencpp_lb src/main.cpp)
target_link_libraries(opencpp_lb PRIVATE opencpp_lb_core)

# Unit tests (run with: ctest --test-dir build)
enable_testing()
add_executable(opencpp_lb_tests tests/unit_tests.cpp)
target_link_libraries(opencpp_lb_tests PRIVATE opencpp_lb_core)
add_test(NAME unit_tests COMMAND opencpp_lb_tests)
