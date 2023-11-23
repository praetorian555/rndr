add_executable(10-converters converters.cpp file-dialog.cpp)
target_link_libraries(10-converters PRIVATE rndr)
set_target_properties(10-converters PROPERTIES FOLDER Examples)