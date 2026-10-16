# Core logic shared by the executable and the tests
add_library(lb_core STATIC
    app/application.cpp
    balance/balancer.cpp
    config/config.cpp
    net/endpoint.cpp
    net/listener.cpp
    net/pipe.cpp
    net/session.cpp
    net/worker_pool.cpp
    observe/logging.cpp
)
add_library(lb::core ALIAS lb_core)
target_include_directories(lb_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lb_core PUBLIC lb::options lb::asio spdlog::spdlog)

add_executable(opencpp_lb main.cpp)
target_link_libraries(opencpp_lb PRIVATE lb::core CLI11::CLI11)
