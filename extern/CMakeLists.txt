# Include spdlog only if it is not already present
if (NOT TARGET spdlog::spdlog)
	add_subdirectory(spdlog)
	set_target_properties(spdlog PROPERTIES FOLDER Extern)
	target_compile_features(spdlog PRIVATE cxx_std_20)
	if (RNDR_SANITIZER)
		target_compile_options(spdlog PRIVATE $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<CONFIG:Release>>>:/fsanitize=address>)
  endif()
endif()

# Include stb_image only if it is not already present
if (NOT TARGET stb_image)
	add_subdirectory(stb_image)
endif()

# Include stb_truetype only if it is not already present
if (NOT TARGET stb_truetype)
	add_subdirectory(stb_truetype)
endif()

# Include math only if it is not already present
if (NOT TARGET math)
	set(MATH_UNITY ${RNDR_UNITY} CACHE BOOL "" FORCE)
	set(MATH_SANITIZER ${RNDR_SANITIZER} CACHE BOOL "" FORCE)
	add_subdirectory(math)
	set_target_properties(math PROPERTIES FOLDER Extern)
endif()

if (NOT TARGET Catch2)
	add_subdirectory(Catch2)
	set_target_properties(Catch2 PROPERTIES FOLDER Extern)
	set_target_properties(Catch2WithMain PROPERTIES FOLDER Extern)
endif()
