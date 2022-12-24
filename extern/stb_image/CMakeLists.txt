# Make an automatic library - will be static or dynamic based on user setting
add_library(stb_image include/stb_image/stb_image.h include/stb_image/stb_image_write.h src/stb_image.cpp)

# We need this directory, and users of our library will need it too
target_include_directories(stb_image PUBLIC include)
target_include_directories(stb_image PRIVATE src)

# All users of this library will need at least C++20
target_compile_features(stb_image PUBLIC cxx_std_20)

# Put stb_image project under Extern folder in IDE
set_target_properties(stb_image PROPERTIES FOLDER Extern)

if (RNDR_SANITIZER)
    target_compile_options(stb_image PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>)
endif()