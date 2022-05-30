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
	if (RNDR_UNITY)
		set(MATH_UNITY ON CACHE BOOL "" FORCE)
	else()
		set(MATH_UNITY OFF CACHE BOOL "" FORCE)
	endif()
	add_subdirectory(math)
	set_target_properties(math PROPERTIES FOLDER Extern)
endif()

# Include imgui
if (RNDR_IMGUI)

add_library(imgui
			imgui/imconfig.h
			imgui/imgui_demo.cpp
            imgui/imgui_draw.cpp
			imgui/imgui_internal.h
			imgui/imgui_tables.cpp
			imgui/imgui_widgets.cpp
			imgui/imgui.cpp
			imgui/imgui.h
			imgui/imstb_rectpack.h
			imgui/imstb_textedit.h
			imgui/imstb_truetype.h)

# Enable unity build
if (RNDR_UNITY)
set_target_properties(imgui PROPERTIES UNITY_BUILD TRUE)
endif()

# All users of this library will need at least C++17
target_compile_features(imgui PUBLIC cxx_std_17)

# Compiler options
target_compile_options(imgui PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>)
if (RNDR_SANITIZER)
  target_compile_options(imgui PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>)
endif()

endif()
