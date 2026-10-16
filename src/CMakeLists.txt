# Core logic shared by the executable and the tests
add_library(lb_core STATIC
    backend.cpp
)
add_library(lb::core ALIAS lb_core)
target_include_directories(lb_core PUBLIC ${PROJECT_SOURCE_DIR}/includes)
target_link_libraries(lb_core PUBLIC lb::options Threads::Threads)

if(WIN32)
    target_link_libraries(lb_core PUBLIC ws2_32)
endif()

add_executable(opencpp_lb main.cpp)
target_link_libraries(opencpp_lb PRIVATE lb::core)
