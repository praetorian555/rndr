# Include spdlog only if it is not already present
if (NOT TARGET spdlog::spdlog)
	add_subdirectory(spdlog)
endif()

# Include stb_image only if it is not already present
if (NOT TARGET stb_image)
	add_subdirectory(stb_image)
endif()