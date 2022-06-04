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

set(IMGUI_SOURCE_LIST imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp imgui/imgui.cpp)

set(IMGUI_HEADER_LIST imgui/imconfig.h imgui/imgui_internal.h imgui/imgui.h imgui/imstb_rectpack.h
	imgui/imstb_textedit.h imgui/imstb_truetype.h)

add_library(imgui ${IMGUI_SOURCE_LIST} ${IMGUI_HEADER_LIST})

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

# IDEs should put the headers in a nice place
set_target_properties(imgui PROPERTIES FOLDER Extern)
source_group(
  TREE "${PROJECT_SOURCE_DIR}/extern/imgui"
  PREFIX "API"
  FILES ${IMGUI_HEADER_LIST})
source_group(
  TREE "${PROJECT_SOURCE_DIR}/extern/imgui"
  PREFIX "Implementation"
  FILES ${IMGUI_SOURCE_LIST})
if (RNDR_UNITY)
	file(GLOB_RECURSE IMGUI_UNITY_LIST CONFIGURE_DEPENDS "${PROJECT_BINARY_DIR}/extern/CMakeFiles/*.cxx")
  source_group(
    TREE "${PROJECT_BINARY_DIR}/extern/CMakeFiles/imgui.dir/Unity"
    PREFIX "Unity"
    FILES ${IMGUI_UNITY_LIST})
endif()

endif()
