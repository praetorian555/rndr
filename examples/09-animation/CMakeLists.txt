add_executable(09-animation animation.cpp animator.h animator.cpp)
target_link_libraries(09-animation PUBLIC rndr)
set_target_properties(09-animation PROPERTIES FOLDER Examples)
target_compile_definitions(09-animation PUBLIC -DASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/")