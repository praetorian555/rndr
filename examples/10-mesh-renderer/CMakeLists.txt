add_executable(10-mesh-renderer mesh-renderer.cpp)
target_link_libraries(10-mesh-renderer PUBLIC rndr)
set_target_properties(10-mesh-renderer PROPERTIES FOLDER Examples)
target_compile_definitions(10-mesh-renderer PUBLIC -DASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/")