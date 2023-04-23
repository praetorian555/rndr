add_executable(cube cube.cpp)
target_link_libraries(cube PUBLIC rndr)
set_target_properties(cube PROPERTIES FOLDER Examples)