include(FetchContent)

FetchContent_Declare(googletest
    GIT_REPOSITORY    https://github.com/google/googletest.git
    GIT_TAG           release-1.11.0
    SOURCE_DIR        "${CMAKE_CURRENT_BINARY_DIR}/googletest-src"
    BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/googletest-build"
)
# Don't build gmock
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(crhandletests
        test_taskhandle.cpp
        test_taskowner.cpp
        test_taskutils.cpp
        test_unichannel.cpp
        )

target_link_libraries(crhandletests
        PRIVATE
        GTest::gtest_main
        cr::handle
        )

include(GoogleTest)

if(MSVC)
    gtest_discover_tests(crhandletests
            TEST_FILTER -*anyof*)
else()
    gtest_discover_tests(crhandletests)
endif()
