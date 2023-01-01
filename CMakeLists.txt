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

	# Global compiler configuration
	if (MSVC)
		# Treat warnings as errors and highest warning level
		# add_compile_options(/W4 /WX)
		# Multiprocessor compilation
		add_compile_options(/MP)
		# Turn on address sanitizer unless its Release build
		if (RNDR_SANITIZER)
			add_compile_options($<$<NOT:$<CONFIG:Release>>:/fsanitize=address>)
		endif()
	endif()
	# Use C++ 20 and make it required
	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Enable unity build
if (RNDR_UNITY)
	set(CMAKE_UNITY_BUILD TRUE)
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
endif()

# Include math only if it is not already present
if (NOT TARGET math)
	FetchContent_Declare(
		math
		GIT_REPOSITORY https://github.com/praetorian555/math
		GIT_TAG        c473de8e4e98558663145b7eab7e728e0acef7e0
	)
	FetchContent_MakeAvailable(math)
	set_target_properties(math PROPERTIES FOLDER Extern)
	set(MATH_UNITY ${RNDR_UNITY} CACHE BOOL "" FORCE)
	set(MATH_SANITIZER ${RNDR_SANITIZER} CACHE BOOL "" FORCE)
endif()

# Adds external dependencies
add_subdirectory(extern)

# The compiled library code is here
add_subdirectory(src)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	# Add catch2 dependency
	if (NOT TARGET Catch2)
		FetchContent_Declare(
			Catch2
			GIT_REPOSITORY https://github.com/catchorg/Catch2
			GIT_TAG        ab6c7375be9a8e71ee84c6f8537113f9f47daf99 # release-3.2.1
		)
		FetchContent_MakeAvailable(Catch2)
		set_target_properties(Catch2 PROPERTIES FOLDER Extern)
		set_target_properties(Catch2WithMain PROPERTIES FOLDER Extern)
	endif()

	# Tests
	add_subdirectory(tests)

	# The executable code is here
	add_subdirectory(examples/uitext)

endif()
