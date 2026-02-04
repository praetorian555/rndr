# Platform independent source and header files
set(SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/include/rndr/application.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/generic-window.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform-application.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/log.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/types.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/definitions.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/delegate.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/time.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/input-system.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/input-primitives.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/error-codes.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/projection-camera.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/windows-forward-def.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/opengl-forward-def.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/file.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/bitmap.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/material.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/renderers/renderer-base.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/renderers/shape-2d-renderer.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/renderers/shape-3d-renderer.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/renderers/grid-renderer.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/enum-flags.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/render-api.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/input-layout-builder.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/frames-per-second-counter.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/fly-camera.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/trace.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/projections.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/imgui-system.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/return-macros.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/shader-permutation.hpp"
        "${PROJECT_SOURCE_DIR}/src/log.cpp"
        "${PROJECT_SOURCE_DIR}/src/time.cpp"
        "${PROJECT_SOURCE_DIR}/src/input-system.cpp"
        "${PROJECT_SOURCE_DIR}/src/projection-camera.cpp"
        "${PROJECT_SOURCE_DIR}/src/file.cpp"
        "${PROJECT_SOURCE_DIR}/src/bitmap.cpp"
        "${PROJECT_SOURCE_DIR}/src/renderer-base.cpp"
        "${PROJECT_SOURCE_DIR}/src/shape-2d-renderer.cpp"
        "${PROJECT_SOURCE_DIR}/src/shape-3d-renderer.cpp"
        "${PROJECT_SOURCE_DIR}/src/grid-renderer.cpp"
        "${PROJECT_SOURCE_DIR}/src/input-layout-builder.cpp"
        "${PROJECT_SOURCE_DIR}/src/fly-camera.cpp"
        "${PROJECT_SOURCE_DIR}/src/frames-per-second-counter.cpp"
        "${PROJECT_SOURCE_DIR}/src/trace.cpp"
        "${PROJECT_SOURCE_DIR}/src/projections.cpp"
        "${PROJECT_SOURCE_DIR}/src/application.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform-application.cpp"
        "${PROJECT_SOURCE_DIR}/src/imgui-system.cpp"
        "${PROJECT_SOURCE_DIR}/src/material.cpp"
        "${PROJECT_SOURCE_DIR}/src/shader-permutation.cpp"
)

list(APPEND SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/opengl-graphics-context.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/opengl-pipeline.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/opengl-command-list.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/opengl-texture.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/opengl-buffer.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/opengl-shader.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/opengl-swap-chain.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/opengl-frame-buffer.hpp"
        "${PROJECT_SOURCE_DIR}/src/platform/opengl-helpers.hpp"
        "${PROJECT_SOURCE_DIR}/src/platform/opengl-graphics-context.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/opengl-pipeline.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/opengl-command-list.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/opengl-texture.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/opengl-buffer.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/opengl-shader.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/opengl-swap-chain.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/opengl-frame-buffer.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/opengl-helpers.cpp")

list(APPEND SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/extern/glad/src/glad.c"
        "${PROJECT_SOURCE_DIR}/extern/glad/src/glad_wgl.c"
        "${PROJECT_SOURCE_DIR}/extern/glad/include/glad/glad.h"
        "${PROJECT_SOURCE_DIR}/extern/glad/include/glad/glad_wgl.h"
        "${PROJECT_SOURCE_DIR}/extern/glad/include/KHR/khrplatform.h")

list(APPEND SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/extern/stb_image/include/stb_image/stb_image.h"
        "${PROJECT_SOURCE_DIR}/extern/stb_image/src/stb_image.cpp"
)

list(APPEND SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/windows-application.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/windows-window.hpp"
        "${PROJECT_SOURCE_DIR}/src/platform/windows-application.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform/windows-window.cpp"
)

if (RNDR_ADVANCED_API)
    list(APPEND SOURCE_LIST
            "${PROJECT_SOURCE_DIR}/include/rndr/advanced/graphics-context.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/advanced/device.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/advanced/physical-device.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/advanced/swap-chain.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/advanced/advanced-texture.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/advanced/advanced-buffer.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/advanced/synchronization.hpp"
            "${PROJECT_SOURCE_DIR}/src/advanced/graphics-context.cpp"
            "${PROJECT_SOURCE_DIR}/src/advanced/device.cpp"
            "${PROJECT_SOURCE_DIR}/src/advanced/physical-device.cpp"
            "${PROJECT_SOURCE_DIR}/src/advanced/swap-chain.cpp"
            "${PROJECT_SOURCE_DIR}/src/advanced/advanced-texture.cpp"
            "${PROJECT_SOURCE_DIR}/src/advanced/advanced-buffer.cpp"
            "${PROJECT_SOURCE_DIR}/src/advanced/synchronization.cpp"
            "${PROJECT_SOURCE_DIR}/src/volk-implementation.cpp"
            "${PROJECT_SOURCE_DIR}/src/vma-implementation.cpp")
endif ()

# Make an automatic library - will be static or dynamic based on user setting
add_library(rndr ${SOURCE_LIST})
target_link_libraries(rndr PRIVATE rndr_warnings)
target_link_libraries(rndr PRIVATE rndr_options)
target_compile_features(rndr PRIVATE cxx_std_20)

# We need this directory, and users of our library will need it too
target_include_directories(rndr PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_include_directories(rndr PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(rndr PRIVATE ${PROJECT_SOURCE_DIR}/extern/glad/include)
target_include_directories(rndr PRIVATE ${PROJECT_SOURCE_DIR}/extern/stb_image/include)

# Setup graphics API to use
target_compile_definitions(rndr PUBLIC RNDR_OPENGL)

# Setup library dependencies
target_link_libraries(rndr PUBLIC opengl32.lib User32.lib)
target_link_libraries(rndr PUBLIC opal imgui assimp)

if (RNDR_ADVANCED_API)
    target_include_directories(rndr PUBLIC $ENV{VULKAN_SDK}/Include)
#    find_library(Slang_LIBRARY NAMES slang HINTS "$ENV{VULKAN_SDK}/Lib" REQUIRED)
    target_link_libraries(rndr PUBLIC ktx)
    if (WIN32)
        target_compile_definitions(rndr PUBLIC -DVK_USE_PLATFORM_WIN32_KHR -DVK_NO_PROTOTYPES)
    endif ()
endif ()

# @formatter:off
target_compile_definitions(rndr PUBLIC RNDR_CORE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets")
# @formatter:on

## Generate the export header for rndr and attach it to the target
include(GenerateExportHeader)
generate_export_header(rndr EXPORT_FILE_NAME include/rndr/export.h)
target_compile_definitions(rndr PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:RNDR_STATIC_DEFINE>")
target_include_directories(rndr PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
target_include_directories(rndr PUBLIC "$<INSTALL_INTERFACE:include>")

# Install the library
install(TARGETS rndr
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}  # For Windows DLL files
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
        PATTERN "*.hpp")
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
        PATTERN "*.hpp")
