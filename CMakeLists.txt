cmake_minimum_required(VERSION 3.28)

# Derive the version (RNDR_VERSION / RNDR_VERSION_FULL) from the latest git tag before project().
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake)

project(RNDR VERSION ${RNDR_VERSION} LANGUAGES CXX C)

# Standard GNU install directory variables (CMAKE_INSTALL_LIBDIR/BINDIR/INCLUDEDIR).
include(GNUInstallDirs)

# Library options
option(RNDR_CANVAS "Enable Canvas API which is a simplified graphics API" ON)
option(RNDR_FORGE "Enable the Forge API, a lower level rendering API that is more powerful but more complicated" OFF)
option(RNDR_HARDENING "Enable hardened mode" ON)
option(RNDR_BUILD_TESTS "Build tests" ON)
option(RNDR_BUILD_SAMPLES "Builds sample executables" ON)
option(RNDR_ASSIMP "Enable Assimp dependency for mesh/model loading" ON)
option(RNDR_KTX "Enable KTX-Software dependency for KTX texture loading" ON)
option(RNDR_SHARED_LIBS "Build rndr (and opal) as shared libraries" OFF)

# rndr and opal key their static-vs-shared choice off the standard BUILD_SHARED_LIBS variable;
# RNDR_SHARED_LIBS is the project-facing knob that drives it.
set(BUILD_SHARED_LIBS ${RNDR_SHARED_LIBS})

message(STATUS "RNDR_FORGE ${RNDR_FORGE}")
message(STATUS "RNDR_CANVAS ${RNDR_CANVAS}")
message(STATUS "RNDR_BUILD_TESTS ${RNDR_BUILD_TESTS}")
message(STATUS "RNDR_HARDENING ${RNDR_HARDENING}")
message(STATUS "RNDR_BUILD_SAMPLES ${RNDR_BUILD_SAMPLES}")
message(STATUS "RNDR_ASSIMP ${RNDR_ASSIMP}")
message(STATUS "RNDR_KTX ${RNDR_KTX}")
message(STATUS "RNDR_SHARED_LIBS ${RNDR_SHARED_LIBS}")

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
    rndr_deploy_runtime(rndr-test)
    target_include_directories(rndr-test PRIVATE src)
    target_include_directories(rndr-test PRIVATE extern/catch2/include)
    target_include_directories(rndr-test PRIVATE extern/catch2/include/catch2)
    target_include_directories(rndr-test PRIVATE extern/glad/include)
    target_link_libraries(rndr-test rndr)
    target_link_libraries(rndr-test rndr_warnings)
    target_link_libraries(rndr-test rndr_options)
    enable_testing()
    add_test(NAME all_tests COMMAND rndr-test)
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
    rndr_deploy_runtime(window-sample)

    add_executable(text-rendering-sample samples/text-rendering/text-rendering-sample.cpp)
    target_link_libraries(text-rendering-sample PUBLIC shared imgui)
    rndr_deploy_runtime(text-rendering-sample)

    if (${RNDR_FORGE})
        add_executable(modern-vulkan samples/modern-vulkan/modern-vulkan.cpp)
        target_link_libraries(modern-vulkan shared imgui)
        rndr_deploy_runtime(modern-vulkan)
    endif ()

endif ()

