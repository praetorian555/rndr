add_executable(rndr_test renderapi_test.cpp)

target_link_libraries(rndr_test PRIVATE Catch2WithMain rndr)
set_target_properties(rndr_test PROPERTIES FOLDER RNDR)