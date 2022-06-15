cmake_minimum_required(VERSION 3.20)

project(
	RNDR
	VERSION 0.1
	DESCRIPTION "Example C++ Project using CMake"
	LANGUAGES CXX)

# Include utils script
include(cmake/utils.cmake)

# Library options
option(RNDR_UNITY "Enable unity builds" OFF)
option(RNDR_SANITIZER "Enable address sanitizer" OFF)
option(RNDR_DX11 "Use DX11 API" ON)
option(RNDR_HANDMADE "Use handmade software rendering pipeline" OFF)

message(STATUS "RNDR_UNITY ${RNDR_UNITY}")
message(STATUS "RNDR_SANITIZER ${RNDR_SANITIZER}")
message(STATUS "RNDR_DX11 ${RNDR_DX11}")
message(STATUS "RNDR_HANDMADE ${RNDR_HANDMADE}")

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
	include(cmake/cppcheck.cmake)

	# Add project level compiler options
	# ...
endif()

	# Adds external dependencies
	add_subdirectory(extern)

  	# The compiled library code is here
	add_subdirectory(src)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

	# The executable code is here
	add_subdirectory(apps)
endif()
