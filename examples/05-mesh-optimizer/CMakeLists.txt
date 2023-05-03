add_executable(05-mesh-optimizer mesh-optimizer.cpp)
target_link_libraries(05-mesh-optimizer PUBLIC rndr)
set_target_properties(05-mesh-optimizer PROPERTIES FOLDER Examples)
target_compile_definitions(05-mesh-optimizer PUBLIC -DASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../04-assimp")