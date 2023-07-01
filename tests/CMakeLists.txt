add_executable(rndr_test base-test.cpp window-test.cpp input-test.cpp bitmap-test.cpp render-api-test.cpp)

target_link_libraries(rndr_test PRIVATE Catch2WithMain rndr)
set_target_properties(rndr_test PROPERTIES FOLDER RNDR)

file(GLOB_RECURSE UNITY_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_BINARY_DIR}/tests/*.cxx")
if (RNDR_UNITY)
    source_group(
            TREE "${PROJECT_BINARY_DIR}/tests/CMakeFiles/rndr_test.dir/Unity"
            PREFIX "Unity"
            FILES ${UNITY_SOURCE_LIST})
endif ()