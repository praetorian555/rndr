# Make an automatic library - will be static or dynamic based on user setting
add_library(stb_truetype include/stb_truetype/stb_truetype.h src/stb_truetype.cpp)

# We need this directory, and users of our library will need it too
target_include_directories(stb_truetype PUBLIC include)
target_include_directories(stb_truetype PRIVATE src)

# All users of this library will need at least C++17
target_compile_features(stb_truetype PUBLIC cxx_std_17)

# Put stb_truetype project under Extern folder in IDE
set_target_properties(stb_truetype PROPERTIES FOLDER Extern)
