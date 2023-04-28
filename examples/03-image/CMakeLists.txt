add_executable(03-image image.cpp)
target_link_libraries(03-image PUBLIC rndr)
set_target_properties(03-image PROPERTIES FOLDER Examples)
target_compile_definitions(03-image PUBLIC -DIMAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")