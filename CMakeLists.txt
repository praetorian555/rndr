cmake_minimum_required(VERSION 3.20)

project(
        RNDR
        VERSION 0.1
        DESCRIPTION "Example C++ Project using CMake"
        LANGUAGES CXX)

# Library options
option(RNDR_UNITY "Enable unity builds" OFF)
option(RNDR_SANITIZER "Enable address sanitizer" OFF)
option(RNDR_DX11 "Use DX11 API" OFF)
option(RNDR_SPDLOG "Use spdlog external library to implement default logging" ON)
option(RNDR_IMGUI "Add imgui wrapper which can be used for development GUI" ON)
option(RNDR_ASSIMP "Add wrapper around assimp library that can be used to import 3D models" ON)
option(RNDR_ETC2COMP "Add wrapper around etc2comp library that can be used to compress textures" ON)
option(RNDR_MESHOPTIMIZER "Add wrapper around meshoptimizer library that can be used to optimize meshes" ON)

message(STATUS "RNDR_UNITY ${RNDR_UNITY}")
message(STATUS "RNDR_SANITIZER ${RNDR_SANITIZER}")
message(STATUS "RNDR_DX11 ${RNDR_DX11}")
message(STATUS "RNDR_SPDLOG ${RNDR_SPDLOG}")
message(STATUS "RNDR_IMGUI ${RNDR_IMGUI}")
message(STATUS "RNDR_ASSIMP ${RNDR_ASSIMP}")
message(STATUS "RNDR_ETC2COMP ${RNDR_ETC2COMP}")
message(STATUS "RNDR_MESHOPTIMIZER ${RNDR_MESHOPTIMIZER}")

if (NOT RNDR_DX11)
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

# Enable unity build
if (RNDR_UNITY)
    set(CMAKE_UNITY_BUILD TRUE)
endif ()

include(FetchContent)

# Include spdlog only if it is not already present
if (NOT TARGET spdlog::spdlog AND RNDR_SPDLOG)
    FetchContent_Declare(
            spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog
            GIT_TAG ad0e89cbfb4d0c1ce4d097e134eb7be67baebb36 # release-1.11.0
    )
    FetchContent_MakeAvailable(spdlog)
    set_target_properties(spdlog PROPERTIES FOLDER Extern)
endif ()

# Include math only if it is not already present
if (NOT TARGET math)
    FetchContent_Declare(
            math
            GIT_REPOSITORY https://github.com/praetorian555/math
            GIT_TAG main
    )
    set(MATH_UNITY ${RNDR_UNITY} CACHE BOOL "" FORCE)
    set(MATH_SANITIZER ${RNDR_SANITIZER} CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(math)
    set_target_properties(math PROPERTIES FOLDER Extern)
endif ()

if (NOT TARGET imgui AND RNDR_IMGUI)
    FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui
            GIT_TAG d7c8516a4b848c0291e3d75b627c0843f515f591 # release-1.89.2
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

    if (RNDR_DX11)
        set(IMGUI_DX11_SOURCE_FILES
            ${SOURCE_PATH}/backends/imgui_impl_dx11.cpp
            ${SOURCE_PATH}/backends/imgui_impl_dx11.h)
        set(IMGUI_OPENGL_SOURCE_FILES)
    else ()
        set(IMGUI_DX11_SOURCE_FILES)
        set(IMGUI_OPENGL_SOURCE_FILES
            ${SOURCE_PATH}/backends/imgui_impl_opengl3.cpp
            ${SOURCE_PATH}/backends/imgui_impl_opengl3.h)
    endif ()

    add_library(imgui
                ${IMGUI_SOURCE_FILES}
                ${IMGUI_WINDOWS_SOURCE_FILES}
                ${IMGUI_DX11_SOURCE_FILES}
                ${IMGUI_OPENGL_SOURCE_FILES})

    target_include_directories(imgui PUBLIC ${SOURCE_PATH})

    set_target_properties(imgui PROPERTIES FOLDER Extern)
endif ()

if (NOT TARGET assimp AND RNDR_ASSIMP)
    FetchContent_Declare(
            assimp
            GIT_REPOSITORY https://github.com/assimp/assimp
            GIT_TAG 9519a62dd20799c5493c638d1ef5a6f484e5faf1 # release-5.2.5
    )
    set(ASSIMP_NO_EXPORT ON CACHE BOOL "")
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
    set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "")
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
    set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
    set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
    set(USE_STATIC_CRT OFF CACHE BOOL "")
    FetchContent_MakeAvailable(assimp)
    set_target_properties(assimp PROPERTIES FOLDER Extern)
    set_target_properties(assimp PROPERTIES UNITY_BUILD FALSE)
    set_target_properties(zlibstatic PROPERTIES FOLDER Extern)
    set_target_properties(zlibstatic PROPERTIES UNITY_BUILD FALSE)
    set_target_properties(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES FOLDER Extern)
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

if (NOT TARGET meshoptimizer AND RNDR_MESHOPTIMIZER)
    FetchContent_Declare(
            meshoptimizer
            GIT_REPOSITORY https://github.com/zeux/meshoptimizer
            GIT_TAG fbe26ff91317bd89c67c3890e038965d55f3e354 # release 0.19
    )
    FetchContent_MakeAvailable(meshoptimizer)
    set_target_properties(meshoptimizer PROPERTIES FOLDER Extern)
endif ()

# Adds external dependencies
add_subdirectory(extern)

# The compiled library code is here
add_subdirectory(src)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Add catch2 dependency
    if (NOT TARGET Catch2)
        FetchContent_Declare(
                Catch2
                GIT_REPOSITORY https://github.com/catchorg/Catch2
                GIT_TAG ab6c7375be9a8e71ee84c6f8537113f9f47daf99 # release-3.2.1
        )
        FetchContent_MakeAvailable(Catch2)
        set_target_properties(Catch2 PROPERTIES FOLDER Extern)
        set_target_properties(Catch2WithMain PROPERTIES FOLDER Extern)
    endif ()

    # Tests
    add_subdirectory(tests)
    enable_testing()
    add_test(NAME main COMMAND rndr_test)

    # The executable code is here
    add_subdirectory(examples/01-triangle)
    add_subdirectory(examples/02-cube)
    add_subdirectory(examples/03-image)
    add_subdirectory(examples/04-assimp)
    add_subdirectory(examples/05-mesh-optimizer)
    add_subdirectory(examples/06-vertex-pulling)

endif ()
