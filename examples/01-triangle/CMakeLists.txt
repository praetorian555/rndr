add_executable(triangle triangle.cpp)
target_link_libraries(triangle PUBLIC rndr)
set_target_properties(triangle PROPERTIES FOLDER Examples)