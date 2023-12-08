# Make an automatic library - will be static or dynamic based on user setting
add_library(stb_image include/stb_image/stb_image.h include/stb_image/stb_image_write.h include/stb_image/stb_image_resize2.h src/stb_image.cpp)

# We need this directory, and users of our library will need it too
target_include_directories(stb_image PUBLIC include)
target_include_directories(stb_image PRIVATE src)

# Put stb_image project under Extern folder in IDE
set_target_properties(stb_image PROPERTIES FOLDER Extern)
