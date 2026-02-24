cmake_minimum_required(VERSION 3.28)
project(RNDR VERSION 0.1.0 LANGUAGES CXX C)

# Library options
option(RNDR_ADVANCED_API "Use more advanced rendering API that is more powerful but more complicated" ON)
option(RNDR_SHARED_LIBS "Build as a shared library" OFF)
option(RNDR_HARDENING "Enable hardened mode" ON)
option(RNDR_BUILD_TESTS "Build tests" ON)
option(RNDR_BUILD_SAMPLES "Builds sample executables" ON)

message(STATUS "RNDR_ADVANCED_API ${RNDR_ADVANCED_API}")
message(STATUS "RNDR_BUILD_TESTS ${RNDR_BUILD_TESTS}")
message(STATUS "RNDR_SHARED_LIBS ${RNDR_SHARED_LIBS}")
message(STATUS "RNDR_HARDENING ${RNDR_HARDENING}")
message(STATUS "RNDR_BUILD_SAMPLES ${RNDR_BUILD_SAMPLES}")

if (RNDR_ADVANCED_API)
    message(WARNING "Advanced API is still in development so library might not compile...")
endif ()

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
message(STATUS "***** Setting up Opal Dependency *****")
SET(OPAL_BUILD_TESTS OFF)
SET(OPAL_HARDENING ${RNDR_HARDENING})
set(OPAL_SHARED_LIBS ${RNDR_SHARED_LIBS})
FetchContent_Declare(
        opal
        GIT_REPOSITORY https://github.com/praetorian555/opal
        GIT_TAG main
)
FetchContent_MakeAvailable(opal)
message(STATUS "***** Setup Complete *****")

message(STATUS "***** Setting up Obsidian Dependency *****")
SET(OBS_BUILD_TESTS OFF)
SET(OBS_HARDENING ${RNDR_HARDENING})
set(OBS_LLVM_PATH "F:/LLVM")
FetchContent_Declare(
        obsidian
        GIT_REPOSITORY https://github.com/praetorian555/obsidian
        GIT_TAG main
)
FetchContent_MakeAvailable(obsidian)
message(STATUS "***** Setup Complete *****")

function(get_include_directories OUT_GENERATOR TARGET)
    set(${OUT_GENERATOR} $<JOIN:$<TARGET_PROPERTY:${TARGET},INCLUDE_DIRECTORIES>,,> PARENT_SCOPE)
endfunction()
function(get_definitions OUT_GENERATOR TARGET)
    set(${OUT_GENERATOR} $<JOIN:$<LIST:TRANSFORM,$<TARGET_PROPERTY:${TARGET},COMPILE_DEFINITIONS>,PREPEND,-D>,,> PARENT_SCOPE)
endfunction()
get_include_directories(INCLUDE_DIRECTORIES rndr)
get_definitions(DEFINITIONS rndr)
add_custom_target(
        generate_reflection
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/include/generated
        COMMAND $<TARGET_FILE:obsidian>
        input-dir=${CMAKE_CURRENT_SOURCE_DIR}/include
        output-dir=${CMAKE_CURRENT_BINARY_DIR}/include/generated
        std=c++20
        compile-options="${DEFINITIONS},-Wall,-Wextra"
        inc-dirs=${INCLUDE_DIRECTORIES}
        DEPENDS obsidian
)

# Setup IMGUI ######################################################################
message(STATUS "***** Setting up ImGui Dependency *****")
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
message(STATUS "***** Setup Complete *****")

# Setup Assimp ######################################################################
message(STATUS "***** Setting up Assimp Dependency *****")
set(ASSIMP_ASAN OFF)
set(BUILD_SHARED_LIBS OFF)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_NO_EXPORT ON)
set(ASSIMP_INSTALL_PDB OFF)
set(ASSIMP_BUILD_ZLIB ON)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_OBJ_IMPORTER ON)
set(ASSIMP_BUILD_GLTF_IMPORTER ON)
set(ASSIMP_BUILD_FBX_IMPORTER OFF)
set(ASSIMP_BUILD_COLLADA_IMPORTER OFF)
FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG "v6.0.2"
)
FetchContent_MakeAvailable(assimp)
message(STATUS "***** Setup Complete *****")

if (RNDR_ADVANCED_API)
    if (NOT IS_DIRECTORY $ENV{VULKAN_SDK})
        message(WARNING "Vulkan SDK not detected!")
    else ()
        message(STATUS "Vulkan SDK found at: $ENV{VULKAN_SDK}")
    endif ()

    message(STATUS "***** Setting up KTX Software Dependency *****")
    FetchContent_Declare(
            ktx
            GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
            GIT_TAG "v4.4.2"
    )
    FetchContent_MakeAvailable(ktx)
    message(STATUS "***** Setup Complete *****")
endif ()

# The compiled library code is here
add_subdirectory(src)

if (RNDR_BUILD_TESTS)
    set(RNDR_TEST_FILES
            test/base-test.cpp
            test/bitmap-test.cpp
            test/camera-test.cpp
            test/frames-per-second-counter-test.cpp
            test/input-test.cpp
            test/renderer-base-test.cpp
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
    add_test(NAME all_tests_no_window_no_render_api COMMAND rndr_test)

    # Install the test executable
    install(TARGETS rndr_test
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}  # For Windows DLL files
    )
endif ()

# Compile Slang shaders to SPIR-V
file(GLOB SLANG_SHADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.slang")
set(SLANG_SPIRV_OUTPUTS)
foreach (SLANG_FILE ${SLANG_SHADER_FILES})
    get_filename_component(SLANG_NAME ${SLANG_FILE} NAME_WE)
    set(SPIRV_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/${SLANG_NAME}.spv")
    add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND slangc ${SLANG_FILE} -g -target spirv -o ${SPIRV_OUTPUT} -matrix-layout-row-major -warnings-as-errors all
            DEPENDS ${SLANG_FILE}
            COMMENT "Compiling ${SLANG_NAME}.slang to SPIR-V..."
    )
    list(APPEND SLANG_SPIRV_OUTPUTS ${SPIRV_OUTPUT})
endforeach ()
add_custom_target(compile_shaders DEPENDS ${SLANG_SPIRV_OUTPUTS})

if (RNDR_BUILD_SAMPLES)
    add_library(shared samples/shared/types.hpp samples/shared/example-controller.h samples/shared/example-controller.cpp)
    target_include_directories(shared PUBLIC samples/shared)
    # @formatter:off
    target_compile_definitions(shared PUBLIC RNDR_SAMPLES_SHADERS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/samples/shared/shaders")
    # @formatter:on
    add_dependencies(shared compile_shaders)
    target_link_libraries(shared PUBLIC rndr opal)

    add_executable(window-sample samples/window/window-sample.cpp)
    target_link_libraries(window-sample PUBLIC shared imgui)

    add_executable(text-rendering-sample samples/text-rendering/text-rendering-sample.cpp samples/extern/stb_truetype/src/stb_truetype.cpp
            samples/text-rendering/bitmap-text-renderer.hpp samples/text-rendering/bitmap-text-renderer.cpp)
    target_include_directories(text-rendering-sample PRIVATE samples/extern/stb_truetype/include)
    target_link_libraries(text-rendering-sample PUBLIC shared imgui)

    add_executable(modern-vulkan samples/modern-vulkan/modern-vulkan.cpp)
    target_link_libraries(modern-vulkan shared imgui)

endif ()

