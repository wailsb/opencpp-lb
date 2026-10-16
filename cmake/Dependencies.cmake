# Third-party dependencies, pinned to exact releases (see ADR-0011).
# FIND_PACKAGE_ARGS lets a system/vcpkg install take precedence when available.

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

find_package(Threads REQUIRED)

# --- Asio (standalone, header-only) ------------------------------------------
FetchContent_Declare(asio
    URL      https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-2.tar.gz
    URL_HASH SHA256=755bd7f85a4b269c67ae0ea254907c078d408cce8e1a352ad2ed664d233780e8)
FetchContent_MakeAvailable(asio)

add_library(lb_asio INTERFACE)
add_library(lb::asio ALIAS lb_asio)
target_include_directories(lb_asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(lb_asio INTERFACE
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
    $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX>)
target_link_libraries(lb_asio INTERFACE
    Threads::Threads
    $<$<PLATFORM_ID:Windows>:ws2_32 mswsock>)

# --- spdlog (bundled fmt) ----------------------------------------------------
FetchContent_Declare(spdlog
    URL      https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz
    URL_HASH SHA256=1586508029a7d0670dfcb2d97575dcdc242d3868a259742b69f100801ab4e16b
    FIND_PACKAGE_ARGS 1.14)
FetchContent_MakeAvailable(spdlog)

# --- toml++ (header-only) ----------------------------------------------------
FetchContent_Declare(tomlplusplus
    URL      https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
    URL_HASH SHA256=8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155
    FIND_PACKAGE_ARGS 3.4)
FetchContent_MakeAvailable(tomlplusplus)

# --- CLI11 (header-only) -----------------------------------------------------
FetchContent_Declare(CLI11
    URL      https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.4.2.tar.gz
    URL_HASH SHA256=f2d893a65c3b1324c50d4e682c0cdc021dd0477ae2c048544f39eed6654b699a
    FIND_PACKAGE_ARGS 2.4)
FetchContent_MakeAvailable(CLI11)

# --- GoogleTest (tests only) -------------------------------------------------
if(LB_BUILD_TESTS)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        URL      https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
        FIND_PACKAGE_ARGS NAMES GTest)
    FetchContent_MakeAvailable(googletest)
endif()
