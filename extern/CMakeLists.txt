# Include stb_image only if it is not already present
if (NOT TARGET stb_image)
	add_subdirectory(stb_image)
endif()

# Include stb_truetype only if it is not already present
if (NOT TARGET stb_truetype)
	add_subdirectory(stb_truetype)
endif()

if (RNDR_IMGUI)
	FetchContent_Declare(
		imgui
		GIT_REPOSITORY https://github.com/ocornut/imgui
		GIT_TAG        d7c8516a4b848c0291e3d75b627c0843f515f591 # release-1.89.2
	)
	FetchContent_MakeAvailable(imgui)

	set(SOURCE_PATH ${PROJECT_BINARY_DIR}/_deps/imgui-src)
	add_library(imgui ${SOURCE_PATH}/imgui.cpp
					  ${SOURCE_PATH}/imgui.h
					  ${SOURCE_PATH}/imconfig.h
					  ${SOURCE_PATH}/imgui_internal.h
					  ${SOURCE_PATH}/imstb_rectpack.h
					  ${SOURCE_PATH}/imstb_truetype.h
					  ${SOURCE_PATH}/imstb_textedit.h
					  ${SOURCE_PATH}/imgui_draw.cpp
					  ${SOURCE_PATH}/imgui_demo.cpp
					  ${SOURCE_PATH}/imgui_tables.cpp
					  ${SOURCE_PATH}/imgui_widgets.cpp
					  ${SOURCE_PATH}/backends/imgui_impl_win32.cpp
					  ${SOURCE_PATH}/backends/imgui_impl_win32.h
					  ${SOURCE_PATH}/backends/imgui_impl_dx11.cpp
					  ${SOURCE_PATH}/backends/imgui_impl_dx11.h)

	target_include_directories(imgui PUBLIC ${SOURCE_PATH})

	set_target_properties(imgui PROPERTIES FOLDER Extern)
endif()
