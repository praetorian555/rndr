cmake_minimum_required(VERSION 3.20)

project(
        RNDR
        VERSION 0.1
        DESCRIPTION "Rendering library"
        LANGUAGES CXX)

# Library options
option(RNDR_HARDENED "Enable hardened mode" OFF)
option(RNDR_TRACER "Use built in profiler" ON)
option(RNDR_DEFAULT_LOGGER "Use default logger" ON)

message(STATUS "RNDR_HARDENED ${RNDR_HARDENED}")
message(STATUS "RNDR_TRACER ${RNDR_TRACER}")
message(STATUS "RNDR_DEFAULT_LOGGER ${RNDR_DEFAULT_LOGGER}")

if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif ()

# Only do these if this is the main project, and not if it is included through add_subdirectory
if (PROJECT_IS_TOP_LEVEL)

    # Let's ensure -std=c++xx instead of -std=g++xx
    set(CMAKE_CXX_EXTENSIONS OFF)

    include(cmake/optimized-debug.cmake)

    # Configuration types
    SET(CMAKE_CONFIGURATION_TYPES "Debug;OptimizedDebug;Release" CACHE STRING "Choose configuration: Debug OptimizedDebug Release" FORCE)

    # Build only static libraries
    set(BUILD_SHARED_LIBS OFF)

    # Let's nicely support folders in IDEs
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    # Include cmake scripts here
    include(cmake/clang-format.cmake)
endif ()

# Setup compiler warnings and options
include(cmake/compiler-warnings.cmake)
include(cmake/compiler-options.cmake)
add_library(rndr_warnings INTERFACE)
add_library(rndr_options INTERFACE)
rndr_setup_compiler_warnings(rndr_warnings)
rndr_setup_compiler_options(rndr_options)

# Setup sanitizer options
include(cmake/sanitizers.cmake)
if (RNDR_HARDENED)
    rndr_setup_sanitizers(rndr_options)
endif ()

include(cmake/dependencies.cmake)
setup_dependencies()

# Adds external dependencies
add_subdirectory(extern)

# The compiled library code is here
add_subdirectory(src)

if (PROJECT_IS_TOP_LEVEL)

    # Tests
    add_subdirectory(tests)
    enable_testing()
    add_test(NAME all_tests_no_window_no_render_api COMMAND rndr_test "~[window] ~[render-api]")

    # Examples
    add_subdirectory(examples/01-triangle)
    add_subdirectory(examples/02-cube)
    add_subdirectory(examples/03-image)
    add_subdirectory(examples/04-assimp)
    add_subdirectory(examples/05-mesh-optimizer)
    add_subdirectory(examples/06-vertex-pulling)
    add_subdirectory(examples/07-cube-map)
    add_subdirectory(examples/08-debug-features)
    add_subdirectory(examples/09-animation)
    add_subdirectory(examples/10-converters)
    add_subdirectory(examples/11-mesh-renderer)
    add_subdirectory(examples/12-pbr-shading)
    add_subdirectory(examples/13-material-system)

endif ()
