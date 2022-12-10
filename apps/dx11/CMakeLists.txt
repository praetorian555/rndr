add_executable(dx11 WIN32 tutorial.cpp)

target_compile_features(dx11 PRIVATE cxx_std_20)

target_link_libraries(dx11 PRIVATE d3d11.lib dxgi.lib d3dcompiler.lib winmm.lib)

target_compile_definitions(dx11 PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../assets")

set_target_properties(dx11 PROPERTIES FOLDER Examples)
