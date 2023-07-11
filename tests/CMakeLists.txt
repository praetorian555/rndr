add_executable(rndr_test base-test.cpp window-test.cpp input-test.cpp bitmap-test.cpp render-api-test.cpp renderer-base-test.cpp frames-per-second-counter-test.cpp)

target_link_libraries(rndr_test PRIVATE Catch2WithMain rndr)
set_target_properties(rndr_test PROPERTIES FOLDER RNDR)
