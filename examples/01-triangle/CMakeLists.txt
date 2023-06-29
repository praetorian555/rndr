add_executable(01-triangle triangle.cpp)
target_link_libraries(01-triangle PUBLIC rndr)
set_target_properties(01-triangle PROPERTIES FOLDER Examples)