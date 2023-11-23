add_executable(11-scene-renderer scene-renderer.cpp)
target_link_libraries(11-scene-renderer PUBLIC rndr)
set_target_properties(11-scene-renderer PROPERTIES FOLDER Examples)
target_compile_definitions(11-scene-renderer PUBLIC -DASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/")