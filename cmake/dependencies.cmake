include(cmake/cpm.cmake)

if (NOT TARGET opal)
    message(STATUS "***** Setting up Opal Dependency *****")
    # Not EXCLUDE_FROM_ALL: opal is a public dependency of rndr and its own install/export
    # rules must run so `find_package(rndr)` can locate it via find_dependency(opal).
    cpmaddpackage(
            NAME opal
            GIT_REPOSITORY https://github.com/praetorian555/opal
            GIT_TAG opal-0.6.4
            OPTIONS
            "OPAL_BUILD_TESTS OFF"
            "OPAL_HARDENING ${RNDR_HARDENING}"
            "OPAL_SHARED_LIBS ${RNDR_SHARED_LIBS}"
            "OPAL_EXCEPTIONS OFF"
    )
    message(STATUS "***** Setup Complete *****")
endif ()

# Setup IMGUI ######################################################################
message(STATUS "***** Setting up ImGui Dependency *****")
cpmaddpackage(
        NAME imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG f5befd2d29e66809cd1110a152e375a7f1981f06 # release-1.91.9b
        DOWNLOAD_ONLY YES
)
set(SOURCE_PATH ${imgui_SOURCE_DIR})
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
# Wrap in BUILD_INTERFACE so imgui can be added to rndr's install export set without leaking
# this build-tree path. No public rndr header includes imgui, so no INSTALL_INTERFACE is needed.
target_include_directories(imgui PUBLIC $<BUILD_INTERFACE:${SOURCE_PATH}>)
message(STATUS "***** Setup Complete *****")

# Setup Assimp ######################################################################
if (RNDR_ASSIMP)
    message(STATUS "***** Setting up Assimp Dependency *****")
    # Not EXCLUDE_FROM_ALL: assimp's own install/export rules must run so find_dependency(assimp)
    # can locate it from the rndr package.
    cpmaddpackage(
            NAME assimp
            GIT_REPOSITORY https://github.com/assimp/assimp.git
            GIT_TAG "v6.0.2"
            OPTIONS
            "ASSIMP_ASAN OFF"
            "BUILD_SHARED_LIBS OFF"
            "ASSIMP_BUILD_ASSIMP_TOOLS OFF"
            "ASSIMP_BUILD_TESTS OFF"
            "ASSIMP_NO_EXPORT ON"
            "ASSIMP_INSTALL_PDB OFF"
            "ASSIMP_BUILD_ZLIB ON"
            "ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF"
            "ASSIMP_BUILD_OBJ_IMPORTER ON"
            "ASSIMP_BUILD_GLTF_IMPORTER ON"
            "ASSIMP_BUILD_FBX_IMPORTER OFF"
            "ASSIMP_BUILD_COLLADA_IMPORTER OFF"
    )
    message(STATUS "***** Setup Complete *****")
endif ()

# Setup KTX-Software ###############################################################
if (RNDR_KTX)
    message(STATUS "***** Setting up KTX Software Dependency *****")
    # Not EXCLUDE_FROM_ALL: KTX's own install/export rules must run so find_dependency(Ktx)
    # can locate it from the rndr package.
    cpmaddpackage(
            NAME ktx
            GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
            GIT_TAG "v4.4.2"
            # Its headers are third-party and not held to rndr's warnings: khr_df.h uses C-style casts.
            SYSTEM YES
            OPTIONS
            "KTX_FEATURE_TOOLS OFF"
    )
    message(STATUS "***** Setup Complete *****")
endif ()

# Setup X11 windowing stack ########################################################
# Raw XCB plus libxkbcommon for keyboard translation - the Linux platform layer wraps
# these directly, mirroring how the Windows layer wraps Win32. xcb-xkb is needed for
# detectable auto-repeat, xcb-randr for monitors, xcb-xfixes for cursor hiding.
if (UNIX AND NOT ANDROID)
    message(STATUS "***** Setting up X11/XCB Dependency *****")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RNDR_XCB REQUIRED IMPORTED_TARGET
            xcb xcb-randr xcb-xfixes xcb-xkb xkbcommon xkbcommon-x11)
    message(STATUS "***** Setup Complete *****")
endif ()

# Setup SPIRV-Reflect ##############################################################
# Two-file library compiled straight into rndr, so only the sources are fetched.
# Pinned to the tag matching the Vulkan SDK release the project builds against, and
# fetched from Khronos rather than taken from the SDK's Source/ directory - the
# Linux SDK does not ship one.
if (RNDR_FORGE)
    message(STATUS "***** Setting up SPIRV-Reflect Dependency *****")
    cpmaddpackage(
            NAME spirv-reflect
            GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect
            GIT_TAG vulkan-sdk-1.4.335.0
            DOWNLOAD_ONLY YES
    )
    message(STATUS "***** Setup Complete *****")
endif ()

# Setup Vulkan headers, volk and VMA ###############################################
# Everything Forge needs at compile time, fetched pinned instead of taken from the
# installed SDK's Include/ directory - the build no longer depends on VULKAN_SDK or
# its Windows-only layout. All header-only from rndr's point of view (the
# implementations live in volk-implementation.cpp / vma-implementation.cpp), so all
# three are DOWNLOAD_ONLY and wired up as include directories in src/CMakeLists.txt.
if (RNDR_FORGE)
    message(STATUS "***** Setting up Vulkan Headers/volk/VMA Dependencies *****")
    cpmaddpackage(
            NAME vulkan-headers
            GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers
            GIT_TAG vulkan-sdk-1.4.335.0
            DOWNLOAD_ONLY YES
    )
    cpmaddpackage(
            NAME volk
            GIT_REPOSITORY https://github.com/zeux/volk
            GIT_TAG vulkan-sdk-1.4.335.0
            DOWNLOAD_ONLY YES
    )
    # VMA is versioned independently of the SDK; v3.3.0 is the release the 1.4.335
    # SDK bundles.
    cpmaddpackage(
            NAME vma
            GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
            GIT_TAG v3.3.0
            DOWNLOAD_ONLY YES
    )
    message(STATUS "***** Setup Complete *****")
endif ()

# Setup Slang #####################################################################
# Pull in prebuilt Slang release binaries instead of building from source (the
# from-source build is very heavy). CPM downloads and extracts the archive and we
# wrap it in an imported target named `slang`. Not on a platform without the compiler (see
# RNDR_SHADER_COMPILER), where there is no archive to take it from.
set(SLANG_VERSION 2026.10.2)
if ((RNDR_FORGE OR RNDR_CANVAS) AND RNDR_SHADER_COMPILER AND NOT TARGET slang)
    message(STATUS "***** Setting up Slang Dependency *****")
    if (WIN32)
        set(SLANG_ARCHIVE "slang-${SLANG_VERSION}-windows-x86_64.zip")
    elseif (LINUX)
        set(SLANG_ARCHIVE "slang-${SLANG_VERSION}-linux-x86_64.tar.gz")
    else ()
        message(FATAL_ERROR "Unsupported platform for Slang dependency")
    endif ()
    cpmaddpackage(
            NAME slang
            VERSION ${SLANG_VERSION}
            URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${SLANG_ARCHIVE}
            DOWNLOAD_ONLY YES
    )
    add_library(slang SHARED IMPORTED GLOBAL)
    target_include_directories(slang INTERFACE ${slang_SOURCE_DIR}/include)
    if (WIN32)
        set_target_properties(slang PROPERTIES
                IMPORTED_IMPLIB ${slang_SOURCE_DIR}/lib/slang.lib
                IMPORTED_LOCATION ${slang_SOURCE_DIR}/bin/slang.dll)
    else ()
        set_target_properties(slang PROPERTIES
                IMPORTED_LOCATION ${slang_SOURCE_DIR}/lib/libslang.so)
    endif ()
    # Directory holding the runtime libraries (slang.dll, slang-glslang.dll, ...)
    # that must sit next to any executable linking against Slang on Windows.
    set(SLANG_RUNTIME_DIR ${slang_SOURCE_DIR}/bin CACHE INTERNAL "Slang runtime library directory")
    # The import library that static consumers of an installed rndr must link against. Installed
    # into <prefix>/lib and re-imported by rndrConfig.cmake.
    if (WIN32)
        set(SLANG_IMPLIB ${slang_SOURCE_DIR}/lib/slang.lib CACHE INTERNAL "Slang import library")
    endif ()
    message(STATUS "***** Setup Complete *****")
endif ()

# Setup the host slangc ###########################################################
# For rndr_compile_shader (cmake/shaders.cmake), which compiles shaders at build time for a build that cannot
# at run time. The archive above has one when the target is the host. A cross build (Android) takes the
# archive for the machine it runs on instead, since the one for the target does not exist.
if (RNDR_FORGE AND NOT DEFINED RNDR_SLANGC)
    if (DEFINED slang_SOURCE_DIR)
        set(RNDR_SLANG_HOST_DIR ${slang_SOURCE_DIR})
    else ()
        message(STATUS "***** Setting up the host Slang compiler *****")
        if (CMAKE_HOST_WIN32)
            set(SLANG_HOST_ARCHIVE "slang-${SLANG_VERSION}-windows-x86_64.zip")
        elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
            set(SLANG_HOST_ARCHIVE "slang-${SLANG_VERSION}-linux-x86_64.tar.gz")
        else ()
            message(FATAL_ERROR "No Slang release for the host ${CMAKE_HOST_SYSTEM_NAME} to compile shaders with")
        endif ()
        cpmaddpackage(
                NAME slang-host
                VERSION ${SLANG_VERSION}
                URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${SLANG_HOST_ARCHIVE}
                DOWNLOAD_ONLY YES
        )
        set(RNDR_SLANG_HOST_DIR ${slang-host_SOURCE_DIR})
        message(STATUS "***** Setup Complete *****")
    endif ()
    if (CMAKE_HOST_WIN32)
        set(RNDR_SLANGC ${RNDR_SLANG_HOST_DIR}/bin/slangc.exe CACHE INTERNAL "The host's slangc")
    else ()
        set(RNDR_SLANGC ${RNDR_SLANG_HOST_DIR}/bin/slangc CACHE INTERNAL "The host's slangc")
    endif ()
endif ()

# Copies Slang's runtime libraries (slang.dll, slang-glslang.dll, ...) next to the
# given executable target so it can be launched from the build tree on Windows.
# Internal helper - downstream targets should use rndr_deploy_runtime() instead.
function(rndr_copy_slang_runtime target)
    if (WIN32 AND DEFINED SLANG_RUNTIME_DIR)
        file(GLOB SLANG_RUNTIME_DLLS "${SLANG_RUNTIME_DIR}/*.dll")
        foreach (dll ${SLANG_RUNTIME_DLLS})
            add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:${target}>"
                    VERBATIM)
        endforeach ()
    endif ()
endfunction()

# Single entry point for deploying every runtime DLL an rndr-linked executable needs next
# to it. Call this once per executable that links rndr - both rndr's own samples/tests and
# downstream projects consuming rndr via CPM/add_subdirectory. Keeps callers from having to
# know which specific runtimes (Slang, ASan, ...) rndr pulls in.
function(rndr_deploy_runtime target)
    rndr_copy_slang_runtime(${target})
    rndr_copy_asan_runtime(${target})
endfunction()

# Makes a shared library the one a NativeActivity loads (android.app.lib_name in the manifest). The activity's
# entry point, ANativeActivity_onCreate, is in the NDK glue that rndr compiles into its own archive, and nothing
# the application writes refers to it - so without this the linker leaves it out and the activity fails to start.
# The application defines android_main, which the glue calls. The activity looks the library up by exact name, so
# the debug postfix a dependency may set globally (assimp sets "d") is kept off it.
function(rndr_android_activity target)
    if (ANDROID)
        target_link_options(${target} PRIVATE "LINKER:--undefined=ANativeActivity_onCreate")
        set_target_properties(${target} PROPERTIES DEBUG_POSTFIX "")
    endif ()
endfunction()