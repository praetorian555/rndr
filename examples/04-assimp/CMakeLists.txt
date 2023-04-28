add_executable(04-assimp assimp.cpp)
target_link_libraries(04-assimp PUBLIC rndr)
set_target_properties(04-assimp PROPERTIES FOLDER Examples)
target_compile_definitions(04-assimp PUBLIC -DASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}")