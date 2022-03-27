add_executable(cubes cubes.cpp boxpass.cpp boxpass.h lightpass.cpp lightpass.h)

target_compile_features(cubes PRIVATE cxx_std_17)

target_link_libraries(cubes PRIVATE rndr)

target_compile_definitions(cubes PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../assets")

set_target_properties(cubes PROPERTIES FOLDER Examples)
