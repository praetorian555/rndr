# Include stb_image only if it is not already present
if (NOT TARGET stb_image)
    add_subdirectory(stb_image)
endif ()

# Include stb_truetype only if it is not already present
if (NOT TARGET stb_truetype)
    add_subdirectory(stb_truetype)
endif ()
