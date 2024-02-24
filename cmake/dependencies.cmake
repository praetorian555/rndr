include(cmake/cpm.cmake)

function(setup_dependencies GLTF_SAMPLE_ASSETS)
    message(STATUS "Setting up dependencies...")

    if (NOT TARGET fmtlib::fmtlib)
        cpmaddpackage("gh:fmtlib/fmt#10.1.1")
        set_target_properties(fmt PROPERTIES FOLDER Extern)
    endif ()

    if (NOT TARGET spdlog::spdlog)
        cpmaddpackage(
                NAME
                spdlog
                VERSION
                1.12.0
                GITHUB_REPOSITORY
                "gabime/spdlog"
                OPTIONS
                "SPDLOG_FMT_EXTERNAL ON")
        set_target_properties(spdlog PROPERTIES FOLDER Extern)
    endif ()

    if (NOT TARGET Catch2::Catch2WithMain AND PROJECT_IS_TOP_LEVEL)
        cpmaddpackage("gh:catchorg/Catch2@3.3.2")
        set_target_properties(Catch2WithMain PROPERTIES FOLDER Extern)
        set_target_properties(Catch2 PROPERTIES FOLDER Extern)
    endif ()

    if (NOT TARGET math)
        cpmaddpackage(
                NAME
                math
                GITHUB_REPOSITORY
                "praetorian555/math"
                GIT_TAG
                "main"
                OPTIONS
                "MATH_SANITIZER ${RNDR_HARDENED}"
        )
        SET_TARGET_PROPERTIES(math PROPERTIES FOLDER Extern)
    endif ()

    if (NOT TARGET assimp)
        cpmaddpackage(
                NAME
                assimp
                GITHUB_REPOSITORY
                "assimp/assimp"
                VERSION
                5.3.1
                OPTIONS
                "ASSIMP_BUILD_ASSIMP_TOOLS OFF"
                "ASSIMP_BUILD_TESTS OFF"
                "ASSIMP_NO_EXPORT ON"
                "ASSIMP_INSTALL_PDB OFF"
                "ASSIMP_BUILD_ZLIB ON"
                "ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF"
                "ASSIMP_BUILD_OBJ_IMPORTER ON"
                "ASSIMP_BUILD_FBX_IMPORTER ON"
                "ASSIMP_BUILD_GLTF_IMPORTER ON"
                "ASSIMP_BUILD_COLLADA_IMPORTER ON"
        )
        SET_TARGET_PROPERTIES(assimp PROPERTIES FOLDER Extern)
        SET_TARGET_PROPERTIES(zlibstatic PROPERTIES FOLDER Extern)
        SET_TARGET_PROPERTIES(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES FOLDER Extern)
    endif ()

    if (NOT TARGET meshoptimizer)
        cpmaddpackage(
                NAME
                meshoptimizer
                GITHUB_REPOSITORY
                "zeux/meshoptimizer"
                GIT_TAG
                "v0.20"
                OPTIONS
                "MESHOPT_WERROR ON"
        )
        SET_TARGET_PROPERTIES(meshoptimizer PROPERTIES FOLDER Extern)
    endif ()

    cpmaddpackage(
            NAME
            sampleassets
            GITHUB_REPOSITORY
            "KhronosGroup/glTF-Sample-Assets"
            GIT_TAG
            "main"
            DOWNLOAD_ONLY
    )
    message(STATUS "sampleassets_SOURCE_DIR: ${sampleassets_SOURCE_DIR}")
    set(${GLTF_SAMPLE_ASSETS} "${sampleassets_SOURCE_DIR}/Models" PARENT_SCOPE)

    include(FetchContent)

    if (NOT TARGET imgui)
        FetchContent_Declare(
                imgui
                GIT_REPOSITORY https://github.com/ocornut/imgui
                GIT_TAG c6e0284ac58b3f205c95365478888f7b53b077e2 # release-1.89.9
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

        if (MSVC)
            set(IMGUI_WINDOWS_SOURCE_FILES
                ${SOURCE_PATH}/backends/imgui_impl_win32.cpp
                ${SOURCE_PATH}/backends/imgui_impl_win32.h)
        else ()
            set(IMGUI_WINDOWS_SOURCE_FILES)
        endif ()

        set(IMGUI_OPENGL_SOURCE_FILES
            ${SOURCE_PATH}/backends/imgui_impl_opengl3.cpp
            ${SOURCE_PATH}/backends/imgui_impl_opengl3.h include/rndr/utility/default-logger.h)

        add_library(imgui
                    ${IMGUI_SOURCE_FILES}
                    ${IMGUI_WINDOWS_SOURCE_FILES}
                    ${IMGUI_OPENGL_SOURCE_FILES})

        target_include_directories(imgui PUBLIC ${SOURCE_PATH})

        set_target_properties(imgui PROPERTIES FOLDER Extern)
    endif ()

    if (NOT TARGET etc2comp)
        FetchContent_Declare(
                etc2comp
                GIT_REPOSITORY https://github.com/google/etc2comp
                GIT_TAG 39422c1aa2f4889d636db5790af1d0be6ff3a226
        )
        FetchContent_Populate(etc2comp)
        FetchContent_GetProperties(etc2comp SOURCE_DIR ETC2COMP_SOURCE_PATH)
        file(GLOB_RECURSE ETC2COMP_SOURCES CONFIGURE_DEPENDS
             ${ETC2COMP_SOURCE_PATH}/EtcLib/Etc/*.h
             ${ETC2COMP_SOURCE_PATH}/EtcLib/EtcCodec/*.h
             ${ETC2COMP_SOURCE_PATH}/EtcLib/Etc/*.cpp
             ${ETC2COMP_SOURCE_PATH}/EtcLib/EtcCodec/*.cpp)
        message(STATUS "ETC2COMP_SOURCES: ${ETC2COMP_SOURCES}")
        add_library(etc2comp ${ETC2COMP_SOURCES}
                    ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFile.h
                    ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFile.cpp
                    ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFileHeader.h
                    ${ETC2COMP_SOURCE_PATH}/EtcTool/EtcFileHeader.cpp)
        target_include_directories(etc2comp PUBLIC ${ETC2COMP_SOURCE_PATH})
        target_include_directories(etc2comp PUBLIC ${ETC2COMP_SOURCE_PATH}/EtcLib/Etc
                                   ${ETC2COMP_SOURCE_PATH}/EtcLib/EtcCodec
                                   ${ETC2COMP_SOURCE_PATH}/EtcTool)
        target_compile_options(etc2comp PRIVATE /W1)
        set_target_properties(etc2comp PROPERTIES FOLDER Extern)
    endif ()

    FetchContent_Declare(
            gli
            GIT_REPOSITORY https://github.com/g-truc/gli
            GIT_TAG 30808550a20ca53a255e6e1e77070493eda7b736 # 0.8.2
    )
    FetchContent_Populate(gli)
    FetchContent_GetProperties(gli SOURCE_DIR GLI_SOURCE_PATH)
    add_library(gli INTERFACE)
    target_include_directories(gli INTERFACE ${GLI_SOURCE_PATH})
    target_include_directories(gli INTERFACE ${GLI_SOURCE_PATH}/external/glm)
    set_target_properties(gli PROPERTIES FOLDER Extern)

endfunction()

function(setup_bistro_scene_assets destination)
    set(URLS
            "https://casual-effects.com/g3d/data10/research/model/bistro/PropTextures"
            "https://casual-effects.com/g3d/data10/research/model/bistro/OtherTextures"
            "https://casual-effects.com/g3d/data10/research/model/bistro/BuildingTextures"
            "https://casual-effects.com/g3d/data10/research/model/bistro/Interior.zip"
            "https://casual-effects.com/g3d/data10/research/model/bistro/Exterior.zip"
    )

    foreach(URL ${URLS})
        get_filename_component(FILENAME ${URL} NAME_WE)
        set(DOWNLOAD_PATH "${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}")
        set(EXTRACT_PATH "${destination}/${FILENAME}")

        message(STATUS "Downloading ${FILENAME} to ${DOWNLOAD_PATH}")

        # If the filename ends with "Textures", extract to a Textures sub folder

        # Download the file
        file(DOWNLOAD ${URL} ${DOWNLOAD_PATH} SHOW_PROGRESS STATUS download_status)
        list(GET download_status 0 download_status_code)
        if (NOT download_status_code EQUAL 0)
            message(FATAL_ERROR "Failed to download ${FILENAME} from ${URL}")
            return()
        endif()

        message(STATUS "Extracting ${FILENAME} to ${EXTRACT_PATH}")

        # Check if the extraction directory exists and create it if it does not
        if(NOT EXISTS ${EXTRACT_PATH})
            file(MAKE_DIRECTORY ${EXTRACT_PATH})
        endif()

        # Unzip the file
        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xrzf ${DOWNLOAD_PATH} WORKING_DIRECTORY ${EXTRACT_PATH} RESULT_VARIABLE unzip_status OUTPUT_VARIABLE unzip_output ERROR_VARIABLE unzip_output)
        if (NOT unzip_status EQUAL 0)
            message(FATAL_ERROR "Failed to extract ${FILENAME} to ${EXTRACT_PATH} with error: ${unzip_output}")
            return()
        endif()

        # Delete the downloaded file
        file(REMOVE ${DOWNLOAD_PATH})
    endforeach()
endfunction()