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

include(FetchContent)

if (NOT TARGET imgui AND RNDR_IMGUI)
    FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui
            GIT_TAG c6e0284ac58b3f205c95365478888f7b53b077e2 # release-1.89.9
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

    if (MSVC)
        set(IMGUI_WINDOWS_SOURCE_FILES
            ${SOURCE_PATH}/backends/imgui_impl_win32.cpp
            ${SOURCE_PATH}/backends/imgui_impl_win32.h)
    else ()
        set(IMGUI_WINDOWS_SOURCE_FILES)
    endif ()

    if (RNDR_OPENGL)
        set(IMGUI_OPENGL_SOURCE_FILES
            ${SOURCE_PATH}/backends/imgui_impl_opengl3.cpp
            ${SOURCE_PATH}/backends/imgui_impl_opengl3.h include/rndr/utility/default-logger.h)
    endif ()

    add_library(imgui
                ${IMGUI_SOURCE_FILES}
                ${IMGUI_WINDOWS_SOURCE_FILES}
                ${IMGUI_OPENGL_SOURCE_FILES})

    target_include_directories(imgui PUBLIC ${SOURCE_PATH})

    set_target_properties(imgui PROPERTIES FOLDER Extern)
endif ()

if (NOT TARGET etc2comp AND RNDR_ETC2COMP)
    FetchContent_Declare(
            etc2comp
            GIT_REPOSITORY https://github.com/google/etc2comp
            GIT_TAG 39422c1aa2f4889d636db5790af1d0be6ff3a226
    )
    FetchContent_Populate(etc2comp)
    FetchContent_GetProperties(etc2comp SOURCE_DIR ETC2COMP_SOURCE_PATH)
    file(GLOB_RECURSE ETC2COMP_SOURCES CONFIGURE_DEPENDS
         ${ETC2COMP_SOURCE_PATH}/EtcLib/Etc/*.h
         ${ETC2COMP_SOURCE_PATH}/EtcLib/EtcCodec/*.h
         ${ETC2COMP_SOURCE_PATH}/EtcLib/Etc/*.cpp
         ${ETC2COMP_SOURCE_PATH}/EtcLib/EtcCodec/*.cpp)
    message(STATUS "ETC2COMP_SOURCES: ${ETC2COMP_SOURCES}")
    add_library(etc2comp ${ETC2COMP_SOURCES}
                ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFile.h
                ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFile.cpp
                ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFileHeader.h
                ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFileHeader.cpp)
    target_include_directories(etc2comp PUBLIC ${ETC2COMP_SOURCE_PATH})
    target_include_directories(etc2comp PUBLIC ${ETC2COMP_SOURCE_PATH}/EtcLib/Etc
                               ${ETC2COMP_SOURCE_PATH}/EtcLib/EtcCodec
                               ${ETC2COMP_SOURCE_PATH}/EtcTool)
    target_compile_options(etc2comp PRIVATE /W1)
    set_target_properties(etc2comp PROPERTIES FOLDER Extern)
endif ()

FetchContent_Declare(
        gli
        GIT_REPOSITORY https://github.com/g-truc/gli
        GIT_TAG 30808550a20ca53a255e6e1e77070493eda7b736 # 0.8.2
)
FetchContent_Populate(gli)
FetchContent_GetProperties(gli SOURCE_DIR GLI_SOURCE_PATH)
add_library(gli INTERFACE)
target_include_directories(gli INTERFACE ${GLI_SOURCE_PATH})
target_include_directories(gli INTERFACE ${GLI_SOURCE_PATH}/external/glm)
set_target_properties(gli PROPERTIES FOLDER Extern)

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
