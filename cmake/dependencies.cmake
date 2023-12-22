include(cmake/cpm.cmake)

function(setup_dependencies)
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
                "MATH_SANITIZER ${RNDR_SANITIZER}"
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

endfunction()