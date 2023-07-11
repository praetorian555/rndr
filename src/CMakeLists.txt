# Platform independent source and header files
set(SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/include/rndr/rndr.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/definitions.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/base.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/delegate.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/span.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/array.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/stack-array.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/time.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/window.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/input.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/input-primitives.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/string.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/hash-map.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/ref.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/scope-ptr.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/projection-camera.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/forward-def-windows.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/forward-def-opengl.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/file.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/bitmap.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/renderer-base.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/render-api/render-api.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/imgui-wrapper.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/input-layout-builder.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/cpu-tracer.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/utility/default-logger.h"
        "${PROJECT_SOURCE_DIR}/src/core/base.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/time.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/window.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/input.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/projection-camera.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/file.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/bitmap.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/renderer-base.cpp"
        "${PROJECT_SOURCE_DIR}/src/render-api/opengl-helpers.h"
        "${PROJECT_SOURCE_DIR}/src/render-api/opengl-helpers.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/imgui-wrapper.cpp"
        "${PROJECT_SOURCE_DIR}/src/utility/input-layout-builder.cpp")

if (RNDR_OPENGL)
    list(APPEND SOURCE_LIST
            "${PROJECT_SOURCE_DIR}/include/rndr/render-api/opengl-render-api.h"
            "${PROJECT_SOURCE_DIR}/src/render-api/opengl-render-api.cpp")
endif ()

if (RNDR_TRACER)
    list(APPEND SOURCE_LIST "${PROJECT_SOURCE_DIR}/src/utility/cpu-tracer.cpp")
endif ()

if (RNDR_DEFAULT_LOGGER)
    list(APPEND SOURCE_LIST "${PROJECT_SOURCE_DIR}/src/utility/default-logger.cpp")
endif ()

# Make an automatic library - will be static or dynamic based on user setting
add_library(rndr ${SOURCE_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(rndr PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(rndr PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Setup graphics API to use
if (RNDR_OPENGL)
    target_compile_definitions(rndr PUBLIC RNDR_OPENGL)
endif ()

if (RNDR_TRACER)
    target_compile_definitions(rndr PUBLIC RNDR_TRACER)
    target_link_libraries(rndr PUBLIC spdlog::spdlog)
endif ()

if (RNDR_DEFAULT_LOGGER)
    target_compile_definitions(rndr PUBLIC RNDR_DEFAULT_LOGGER)
    target_link_libraries(rndr PUBLIC spdlog::spdlog)
endif ()

if (RNDR_IMGUI)
    target_compile_definitions(rndr PUBLIC RNDR_IMGUI)
    target_link_libraries(rndr PUBLIC imgui)
endif ()

# Setup library dependencies
target_link_libraries(rndr PUBLIC stb_image)
target_link_libraries(rndr PUBLIC stb_truetype)
target_link_libraries(rndr PUBLIC math)
if (RNDR_OPENGL)
    target_link_libraries(rndr PUBLIC opengl32.lib User32.lib)
    target_link_libraries(rndr PUBLIC glad)
endif ()
if (RNDR_ASSIMP)
    target_compile_definitions(rndr PUBLIC RNDR_ASSIMP)
    target_link_libraries(rndr PUBLIC assimp)
endif ()
if (RNDR_ETC2COMP)
    target_compile_definitions(rndr PUBLIC RNDR_ETC2COMP)
    target_link_libraries(rndr PUBLIC etc2comp)
endif ()
if (RNDR_MESHOPTIMIZER)
    target_compile_definitions(rndr PUBLIC RNDR_MESHOPTIMIZER)
    target_link_libraries(rndr PUBLIC meshoptimizer)
endif ()

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
target_compile_definitions(rndr PUBLIC RNDR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets")
# @formatter:on
