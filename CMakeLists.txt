cmake_minimum_required(VERSION 3.28)
project(RNDR VERSION 0.1.0 LANGUAGES CXX C)

# Library options
option(RNDR_BUILD_TESTS "Build tests" ON)
message(STATUS "RNDR_BUILD_TESTS ${RNDR_BUILD_TESTS}")
option(RNDR_SHARED_LIBS "Build shared library" OFF)
message(STATUS "RNDR_SHARED_LIBS ${RNDR_SHARED_LIBS}")
option(RNDR_HARDENING "Enable hardened mode" ON)
message(STATUS "RNDR_HARDENING ${RNDR_HARDENING}")
option(RNDR_TRACER "Use built in profiler" OFF)
message(STATUS "RNDR_TRACER ${RNDR_TRACER}")
option(RNDR_DEFAULT_LOGGER "Use default logger" OFF)
message(STATUS "RNDR_DEFAULT_LOGGER ${RNDR_DEFAULT_LOGGER}")

## C++ language configuration boilerplate, hide symbols by default
if (NOT DEFINED CMAKE_CXX_VISIBILITY_PRESET AND NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
endif ()

## Let OPAL_SHARED_LIBS override BUILD_SHARED_LIBS
if (RNDR_SHARED_LIBS)
    set(BUILD_SHARED_LIBS "${RNDR_SHARED_LIBS}")
endif ()

# Setup compiler warnings and options
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
include(cmake/compiler-warnings.cmake)
include(cmake/compiler-options.cmake)
add_library(rndr_warnings INTERFACE)
add_library(rndr_options INTERFACE)
rndr_setup_compiler_warnings(rndr_warnings)
rndr_setup_compiler_options(rndr_options)

# Setup sanitizer options
include(cmake/sanitizers.cmake)
if (RNDR_HARDENING)
    rndr_setup_sanitizers(rndr_options)
endif ()

include(FetchContent)
SET(MATH_BUILD_TESTS OFF)
SET(MATH_HARDENED ${RNDR_HARDENING})
set(MATH_SHARED_LIBS ${RNDR_SHARED_LIBS})
FetchContent_Declare(
        math
        GIT_REPOSITORY https://github.com/praetorian555/math
        GIT_TAG        main
)
FetchContent_MakeAvailable(math)

SET(OPAL_BUILD_TESTS OFF)
SET(OPAL_HARDENING ${RNDR_HARDENING})
set(OPAL_SHARED_LIBS ${RNDR_SHARED_LIBS})
FetchContent_Declare(
        opal
        GIT_REPOSITORY https://github.com/praetorian555/opal
        GIT_TAG        main
)
FetchContent_MakeAvailable(opal)

# The compiled library code is here
add_subdirectory(src)

if (RNDR_BUILD_TESTS)
    set(RNDR_TEST_FILES
        test/base-test.cpp
        test/bitmap-test.cpp
        test/camera-test.cpp
        test/frames-per-second-counter-test.cpp
        test/input-test.cpp
        test/render-api-test.cpp
        test/renderer-base-test.cpp
        test/window-test.cpp
        extern/catch2/src/catch_amalgamated.cpp)
    add_executable(rndr_test ${RNDR_TEST_FILES})
    target_include_directories(rndr_test PRIVATE src)
    target_include_directories(rndr_test PRIVATE extern/catch2/include)
    target_include_directories(rndr_test PRIVATE extern/catch2/include/catch2)
    target_include_directories(rndr_test PRIVATE extern/glad/include)
    target_link_libraries(rndr_test rndr)
    target_link_libraries(rndr_test rndr_warnings)
    target_link_libraries(rndr_test rndr_options)
    enable_testing()
    add_test(NAME all_tests_no_window_no_render_api COMMAND rndr_test "~[window] ~[render-api]")

    # Install the test executable
    install(TARGETS rndr_test
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}  # For Windows DLL files
    )
endif()

