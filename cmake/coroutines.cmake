# Enable compiling with coroutine support on GCC or Clang
macro(enable_coroutines)

    if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        # Enforce using libc++ because of different coroutine headers, else it might end up using libstdc++
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts -stdlib=libc++")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
    endif()

endmacro()
