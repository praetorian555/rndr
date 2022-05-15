# Include spdlog only if it is not already present
if (NOT TARGET spdlog::spdlog)
	add_subdirectory(spdlog)
	set_target_properties(spdlog PROPERTIES FOLDER Extern)
endif()

# Include stb_image only if it is not already present
if (NOT TARGET stb_image)
	add_subdirectory(stb_image)
endif()

# Include math only if it is not already present
if (NOT TARGET math)
	add_subdirectory(math)
endif()
