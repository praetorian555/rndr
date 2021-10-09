cmake_minimum_required(VERSION 3.20)

project(
	RNDR
	VERSION 0.1
	DESCRIPTION "Example C++ Project using CMake"
	LANGUAGES CXX)

# Include utils script
include(cmake/utils.cmake)

# Only do these if this is the main project, and not if it is included through add_subdirectory
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

	# Let's ensure -std=c++xx instead of -std=g++xx
  	set(CMAKE_CXX_EXTENSIONS OFF)

	# Configuration types
	SET(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configs" FORCE)

 	# Let's nicely support folders in IDEs
  	set_property(GLOBAL PROPERTY USE_FOLDERS ON)

  	# Setup address sanitizer if this option is selected
  	if (ADDRESS_SANITIZER)
		add_compile_options(
			# For MSVC compilers activate address sanitizer
	  		$<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>)
	endif()

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