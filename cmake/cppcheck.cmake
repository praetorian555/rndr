find_program(CPPCHECK "cppcheck")

if (CPPCHECK)

    file(GLOB_RECURSE ALL_CXX_SOURCE_FILES
        ${PROJECT_SOURCE_DIR}/src/*.cpp
        ${PROJECT_SOURCE_DIR}/src/*.c
        ${PROJECT_SOURCE_DIR}/apps/*.cpp
        ${PROJECT_SOURCE_DIR}/apps/*.c
    )

    add_custom_target(
        cppcheck
        COMMAND cppcheck
        ${ALL_CXX_SOURCE_FILES}
        --std=c++17
        --enable=all
        -I ../include
        -I ../src
        -I ../apps
        -q
        --suppress=missingIncludeSystem
        --suppress=missingInclude
        --suppress=unmatchedSuppression
    )
    set_target_properties(cppcheck PROPERTIES FOLDER CustomCommands)

else()

    message(WARNING "cppcheck not found, cppcheck target will not work!")

endif()
