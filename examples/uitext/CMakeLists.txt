# Collect all header files so that we can add them to the IDE later
file(GLOB_RECURSE SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/examples/uitext/src/*[.h|.c|.cpp]")

# Make an automatic library - will be static or dynamic based on user setting
add_executable(uitext ${SOURCE_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(uitext PRIVATE ${PROJECT_SOURCE_DIR}/examples/uitext/src)

# Setup library dependencies
target_link_libraries(uitext PRIVATE rndr)

# IDEs should put the headers in a nice place
set_target_properties(uitext PROPERTIES FOLDER Examples)
source_group(
  TREE "${PROJECT_SOURCE_DIR}/examples/uitext/src"
  PREFIX "App"
  FILES ${SOURCE_LIST})

target_compile_definitions(uitext PUBLIC BASIC2D_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

file(GLOB_RECURSE UNITY_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_BINARY_DIR}/examples/uitext/*.cxx")
if (RNDR_UNITY)
  source_group(
    TREE "${PROJECT_BINARY_DIR}/examples/uitext/CMakeFiles/uitext.dir/Unity"
    PREFIX "Unity"
    FILES ${UNITY_SOURCE_LIST})
endif()
