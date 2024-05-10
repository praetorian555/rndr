# Platform independent source and header files
set(SOURCE_LIST
    "${PROJECT_SOURCE_DIR}/include/rndr/rndr.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/core/types.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/core/definitions.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/core/base.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/core/delegate.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/core/containers/stack-array.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/core/time.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/core/window.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/core/input.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/core/input-primitives.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/core/error-codes.h"
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
    "${PROJECT_SOURCE_DIR}/include/rndr/utility/input-layout-builder.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/utility/cpu-tracer.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/utility/default-logger.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/utility/frames-per-second-counter.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/utility/line-renderer.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/utility/fly-camera.h"
    "${PROJECT_SOURCE_DIR}/include/rndr/utility/cube-map.h"
    "${PROJECT_SOURCE_DIR}/src/core/base.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/time.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/window.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/input.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/projection-camera.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/file.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/bitmap.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/renderer-base.cpp"
    "${PROJECT_SOURCE_DIR}/src/utility/input-layout-builder.cpp"
    "${PROJECT_SOURCE_DIR}/src/utility/frames-per-second-counter.cpp"
    "${PROJECT_SOURCE_DIR}/src/utility/line-renderer.cpp"
    "${PROJECT_SOURCE_DIR}/src/utility/fly-camera.cpp"
    "${PROJECT_SOURCE_DIR}/src/utility/cube-map.cpp")

list(APPEND SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-graphics-context.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-pipeline.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-command-list.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-image.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-buffer.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-shader.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-swap-chain.h"
        "${PROJECT_SOURCE_DIR}/include/rndr/core/platform/opengl-frame-buffer.h"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-helpers.h"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-graphics-context.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-pipeline.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-command-list.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-image.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-buffer.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-shader.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-swap-chain.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-frame-buffer.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/platform/opengl-helpers.cpp")

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
target_link_libraries(rndr PUBLIC math opal)

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
