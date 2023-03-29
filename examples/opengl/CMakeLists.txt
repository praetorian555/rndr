add_executable(opengl-example main-opengl.cpp)

target_link_libraries(opengl-example PUBLIC rndr)
set_target_properties(scene3d PROPERTIES FOLDER Examples)