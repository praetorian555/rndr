find_program(CLANG_TIDY "clang-tidy")

if (CLANG_TIDY)

	file(GLOB_RECURSE FILES_TO_FORMAT
		${PROJECT_SOURCE_DIR}/src/*.c
		${PROJECT_SOURCE_DIR}/src/*.cpp)

    
    if (WIN32)
    endif()

	add_custom_target(
        clang-tidy
        COMMAND clang-tidy
        -config-file=../.clang-tidy
        ${FILES_TO_FORMAT}
        --
        -I${PROJECT_SOURCE_DIR}/include
		-I${PROJECT_SOURCE_DIR}/src
        -I${PROJECT_SOURCE_DIR}/build/_deps/math-src/include
        -I${PROJECT_SOURCE_DIR}/build/_deps/spdlog-src/include
        -I${PROJECT_SOURCE_DIR}/extern/stb_image/include
        -DWIN32
        -DRNDR_DX11
        -DRNDR_SPDLOG
        -DRNDR_ASSET_DIR="C:/dev/rndr/src/../assets"
        -std=c++20
    )
	set_target_properties(clang-tidy PROPERTIES FOLDER CustomCommands)

else()

    message(WARNING "clang-tidy not found, clang-tidy target will not work!")

endif()
