cmake_minimum_required(VERSION 3.28)
project(RNDR VERSION 0.1.0 LANGUAGES CXX C)

# Library options
option(RNDR_CANVAS "Enable Canvas API which is a simplified graphics API" ON)
option(RNDR_FORGE "Use more advanced rendering API that is more powerful but more complicated" OFF)
option(RNDR_HARDENING "Enable hardened mode" ON)
option(RNDR_BUILD_TESTS "Build tests" ON)
option(RNDR_BUILD_SAMPLES "Builds sample executables" ON)

message(STATUS "RNDR_FORGE ${RNDR_FORGE}")
message(STATUS "RNDR_CANVAS ${RNDR_CANVAS}")
message(STATUS "RNDR_BUILD_TESTS ${RNDR_BUILD_TESTS}")
message(STATUS "RNDR_HARDENING ${RNDR_HARDENING}")
message(STATUS "RNDR_BUILD_SAMPLES ${RNDR_BUILD_SAMPLES}")

## C++ language configuration boilerplate, hide symbols by default
if (NOT DEFINED CMAKE_CXX_VISIBILITY_PRESET AND NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
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
if (${RNDR_HARDENING})
    rndr_setup_sanitizers(rndr_options)
endif ()

include(cmake/dependencies.cmake)

# Setup reflection generator
set(RNDR_INCLUDE_DIRS $<JOIN:$<TARGET_PROPERTY:rndr,INCLUDE_DIRECTORIES>,,>)
set(RNDR_DEFINES $<JOIN:$<LIST:TRANSFORM,$<LIST:FILTER,$<TARGET_PROPERTY:rndr,COMPILE_DEFINITIONS>,INCLUDE,.+>,PREPEND,-D>,,>)
if (WIN32)
    set(OBSIDIAN_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/tools/obsidian/win32/obsidian.exe)
elseif (LINUX)
    set(OBSIDIAN_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/tools/obsidian/linux/obsidian)
else ()
    message(FATAL_ERROR "Unsupported platform for obsidian tool")
endif ()
add_custom_target(
        generate-reflection
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/include/generated
        COMMAND ${OBSIDIAN_EXECUTABLE}
        input-dirs=${CMAKE_CURRENT_SOURCE_DIR}/include,${CMAKE_CURRENT_SOURCE_DIR}/src
        output-dir=${CMAKE_CURRENT_BINARY_DIR}/include/generated
        std=c++20
        compile-options="${RNDR_DEFINES},-Wall,-Wextra"
        inc-dirs=${RNDR_INCLUDE_DIRS}
        log-level=error
        DEPENDS obsidian
)

if (${RNDR_FORGE})
    if (NOT IS_DIRECTORY $ENV{VULKAN_SDK})
        message(WARNING "Vulkan SDK not detected!")
    else ()
        message(STATUS "Vulkan SDK found at: $ENV{VULKAN_SDK}")
    endif ()
endif ()

# The compiled library code is here
add_subdirectory(src)

if (${RNDR_BUILD_TESTS})
    set(RNDR_TEST_FILES
            test/base-test.cpp
            test/bitmap-test.cpp
            test/camera-test.cpp
            test/frames-per-second-counter-test.cpp
            test/input-test.cpp
            extern/catch2/src/catch_amalgamated.cpp)
    if (${RNDR_CANVAS})
        list(APPEND RNDR_TEST_FILES
                test/canvas/context-test.cpp
                test/canvas/buffer-test.cpp
                test/canvas/texture-test.cpp
                test/canvas/render-target-test.cpp
                test/canvas/shader-test.cpp
                test/canvas/mesh-test.cpp
                test/canvas/brush-test.cpp
                test/canvas/bitmap-test.cpp)
    endif ()
    add_executable(rndr-test ${RNDR_TEST_FILES})
    target_include_directories(rndr-test PRIVATE src)
    target_include_directories(rndr-test PRIVATE extern/catch2/include)
    target_include_directories(rndr-test PRIVATE extern/catch2/include/catch2)
    target_include_directories(rndr-test PRIVATE extern/glad/include)
    target_link_libraries(rndr-test rndr)
    target_link_libraries(rndr-test rndr_warnings)
    target_link_libraries(rndr-test rndr_options)
    enable_testing()
    add_test(NAME all_tests COMMAND rndr-test)

    # Install the test executable
    install(TARGETS rndr-test
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}  # For Windows DLL files
    )
endif ()

if (${RNDR_BUILD_SAMPLES})
    add_library(shared samples/shared/types.hpp samples/shared/example-controller.h samples/shared/example-controller.cpp)
    target_include_directories(shared PUBLIC samples/shared)
    # @formatter:off
    target_compile_definitions(shared PUBLIC RNDR_SAMPLES_SHADERS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/samples/shared/shaders")
    # @formatter:on
    target_link_libraries(shared PUBLIC rndr opal)

    add_executable(window-sample samples/window/window-sample.cpp)
    target_link_libraries(window-sample PUBLIC shared imgui)

    add_executable(text-rendering-sample samples/text-rendering/text-rendering-sample.cpp)
    target_link_libraries(text-rendering-sample PUBLIC shared imgui)

    if (${RNDR_FORGE})
        add_executable(modern-vulkan samples/modern-vulkan/modern-vulkan.cpp)
        target_link_libraries(modern-vulkan shared imgui)
    endif ()

endif ()

