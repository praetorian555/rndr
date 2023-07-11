add_executable(08-debug-features debug-features.cpp)
target_link_libraries(08-debug-features PUBLIC rndr)
set_target_properties(08-debug-features PROPERTIES FOLDER Examples)
target_compile_definitions(08-debug-features PUBLIC -DASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/")