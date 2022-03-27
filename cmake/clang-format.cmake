find_program(CLANG_FORMAT "clang-format")

if (CLANG_FORMAT)

	file(GLOB_RECURSE FILES_TO_FORMAT
		${PROJECT_SOURCE_DIR}/include/*.h
		${PROJECT_SOURCE_DIR}/include/*.hpp
		${PROJECT_SOURCE_DIR}/src/*.c
		${PROJECT_SOURCE_DIR}/src/*.cpp
		${PROJECT_SOURCE_DIR}/src/*.h
		${PROJECT_SOURCE_DIR}/src/*.hpp
		${PROJECT_SOURCE_DIR}/apps/*.c
		${PROJECT_SOURCE_DIR}/apps/*.cpp
		${PROJECT_SOURCE_DIR}/apps/*.h
		${PROJECT_SOURCE_DIR}/apps/*.hpp)

	add_custom_target(
        clang-format
        COMMAND clang-format
        -i
        -style=file
        ${FILES_TO_FORMAT}
    )
	set_target_properties(clang-format PROPERTIES FOLDER CustomCommands)

else()

    message(WARNING "clang-format not found, clang-format target will not work!")

endif()
