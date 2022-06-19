cmake_minimum_required(VERSION 3.18.1)

project(crhandle CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(crhandle_build_tests "Build unit tests." OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(warnings)
include(coroutines)

set_warnings(TREAT_AS_ERRORS)
enable_coroutines()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(crhandle_build_tests)
    enable_testing()
endif()


add_subdirectory(src)

if(crhandle_build_tests)
    add_subdirectory(test)
endif()
