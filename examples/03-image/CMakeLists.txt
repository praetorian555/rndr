add_executable(image image.cpp)
target_link_libraries(image PUBLIC rndr)
set_target_properties(image PROPERTIES FOLDER Examples)
target_compile_definitions(image PUBLIC -DIMAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")