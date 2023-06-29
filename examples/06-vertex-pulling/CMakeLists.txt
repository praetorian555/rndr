add_executable(06-vertex-pulling vertex-pulling.cpp)
target_link_libraries(06-vertex-pulling PUBLIC rndr)
set_target_properties(06-vertex-pulling PROPERTIES FOLDER Examples)
target_compile_definitions(06-vertex-pulling PUBLIC -DASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/")
