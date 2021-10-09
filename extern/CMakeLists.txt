# Include spdlog only if it is not already present
if (NOT TARGET spdlog::spdlog)
	add_subdirectory(spdlog)
endif()