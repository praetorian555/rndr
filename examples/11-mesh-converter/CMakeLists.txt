add_executable(11-mesh-converter mesh-converter.cpp ../10-mesh-renderer/mesh.cpp ../10-mesh-renderer/mesh.h)
target_link_libraries(11-mesh-converter PRIVATE rndr)
set_target_properties(11-mesh-converter PROPERTIES FOLDER Examples)