include(cmake/cpm.cmake)

if (NOT TARGET opal)
    message(STATUS "***** Setting up Opal Dependency *****")
    cpmaddpackage(
            NAME opal
            GIT_REPOSITORY https://github.com/praetorian555/opal
            GIT_TAG opal-0.3.3
            OPTIONS
            "OPAL_BUILD_TESTS OFF"
            "OPAL_HARDENING ${RNDR_HARDENING}"
            "OPAL_SHARED_LIBS ${RNDR_SHARED_LIBS}"
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
target_include_directories(imgui PUBLIC ${SOURCE_PATH})
message(STATUS "***** Setup Complete *****")

# Setup Assimp ######################################################################
message(STATUS "***** Setting up Assimp Dependency *****")
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

# Setup KTX-Software ###############################################################
message(STATUS "***** Setting up KTX Software Dependency *****")
cpmaddpackage(
        NAME ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG "v4.4.2"
)
message(STATUS "***** Setup Complete *****")

# Setup Slang #####################################################################
# Pull in prebuilt Slang release binaries instead of building from source (the
# from-source build is very heavy). CPM downloads and extracts the archive and we
# wrap it in an imported target named `slang`.
if ((RNDR_FORGE OR RNDR_CANVAS) AND NOT TARGET slang)
    message(STATUS "***** Setting up Slang Dependency *****")
    set(SLANG_VERSION 2026.10.2)
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
    message(STATUS "***** Setup Complete *****")
endif ()

# Copies Slang's runtime libraries (slang.dll, slang-glslang.dll, ...) next to the
# given executable target so it can be launched from the build tree on Windows.
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