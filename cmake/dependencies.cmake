include(cmake/cpm.cmake)

if (NOT TARGET opal)
    message(STATUS "***** Setting up Opal Dependency *****")
    cpmaddpackage(
            NAME opal
            GIT_REPOSITORY https://github.com/praetorian555/opal
            GIT_TAG opal-0.3.2
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