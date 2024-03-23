# Platform independent source and header files
set(SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/include/rndr/rndr.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/definitions.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/base.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/delegate.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/containers/span.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/containers/array.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/containers/stack-array.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/time.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/window.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/input.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/input-primitives.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/containers/string.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/containers/hash-map.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/containers/ref.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/containers/scope-ptr.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/projection-camera.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/windows-forward-def.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-forward-def.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/file.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/bitmap.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/renderer-base.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/enum-flags.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/render-api.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/imgui-wrapper.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/input-layout-builder.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/cpu-tracer.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/default-logger.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/frames-per-second-counter.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/line-renderer.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/fly-camera.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/mesh.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/material.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/cube-map.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/assimp-helpers.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/scene.h"
        "${PROJECT_SOURCE_DIR}/src/core/base.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/time.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/window.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/input.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/projection-camera.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/file.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/bitmap.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/renderer-base.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/imgui-wrapper.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/input-layout-builder.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/frames-per-second-counter.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/line-renderer.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/fly-camera.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/mesh.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/material.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/cube-map.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/assimp-helpers.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/scene.cpp")

list(APPEND SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-graphics-context.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-pipeline.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-command-list.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-image.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-buffer.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-shader.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-swap-chain.h"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-helpers.h"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-graphics-context.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-pipeline.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-command-list.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-image.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-buffer.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-shader.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-swap-chain.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-helpers.cpp")

if (RNDR_TRACER)
    list(APPEND SOURCE_LIST "${PROJECT_SOURCE_DIR}/src/utility/cpu-tracer.cpp")
endif ()

if (RNDR_DEFAULT_LOGGER)
    list(APPEND SOURCE_LIST "${PROJECT_SOURCE_DIR}/src/utility/default-logger.cpp")
endif ()

# Make an automatic library - will be static or dynamic based on user setting
add_library(rndr ${SOURCE_LIST})
target_link_libraries(rndr PRIVATE rndr_warnings)
target_link_libraries(rndr PRIVATE rndr_options)

# We need this directory, and users of our library will need it too
target_include_directories(rndr PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(rndr PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Setup graphics API to use
target_compile_definitions(rndr PUBLIC RNDR_OPENGL)

if (RNDR_TRACER)
    target_compile_definitions(rndr PUBLIC RNDR_TRACER)
    target_link_libraries(rndr PUBLIC spdlog::spdlog)
endif ()

if (RNDR_DEFAULT_LOGGER)
    target_compile_definitions(rndr PUBLIC RNDR_DEFAULT_LOGGER)
    target_link_libraries(rndr PUBLIC spdlog::spdlog)
endif ()

# Setup library dependencies
target_link_libraries(rndr PUBLIC stb_image)
target_link_libraries(rndr PUBLIC stb_truetype)
target_link_libraries(rndr PUBLIC math)
target_compile_definitions(rndr PUBLIC RNDR_IMGUI)
target_link_libraries(rndr PUBLIC imgui)
target_link_libraries(rndr PUBLIC opengl32.lib User32.lib)
target_link_libraries(rndr PUBLIC glad)
target_compile_definitions(rndr PUBLIC RNDR_ASSIMP)
target_link_libraries(rndr PUBLIC assimp)
target_compile_definitions(rndr PUBLIC RNDR_ETC2COMP)
target_link_libraries(rndr PUBLIC etc2comp)
target_compile_definitions(rndr PUBLIC RNDR_MESHOPTIMIZER)
target_link_libraries(rndr PUBLIC meshoptimizer)
target_link_libraries(rndr PUBLIC gli)

# IDEs should put the headers in a nice place
set_target_properties(rndr PROPERTIES FOLDER RNDR)
source_group(
        TREE "${PROJECT_SOURCE_DIR}/include"
        PREFIX "API"
        FILES ${PUBLIC_SOURCE_LIST})
source_group(
        TREE "${PROJECT_SOURCE_DIR}/src"
        PREFIX "Implementation"
        FILES ${PRIVATE_SOURCE_LIST})

# @formatter:off
target_compile_definitions(rndr PUBLIC RNDR_CORE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets")
# @formatter:on
