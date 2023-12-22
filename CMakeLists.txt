cmake_minimum_required(VERSION 3.20)

project(
        RNDR
        VERSION 0.1
        DESCRIPTION "Example C++ Project using CMake"
        LANGUAGES CXX)

# Library options
option(RNDR_OPENGL "Use OpenGL API" ON)
option(RNDR_SANITIZER "Enable address sanitizer" OFF)
option(RNDR_TRACER "Use built in profiler" ON)
option(RNDR_DEFAULT_LOGGER "Use default logger" ON)
option(RNDR_IMGUI "Include ImGUI library as a dependency that can be used to create UI features" ON)
option(RNDR_ASSIMP "Include assimp library as a dependency that can be used to import 3D models" ON)
option(RNDR_ETC2COMP "Include etc2comp library as a dependency that can be used to compress textures" ON)
option(RNDR_MESHOPTIMIZER "Include meshoptimizer library as a dependency that can be used to optimize meshes" ON)

message(STATUS "RNDR_OPENGL ${RNDR_OPENGL}")
message(STATUS "RNDR_SANITIZER ${RNDR_SANITIZER}")
message(STATUS "RNDR_TRACER ${RNDR_TRACER}")
message(STATUS "RNDR_DEFAULT_LOGGER ${RNDR_DEFAULT_LOGGER}")
message(STATUS "RNDR_IMGUI ${RNDR_IMGUI}")
message(STATUS "RNDR_ASSIMP ${RNDR_ASSIMP}")
message(STATUS "RNDR_ETC2COMP ${RNDR_ETC2COMP}")
message(STATUS "RNDR_MESHOPTIMIZER ${RNDR_MESHOPTIMIZER}")

if (NOT RNDR_OPENGL)
    set(RNDR_OPENGL ON)
endif ()

# Only do these if this is the main project, and not if it is included through add_subdirectory
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

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

    # Global compiler configuration
    if (MSVC)
        # Treat warnings as errors and highest warning level
        add_compile_options(/W4 /WX /wd4201)
        # Multiprocessor compilation
        add_compile_options(/MP)
        # Turn on address sanitizer unless its Release build
        if (RNDR_SANITIZER)
            add_compile_options($<$<NOT:$<CONFIG:Release>>:/fsanitize=address>)
        endif ()
    endif ()
    # Use C++ 20 and make it required
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
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
    add_test(NAME all_tests_no_window COMMAND rndr_test "~[window] ~[render-api]")

    # The executable code is here
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

endif ()
