# Collect all header files so that we can add them to the IDE later
file(GLOB_RECURSE SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/examples/scene3d/src/*[.h|.c|.cpp]")
file(GLOB_RECURSE UNITY_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_BINARY_DIR}/examples/scene3d/*.cxx")

# Make an automatic library - will be static or dynamic based on user setting
add_executable(scene3d ${SOURCE_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(scene3d PRIVATE ${PROJECT_SOURCE_DIR}/examples/scene3d/src)

# Setup library dependencies
target_link_libraries(scene3d PRIVATE rndr)

# IDEs should put the headers in a nice place
set_target_properties(scene3d PROPERTIES FOLDER Examples)
source_group(
        TREE "${PROJECT_SOURCE_DIR}/examples/scene3d/src"
        PREFIX "App"
        FILES ${SOURCE_LIST})

target_compile_definitions(scene3d PUBLIC SCENE3D_ASSET_DIR= "${CMAKE_CURRENT_SOURCE_DIR}/assets")

if (RNDR_UNITY)
    source_group(
            TREE "${PROJECT_BINARY_DIR}/examples/scene3d/CMakeFiles/scene3d.dir/Unity"
            PREFIX "Unity"
            FILES ${UNITY_SOURCE_LIST})
endif ()
