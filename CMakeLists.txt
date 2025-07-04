cmake_minimum_required(VERSION 3.28)
project(RNDR VERSION 0.1.0 LANGUAGES CXX C)

# Library options
option(RNDR_BUILD_TESTS "Build tests" ON)
message(STATUS "RNDR_BUILD_TESTS ${RNDR_BUILD_TESTS}")
option(RNDR_SHARED_LIBS "Build as a shared library" OFF)
message(STATUS "RNDR_SHARED_LIBS ${RNDR_SHARED_LIBS}")
option(RNDR_HARDENING "Enable hardened mode" ON)
message(STATUS "RNDR_HARDENING ${RNDR_HARDENING}")
option(RNDR_TRACER "Use built in profiler" OFF)
message(STATUS "RNDR_TRACER ${RNDR_TRACER}")
option(RNDR_DEFAULT_LOGGER "Use default logger" OFF)
message(STATUS "RNDR_DEFAULT_LOGGER ${RNDR_DEFAULT_LOGGER}")
option(RNDR_BUILD_SAMPLES "Builds sample executables" ON)
message(STATUS "RNDR_BUILD_SAMPLES ${RNDR_BUILD_SAMPLES}")

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
SET(OPAL_BUILD_TESTS OFF)
SET(OPAL_HARDENING ${RNDR_HARDENING})
set(OPAL_SHARED_LIBS ${RNDR_SHARED_LIBS})
FetchContent_Declare(
        opal
        GIT_REPOSITORY https://github.com/praetorian555/opal
        GIT_TAG main
)
FetchContent_MakeAvailable(opal)

# Setup IMGUI ######################################################################
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG f5befd2d29e66809cd1110a152e375a7f1981f06 # release-1.91.9b
)
FetchContent_MakeAvailable(imgui)
FetchContent_GetProperties(imgui SOURCE_DIR SOURCE_PATH)
set(IMGUI_SOURCE_FILES
        ${SOURCE_PATH}/imgui.cpp
        ${SOURCE_PATH}/imgui.h
        ${SOURCE_PATH}/imconfig.h
        ${SOURCE_PATH}/imgui_internal.h
        ${SOURCE_PATH}/imstb_rectpack.h
        ${SOURCE_PATH}/imstb_truetype.h
        ${SOURCE_PATH}/imstb_textedit.h
        ${SOURCE_PATH}/imgui_draw.cpp
        ${SOURCE_PATH}/imgui_demo.cpp
        ${SOURCE_PATH}/imgui_tables.cpp
        ${SOURCE_PATH}/imgui_widgets.cpp)
# Use Win32 backhand for Windows
if (MSVC)
    set(IMGUI_WINDOWS_SOURCE_FILES
            ${SOURCE_PATH}/backends/imgui_impl_win32.cpp
            ${SOURCE_PATH}/backends/imgui_impl_win32.h)
else ()
    set(IMGUI_WINDOWS_SOURCE_FILES)
endif ()
set(IMGUI_OPENGL_SOURCE_FILES
        ${SOURCE_PATH}/backends/imgui_impl_opengl3.cpp
        ${SOURCE_PATH}/backends/imgui_impl_opengl3.h)
add_library(imgui
        ${IMGUI_SOURCE_FILES}
        ${IMGUI_WINDOWS_SOURCE_FILES}
        ${IMGUI_OPENGL_SOURCE_FILES})
target_include_directories(imgui PUBLIC ${SOURCE_PATH})

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
endif ()

if (RNDR_BUILD_SAMPLES)
    add_library(shared samples/shared/example-controller.h samples/shared/example-controller.cpp)
    target_include_directories(shared PUBLIC samples/shared)
    target_link_libraries(shared PUBLIC rndr opal)

    add_executable(window-sample samples/window/window-sample.cpp)
    target_link_libraries(window-sample PUBLIC shared imgui)

endif ()

