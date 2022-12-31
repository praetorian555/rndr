# Collect all header files so that we can add them to the IDE later
file(GLOB_RECURSE PUBLIC_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE PRIVATE_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*[.h|.c|.cpp]")
file(GLOB_RECURSE UNITY_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_BINARY_DIR}/src/*.cxx")

# Make an automatic library - will be static or dynamic based on user setting
add_library(rndr ${PRIVATE_SOURCE_LIST} ${PUBLIC_SOURCE_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(rndr PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(rndr PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Setup graphics API to use
if (RNDR_DX11)
target_compile_definitions(rndr PUBLIC RNDR_DX11)
endif()
if (RNDR_HANDMADE)
target_compile_definitions(rndr PUBLIC RNDR_HANDMADE)
endif()

if (RNDR_SPDLOG)
target_compile_definitions(rndr PUBLIC RNDR_SPDLOG)
target_link_libraries(rndr PUBLIC spdlog::spdlog)
endif()

# Setup library dependencies
target_link_libraries(rndr PUBLIC stb_image)
target_link_libraries(rndr PUBLIC stb_truetype)
target_link_libraries(rndr PUBLIC math)
if (RNDR_DX11)
target_link_libraries(rndr PUBLIC d3d11.lib dxgi.lib d3dcompiler.lib)
endif()

# Enable unity build
if (RNDR_UNITY)
set_target_properties(rndr PROPERTIES UNITY_BUILD TRUE)
endif()

# All users of this library will need at least C++20
target_compile_features(rndr PUBLIC cxx_std_20)

# Compiler options
target_compile_options(rndr PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>)
if (RNDR_SANITIZER)
  target_compile_options(rndr PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>)
endif()

# IDEs should put the headers in a nice place
set_target_properties(rndr PROPERTIES FOLDER RNDR)
source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "API"
  FILES ${PUBLIC_SOURCE_LIST})
source_group(
  TREE "${PROJECT_SOURCE_DIR}/src"
  PREFIX "Implementation"
  FILES ${PRIVATE_SOURCE_LIST})
if (RNDR_UNITY)
  source_group(
    TREE "${PROJECT_BINARY_DIR}/src/CMakeFiles/rndr.dir/Unity"
    PREFIX "Unity"
    FILES ${UNITY_SOURCE_LIST})
endif()

target_compile_definitions(rndr PUBLIC RNDR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets")
