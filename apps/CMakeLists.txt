add_executable(app app.cpp)

target_compile_features(app PRIVATE cxx_std_17)

target_link_libraries(app PRIVATE rndr)

target_compile_definitions(app PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets")
