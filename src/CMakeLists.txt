# Collect all header files so that we can add them to the IDE later
#file(GLOB_RECURSE PUBLIC_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/*.h")
#file(GLOB_RECURSE PRIVATE_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*[.h|.c|.cpp]")
file(GLOB_RECURSE UNITY_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_BINARY_DIR}/src/*.cxx")
set(PUBLIC_SOURCE_LIST "${PROJECT_SOURCE_DIR}/include/rndr/core/init.h")
set(PRIVATE_SOURCE_LIST "${PROJECT_SOURCE_DIR}/src/core/init.cpp")

# Make an automatic library - will be static or dynamic based on user setting
add_library(rndr ${PRIVATE_SOURCE_LIST} ${PUBLIC_SOURCE_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(rndr PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(rndr PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Setup graphics API to use
if (RNDR_DX11)
    target_compile_definitions(rndr PUBLIC RNDR_DX11)
endif ()
if (RNDR_OPENGL)
    target_compile_definitions(rndr PUBLIC RNDR_OPENGL)
endif ()

if (RNDR_SPDLOG)
    target_compile_definitions(rndr PUBLIC RNDR_SPDLOG)
    target_link_libraries(rndr PUBLIC spdlog::spdlog)
endif ()

if (RNDR_IMGUI)
    target_compile_definitions(rndr PUBLIC RNDR_IMGUI)
    target_link_libraries(rndr PUBLIC imgui)
endif ()

# Setup library dependencies
target_link_libraries(rndr PUBLIC stb_image)
target_link_libraries(rndr PUBLIC stb_truetype)
target_link_libraries(rndr PUBLIC math)
if (RNDR_DX11)
    target_link_libraries(rndr PUBLIC d3d11.lib dxgi.lib d3dcompiler.lib User32.lib)
endif ()
if (RNDR_OPENGL)
    target_link_libraries(rndr PUBLIC opengl32.lib User32.lib)
    target_link_libraries(rndr PUBLIC glad)
endif ()
if (RNDR_ASSIMP)
    target_compile_definitions(rndr PUBLIC RNDR_ASSIMP)
    target_link_libraries(rndr PUBLIC assimp)
endif ()

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
endif ()

# @formatter:off
target_compile_definitions(rndr PUBLIC RNDR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets")
# @formatter:on
