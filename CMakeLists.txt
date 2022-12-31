cmake_minimum_required(VERSION 3.20)

project(
	RNDR
	VERSION 0.1
	DESCRIPTION "Example C++ Project using CMake"
	LANGUAGES CXX)

# Include utils script
include(cmake/utils.cmake)

# Library options
option(RNDR_UNITY "Enable unity builds" ON)
option(RNDR_SANITIZER "Enable address sanitizer" ON)
option(RNDR_DX11 "Use DX11 API" ON)
option(RNDR_SPDLOG "Use spdlog external library to implement default logging" ON)

message(STATUS "RNDR_UNITY ${RNDR_UNITY}")
message(STATUS "RNDR_SANITIZER ${RNDR_SANITIZER}")
message(STATUS "RNDR_DX11 ${RNDR_DX11}")
message(STATUS "RNDR_SPDLOG ${RNDR_SPDLOG}")

# Only do these if this is the main project, and not if it is included through add_subdirectory
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

	# Let's ensure -std=c++xx instead of -std=g++xx
  	set(CMAKE_CXX_EXTENSIONS OFF)

  	include(cmake/optimized-debug.cmake)

	# Configuration types
	SET(CMAKE_CONFIGURATION_TYPES "Debug;OptimizedDebug;Release" CACHE STRING "Choose configuration: Debug OptimizedDebug Release" FORCE)

 	# Let's nicely support folders in IDEs
  	set_property(GLOBAL PROPERTY USE_FOLDERS ON)

  	# Include cmake scripts here
  	include(cmake/clang-format.cmake)
  	include(cmake/clang-tidy.cmake)

	# Add project level compiler options
	# ...
endif()

	include(FetchContent)

	# Include spdlog only if it is not already present
	if (NOT TARGET spdlog::spdlog AND RNDR_SPDLOG)
		FetchContent_Declare(
			spdlog
			GIT_REPOSITORY https://github.com/gabime/spdlog
			GIT_TAG        ad0e89cbfb4d0c1ce4d097e134eb7be67baebb36 # release-1.11.0
		)
		FetchContent_MakeAvailable(spdlog)
		set_target_properties(spdlog PROPERTIES FOLDER Extern)
		target_compile_features(spdlog PRIVATE cxx_std_20)
		target_compile_options(spdlog PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>)
		if (RNDR_SANITIZER)
			target_compile_options(spdlog PRIVATE $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<CONFIG:Release>>>:/fsanitize=address>)
		endif()
		if (RNDR_UNITY)
			set_target_properties(spdlog PROPERTIES UNITY_BUILD TRUE)
		endif()
	endif()

	if (NOT TARGET Catch2)
		FetchContent_Declare(
			Catch2
			GIT_REPOSITORY https://github.com/catchorg/Catch2
			GIT_TAG        ab6c7375be9a8e71ee84c6f8537113f9f47daf99 # release-3.2.1
		)
		FetchContent_MakeAvailable(Catch2)
		set_target_properties(Catch2 PROPERTIES FOLDER Extern)
		set_target_properties(Catch2WithMain PROPERTIES FOLDER Extern)
		target_compile_features(Catch2 PRIVATE cxx_std_20)
		target_compile_features(Catch2WithMain PRIVATE cxx_std_20)
		target_compile_options(Catch2 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>)
		target_compile_options(Catch2WithMain PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>)
		if (RNDR_SANITIZER)
			target_compile_options(Catch2 PRIVATE $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<CONFIG:Release>>>:/fsanitize=address>)
			target_compile_options(Catch2WithMain PRIVATE $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<CONFIG:Release>>>:/fsanitize=address>)
		endif()
		if (RNDR_UNITY)
			set_target_properties(Catch2 PROPERTIES UNITY_BUILD TRUE)
			set_target_properties(Catch2WithMain PROPERTIES UNITY_BUILD TRUE)
		endif()
	endif()

	# Adds external dependencies
	add_subdirectory(extern)

  	# The compiled library code is here
	add_subdirectory(src)

	# Tests
	add_subdirectory(tests)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

	# The executable code is here
	add_subdirectory(examples/uitext)

endif()
