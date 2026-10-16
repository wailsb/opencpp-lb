cmake_minimum_required(VERSION 3.12)
project(opencpp_lb VERSION 1.0 LANGUAGES CXX)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the executable and point to main.cpp
add_executable(opencpp_lb src/main.cpp)

# Platform-specific configurations
if(WIN32)
    # Link Winsock library explicitly on Windows
    target_link_libraries(opencpp_lb PRIVATE ws2_32)
endif()