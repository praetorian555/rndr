# Make an automatic library - will be static or dynamic based on user setting
add_library(stb_image include/stb_image/stb_image.h src/stb_image.cpp)

# We need this directory, and users of our library will need it too
target_include_directories(stb_image PUBLIC include)
target_include_directories(stb_image PRIVATE src)

# All users of this library will need at least C++17
target_compile_features(stb_image PUBLIC cxx_std_17)

# IDEs should put the headers in a nice place
source_group(
  TREE "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PREFIX "Public"
  FILES include/stb_image/stb_image.h)
source_group(
  TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
  PREFIX "Private"
  FILES src/stb_image.cpp)