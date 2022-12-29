find_program(CLANG_TIDY "clang-tidy")

if (CLANG_TIDY)

	file(GLOB_RECURSE FILES_TO_FORMAT
		${PROJECT_SOURCE_DIR}/src/*.c
		${PROJECT_SOURCE_DIR}/src/*.cpp)

	add_custom_target(
        clang-tidy
        COMMAND clang-tidy
        -p ${PROJECT_SOURCE_DIR}/build 
        -checks=-*,clang-analyzer-*,concurrency-*,cppcoreguidelines-*,modernize-*,performance-*,readability-*,-modernize-use-trailing-return-type,-cppcoreguidelines-pro-type-union-access,-modernize-avoid-bind
        ${FILES_TO_FORMAT}
        --
        -I${PROJECT_SOURCE_DIR}/include
		-I${PROJECT_SOURCE_DIR}/src
        -I${PROJECT_SOURCE_DIR}/extern/math/include
        -DRNDR_DX11
        -DWIN32
    )
	set_target_properties(clang-tidy PROPERTIES FOLDER CustomCommands)

else()

    message(WARNING "clang-tidy not found, clang-tidy target will not work!")

endif()
