# Platform independent source and header files
set(SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/include/rndr/application.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/generic-window.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform-application.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/log.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/types.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/monitor-info.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/definitions.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/time.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/input-system.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/input-primitives.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/error-codes.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/projection-camera.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/platform/windows-forward-def.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/file.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/bitmap.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/frames-per-second-counter.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/fly-camera.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/trace.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/projections.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/return-macros.hpp"
        "${PROJECT_SOURCE_DIR}/include/rndr/pixel-format.hpp"
        "${PROJECT_SOURCE_DIR}/src/time.cpp"
        "${PROJECT_SOURCE_DIR}/src/input-system.cpp"
        "${PROJECT_SOURCE_DIR}/src/projection-camera.cpp"
        "${PROJECT_SOURCE_DIR}/src/file.cpp"
        "${PROJECT_SOURCE_DIR}/src/bitmap.cpp"
        "${PROJECT_SOURCE_DIR}/src/fly-camera.cpp"
        "${PROJECT_SOURCE_DIR}/src/frames-per-second-counter.cpp"
        "${PROJECT_SOURCE_DIR}/src/trace.cpp"
        "${PROJECT_SOURCE_DIR}/src/projections.cpp"
        "${PROJECT_SOURCE_DIR}/src/application.cpp"
        "${PROJECT_SOURCE_DIR}/src/platform-application.cpp"
        "${PROJECT_SOURCE_DIR}/src/pixel-format.cpp"
        # Canvas-namespaced but pure math with no GL dependency, and projection-camera.cpp
        # (always built) calls it, so it cannot be gated on RNDR_CANVAS.
        "${PROJECT_SOURCE_DIR}/include/rndr/canvas/projections.hpp"
        "${PROJECT_SOURCE_DIR}/src/canvas/projections.cpp"
)

if (${RNDR_CANVAS} OR ${RNDR_FORGE})
    list(APPEND SOURCE_LIST
            "${PROJECT_SOURCE_DIR}/include/rndr/core/shader-compiler.hpp"
            "${PROJECT_SOURCE_DIR}/src/core/shader-compiler.cpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/core/shader-cache.hpp"
            "${PROJECT_SOURCE_DIR}/src/core/shader-cache.cpp")
endif()

if (${RNDR_CANVAS})
    list(APPEND SOURCE_LIST
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/canvas.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/gl-result.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/context.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/shader.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/render-target.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/texture.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/brush.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/vertex-layout.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/mesh.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/buffer.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/draw-command-buffer.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/draw-list.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/compute-list.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/timestamp-query.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/bitmap.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/renderers/shape-renderer.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/renderers/bitmap-text-renderer.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/renderers/cubemap-renderer.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/renderers/pbr-renderer.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/canvas/renderers/grid-renderer.hpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/context.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/buffer.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/texture.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/render-target.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/shader.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/vertex-layout.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/mesh.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/brush.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/draw-list.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/timestamp-query.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/bitmap.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/shape-renderer.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/bitmap-text-renderer.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/cubemap-renderer.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/pbr-renderer.cpp"
            "${PROJECT_SOURCE_DIR}/src/canvas/grid-renderer.cpp")
endif ()

# glad is only reached from Canvas and the Win32 platform layer, both Windows-only.
if (WIN32)
    list(APPEND SOURCE_LIST
            "${PROJECT_SOURCE_DIR}/extern/glad/src/glad.c"
            "${PROJECT_SOURCE_DIR}/extern/glad/src/glad_wgl.c"
            "${PROJECT_SOURCE_DIR}/extern/glad/include/glad/glad.h"
            "${PROJECT_SOURCE_DIR}/extern/glad/include/glad/glad_wgl.h"
            "${PROJECT_SOURCE_DIR}/extern/glad/include/KHR/khrplatform.h")
endif ()

list(APPEND SOURCE_LIST
        "${PROJECT_SOURCE_DIR}/extern/stb_image/include/stb_image/stb_image.h"
        "${PROJECT_SOURCE_DIR}/extern/stb_image/src/stb_image.cpp"
)

if (${RNDR_AUDIO})
    list(APPEND SOURCE_LIST
            "${PROJECT_SOURCE_DIR}/extern/stb_vorbis/include/stb_vorbis/stb_vorbis.h"
            "${PROJECT_SOURCE_DIR}/extern/stb_vorbis/src/stb_vorbis.cpp")
    list(APPEND SOURCE_LIST
            "${PROJECT_SOURCE_DIR}/include/rndr/audio/audio-types.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/audio/audio-clip.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/audio/audio-mixer.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/audio/audio-device.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/audio/audio-system.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/platform/windows-audio-device.hpp"
            "${PROJECT_SOURCE_DIR}/src/audio/audio-clip.cpp"
            "${PROJECT_SOURCE_DIR}/src/audio/audio-decoders.cpp"
            "${PROJECT_SOURCE_DIR}/src/audio/audio-mixer.cpp"
            "${PROJECT_SOURCE_DIR}/src/audio/audio-device.cpp"
            "${PROJECT_SOURCE_DIR}/src/audio/audio-system.cpp"
            "${PROJECT_SOURCE_DIR}/src/platform/windows-audio-device.cpp")
endif ()

if (WIN32)
    list(APPEND SOURCE_LIST
            "${PROJECT_SOURCE_DIR}/include/rndr/platform/windows-application.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/platform/windows-gamepad.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/platform/windows-window.hpp"
            "${PROJECT_SOURCE_DIR}/src/platform/windows-application.cpp"
            "${PROJECT_SOURCE_DIR}/src/platform/windows-gamepad.cpp"
            "${PROJECT_SOURCE_DIR}/src/platform/windows-window.cpp"
            # imgui-system wraps the Win32 ImGui backend (imgui_impl_win32.h), so it is
            # a Windows platform source until a Linux backend exists.
            "${PROJECT_SOURCE_DIR}/include/rndr/imgui-system.hpp"
            "${PROJECT_SOURCE_DIR}/src/imgui-system.cpp"
    )
elseif (UNIX)
    list(APPEND SOURCE_LIST
            "${PROJECT_SOURCE_DIR}/include/rndr/platform/linux-application.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/platform/linux-window.hpp"
            "${PROJECT_SOURCE_DIR}/src/platform/linux-application.cpp"
            "${PROJECT_SOURCE_DIR}/src/platform/linux-window.cpp"
    )
endif ()

if (${RNDR_FORGE})
    list(APPEND SOURCE_LIST
            "${spirv-reflect_SOURCE_DIR}/spirv_reflect.h"
            "${spirv-reflect_SOURCE_DIR}/spirv_reflect.c")
    list(APPEND SOURCE_LIST
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/forward.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/types.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/graphics-context.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/device.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/physical-device.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/swap-chain.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/texture.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/buffer.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/synchronization.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/command-buffer.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/transfer.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/debug.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/query.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/frame-context.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/descriptor-set.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/shader.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/mesh.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/pipeline.hpp"
            "${PROJECT_SOURCE_DIR}/include/rndr/forge/vulkan-result.hpp"
            "${PROJECT_SOURCE_DIR}/src/forge/graphics-context.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/device.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/physical-device.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/swap-chain.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/texture.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/buffer.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/synchronization.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/command-buffer.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/transfer.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/debug.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/query.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/frame-context.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/descriptor-set.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/shader.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/mesh.cpp"
            "${PROJECT_SOURCE_DIR}/src/forge/pipeline.cpp"
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
# glad and stb_image are used only inside rndr's implementation (no public header includes
# them), so they are wrapped in BUILD_INTERFACE and not shipped with the installed package.
# SYSTEM keeps these vendored headers out of the -Werror surface on GCC/Clang.
target_include_directories(rndr SYSTEM PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/extern/glad/include>)
target_include_directories(rndr SYSTEM PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/extern/stb_image/include>)

if (${RNDR_FORGE} OR ${RNDR_CANVAS})
    target_link_libraries(rndr PRIVATE slang)
endif ()

if (${RNDR_CANVAS})
    target_link_libraries(rndr PRIVATE opengl32.lib)
    target_compile_definitions(rndr PUBLIC RNDR_CANVAS)
    # stb_truetype.h is included by the public bitmap-text-renderer.hpp, so it is shipped with
    # the package (its headers install into <prefix>/include alongside rndr's own).
    target_include_directories(rndr PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/samples/extern/stb_truetype/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_sources(rndr PRIVATE ${PROJECT_SOURCE_DIR}/samples/extern/stb_truetype/src/stb_truetype.cpp)
endif ()
if (${RNDR_FORGE})
    target_compile_definitions(rndr PUBLIC RNDR_FORGE)
endif ()
if (${RNDR_AUDIO})
    target_compile_definitions(rndr PUBLIC RNDR_AUDIO)
    # stb_vorbis is only included from rndr's implementation, so like stb_image it is build-only.
    target_include_directories(rndr PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/extern/stb_vorbis/include>)
    # WASAPI is reached through COM (Ole32) and the audio thread registers with MMCSS (Avrt). Both are part of
    # every supported Windows, so nothing needs redistributing.
    target_link_libraries(rndr PRIVATE Ole32.lib Avrt.lib)
endif ()

if (WIN32)
    # Xinput.lib resolves to XInput 1.4 in the Windows 10 SDK, which ships with every supported
    # Windows version, so there is no runtime redistributable to worry about.
    target_link_libraries(rndr PRIVATE User32.lib Shcore.lib Xinput.lib)
elseif (UNIX)
    # The X11/XCB stack found by pkg_check_modules in cmake/dependencies.cmake.
    target_link_libraries(rndr PRIVATE PkgConfig::RNDR_XCB)
endif ()
target_link_libraries(rndr PUBLIC opal imgui)
if (RNDR_ASSIMP)
    target_link_libraries(rndr PRIVATE assimp)
    target_compile_definitions(rndr PRIVATE RNDR_ASSIMP)
endif ()

if (RNDR_KTX)
    target_link_libraries(rndr PRIVATE ktx)
    target_compile_definitions(rndr PRIVATE RNDR_KTX)
endif ()

if (${RNDR_FORGE})
    # SYSTEM on both: the SDK headers (VMA in particular) do not survive this project's warning
    # level, and SPIRV-Reflect is Khronos source rather than ours, so it is compiled with
    # warnings off instead of being edited.
    target_include_directories(rndr SYSTEM PRIVATE ${spirv-reflect_SOURCE_DIR})
    if (NOT MSVC)
        set_source_files_properties(${spirv-reflect_SOURCE_DIR}/spirv_reflect.c
                PROPERTIES COMPILE_OPTIONS "-w")
    endif ()
    # VMA is only touched from src/, so it stays PRIVATE; the Vulkan headers and volk leak
    # through the public Forge headers. Consumers of an installed FORGE-enabled rndr provide
    # their own copies, so none of these paths are baked into the install interface.
    target_include_directories(rndr SYSTEM PRIVATE ${vma_SOURCE_DIR}/include)
    target_include_directories(rndr SYSTEM PUBLIC
            $<BUILD_INTERFACE:${vulkan-headers_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${volk_SOURCE_DIR}>)
    target_compile_definitions(rndr PUBLIC RNDR_FORGE)
    if (WIN32)
        target_compile_definitions(rndr PUBLIC -DVK_USE_PLATFORM_WIN32_KHR -DVK_NO_PROTOTYPES)
    elseif (UNIX)
        target_compile_definitions(rndr PUBLIC -DVK_USE_PLATFORM_XCB_KHR -DVK_NO_PROTOTYPES)
    endif ()
    # Public, because whether the layer is there decides what a caller sees: without it the context reports
    # no debug messages at all, so a test that asserts on them asserts on nothing and cannot tell the two
    # builds apart from the inside.
    if (${RNDR_FORGE_VALIDATION})
        target_compile_definitions(rndr PUBLIC RNDR_FORGE_VALIDATION)
    endif ()
endif ()

# @formatter:off
target_compile_definitions(rndr PUBLIC RNDR_CORE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets")
# @formatter:on

# Version info as compile-time defines (derived from the git tag, see cmake/version.cmake).
# PUBLIC so consumers of an installed rndr can query the version they built against. The numeric
# components come from project(RNDR VERSION ...); RNDR_VERSION_FULL carries the full git describe.
target_compile_definitions(rndr PUBLIC
        RNDR_VERSION_MAJOR=${RNDR_VERSION_MAJOR}
        RNDR_VERSION_MINOR=${RNDR_VERSION_MINOR}
        RNDR_VERSION_PATCH=${RNDR_VERSION_PATCH}
        RNDR_VERSION_STRING="${RNDR_VERSION}"
        RNDR_VERSION_FULL="${RNDR_VERSION_FULL}")

## Generate the export header for rndr and attach it to the target
include(GenerateExportHeader)
generate_export_header(rndr EXPORT_FILE_NAME include/rndr/export.h)
target_compile_definitions(rndr PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:RNDR_STATIC_DEFINE>")
target_include_directories(rndr PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
target_include_directories(rndr PUBLIC "$<INSTALL_INTERFACE:include>")

################################################################################################
# Install / package configuration
#
# Installs rndr as a find_package()-able CMake package: the library, its public headers, the
# Slang runtime it needs, and a relocatable rndrConfig.cmake. The bundled dependencies that a
# static rndr propagates to consumers (opal, imgui, assimp, ktx, slang) are handled as follows:
#   * imgui      - in-tree target we own, exported in the rndrTargets set below.
#   * opal       - installed via its own export; pulled in by find_dependency(opal) in the config.
#   * assimp/ktx - same, behind RNDR_ASSIMP / RNDR_KTX.
#   * slang      - a prebuilt imported target, re-created by rndrConfig.cmake from the shipped
#                  import library and runtime DLLs.
################################################################################################
include(CMakePackageConfigHelpers)

set(RNDR_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/rndr)

# Hardened builds are AddressSanitizer-instrumented and are not meant to be distributed.
# Fail the install (at install time, not configure time) so such a build can't be installed.
if (RNDR_HARDENING)
    install(CODE [[message(FATAL_ERROR
            "rndr was configured with RNDR_HARDENING=ON (AddressSanitizer-instrumented build), \
which must not be installed. Reconfigure with -DRNDR_HARDENING=OFF before installing.")]])
endif ()

# Export rndr together with the in-tree targets it links: imgui (a public dependency) and the
# rndr_warnings / rndr_options interface libraries it carries as private link usage.
install(TARGETS rndr imgui rndr_warnings rndr_options
        EXPORT rndrTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}  # For Windows DLL files
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Public headers (note: CMAKE_CURRENT_SOURCE_DIR here is the src/ dir, so the headers
# live one level up under the project root).
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
        PATTERN "*.hpp")
# Generated headers (rndr/export.h) produced by generate_export_header into the build tree.
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
        PATTERN "*.hpp")
# stb_truetype.h is pulled in by the public bitmap-text-renderer.hpp, so ship it too.
if (${RNDR_CANVAS})
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/samples/extern/stb_truetype/include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.h")
endif ()

# Install the Slang runtime DLLs rndr pulls in into the bin directory, and the import library
# into the lib directory, so the install prefix is self-contained: any executable linking the
# installed rndr can be both linked and launched. Mirrors rndr_deploy_runtime(), which performs
# the equivalent copy for build-tree executables.
# (The ASan runtime is intentionally not installed - hardened builds are blocked above.)
if (WIN32 AND DEFINED SLANG_RUNTIME_DIR)
    install(DIRECTORY ${SLANG_RUNTIME_DIR}/
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            FILES_MATCHING PATTERN "*.dll")
endif ()
if (WIN32 AND DEFINED SLANG_IMPLIB)
    install(FILES ${SLANG_IMPLIB} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif ()

# Install the export set (creates rndr::rndr and the namespaced in-tree targets).
install(EXPORT rndrTargets
        FILE rndrTargets.cmake
        NAMESPACE rndr::
        DESTINATION ${RNDR_INSTALL_CMAKEDIR})

# Generate rndrConfig.cmake. The find_dependency() calls for the optional dependencies are
# injected based on the build-time options so the config only requires what was actually built.
set(RNDR_CONFIG_FIND_ASSIMP "")
set(RNDR_CONFIG_FIND_KTX "")
if (RNDR_ASSIMP)
    set(RNDR_CONFIG_FIND_ASSIMP "find_dependency(assimp)")
endif ()
if (RNDR_KTX)
    set(RNDR_CONFIG_FIND_KTX "find_dependency(Ktx)")
endif ()

configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/rndr-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/rndrConfig.cmake
        INSTALL_DESTINATION ${RNDR_INSTALL_CMAKEDIR})

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/rndrConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/rndrConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/rndrConfigVersion.cmake
        DESTINATION ${RNDR_INSTALL_CMAKEDIR})
