add_executable(02-cube cube.cpp)
target_link_libraries(02-cube PUBLIC rndr)
target_link_libraries(02-cube PRIVATE rndr_options rndr_warnings)
set_target_properties(02-cube PROPERTIES FOLDER Examples)