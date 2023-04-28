# Collect all header files so that we can add them to the IDE later
#file(GLOB_RECURSE PUBLIC_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/*.h")
#file(GLOB_RECURSE PRIVATE_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*[.h|.c|.cpp]")
file(GLOB_RECURSE UNITY_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_BINARY_DIR}/src/*.cxx")
set(PUBLIC_SOURCE_LIST
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
    "${PROJECT_SOURCE_DIR}/include/rndr/render-api/render-api.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/utility/imgui-wrapper.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/utility/input-layout-builder.h")
set(PRIVATE_SOURCE_LIST
    "${PROJECT_SOURCE_DIR}/src/core/base.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/time.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/window.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/input.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/projection-camera.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/file.cpp"
    "${PROJECT_SOURCE_DIR}/src/render-api/opengl-helpers.h"
    "${PROJECT_SOURCE_DIR}/src/render-api/opengl-helpers.cpp"
    "${PROJECT_SOURCE_DIR}/src/utility/imgui-wrapper.cpp"
    "${PROJECT_SOURCE_DIR}/src/utility/input-layout-builder.cpp")
if (RNDR_DX11)
    set(DX11_SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/include/rndr/render-api/dx11-render-api.h"
        "${PROJECT_SOURCE_DIR}/src/render-api/dx11-render-api.cpp")
else ()
    set(DX11_SOURCE_LIST)
endif ()
if (RNDR_OPENGL)
    set(OPENGL_SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/include/rndr/render-api/opengl-render-api.h"
        "${PROJECT_SOURCE_DIR}/src/render-api/opengl-render-api.cpp")
else ()
    set(OPENGL_SOURCE_LIST)
endif ()

# Make an automatic library - will be static or dynamic based on user setting
add_library(rndr ${PRIVATE_SOURCE_LIST} ${PUBLIC_SOURCE_LIST} ${DX11_SOURCE_LIST} ${OPENGL_SOURCE_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(rndr PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(rndr PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Setup graphics API to use
if (RNDR_DX11)
    target_compile_definitions(rndr PUBLIC RNDR_DX11)
endif ()
if (RNDR_OPENGL)
    target_compile_definitions(rndr PUBLIC RNDR_OPENGL)
endif ()

if (RNDR_SPDLOG)
    target_compile_definitions(rndr PUBLIC RNDR_SPDLOG)
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
if (RNDR_DX11)
    target_link_libraries(rndr PUBLIC d3d11.lib dxgi.lib d3dcompiler.lib User32.lib)
endif ()
if (RNDR_OPENGL)
    target_link_libraries(rndr PUBLIC opengl32.lib User32.lib)
    target_link_libraries(rndr PUBLIC glad)
endif ()
if (RNDR_ASSIMP)
    target_compile_definitions(rndr PUBLIC RNDR_ASSIMP)
    target_link_libraries(rndr PUBLIC assimp)
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
if (RNDR_UNITY)
    source_group(
            TREE "${PROJECT_BINARY_DIR}/src/CMakeFiles/rndr.dir/Unity"
            PREFIX "Unity"
            FILES ${UNITY_SOURCE_LIST})
endif ()

# @formatter:off
target_compile_definitions(rndr PUBLIC RNDR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets")
# @formatter:on
