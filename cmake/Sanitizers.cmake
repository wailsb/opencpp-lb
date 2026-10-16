# Optional sanitizer instrumentation, e.g. -DLB_SANITIZE=address,undefined or -DLB_SANITIZE=thread

set(LB_SANITIZE "" CACHE STRING "Comma-separated list of sanitizers to enable (GCC/Clang only)")

function(lb_enable_sanitizers target)
    if(NOT LB_SANITIZE)
        return()
    endif()
    if(MSVC)
        message(WARNING "LB_SANITIZE is ignored with MSVC")
        return()
    endif()
    target_compile_options(${target} INTERFACE -fsanitize=${LB_SANITIZE} -fno-omit-frame-pointer)
    target_link_options(${target} INTERFACE -fsanitize=${LB_SANITIZE})
endfunction()
