# Collect all header files so that we can add them to the IDE later
file(GLOB_RECURSE PUBLIC_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE PRIVATE_SOURCE_LIST CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*[.h|.c|.cpp]")

# Make an automatic library - will be static or dynamic based on user setting
add_library(rndr ${PRIVATE_SOURCE_LIST} ${PUBLIC_SOURCE_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(rndr PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(rndr PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(rndr PUBLIC spdlog::spdlog)

# All users of this library will need at least C++17
target_compile_features(rndr PUBLIC cxx_std_17)

# IDEs should put the headers in a nice place
source_group(
  TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Public"
  FILES ${PUBLIC_SOURCE_LIST})
source_group(
  TREE "${PROJECT_SOURCE_DIR}/src"
  PREFIX "Private"
  FILES ${PRIVATE_SOURCE_LIST})
