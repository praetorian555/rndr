# Render API in C++ #

## About ##

Basic wrapper around different render APIs. Currently supported APIs are:

* DX11

## Requirements ##

To setup this project you will need:

* _CMake_ version 3.20 or heigher.
* Any version of _Visual Studio_ and _MSVC_ compiler.

Optionally you should install _clang-format_ and _clang-tidy_ and add them to the path.

## Setup ##

To clone the project and include all submodulues:

	git clone --recurse-submodules <project_url>

If you already cloned the project but forgot the --recurse-submodules you can use the following commands:

	git submodule init
	git submodule update

To generate a build system go to the root of a project and use:

	cmake -S <path_to_lib_root> -B <path_to_build_dir>

To build project using the cmake from command line:

	cmake --build <path_to_build_dir> --config <config_name>

If you installed the _clang-format_ and _clang-tidy_ tools you will have access to clang-format and clang-tidy targets. They can be run either by building them in IDE or with following directive in command-line:

	cmake --build <build_dir> --target clang-format
	cmake --build <build_dir> --target clang-tidy

The clang-format target will apply formatting to all files under apps, include and src folders. To see the rules applied take a look at .clang-format file at the project's root.

The clang-tidy target will apply static-code analysis on all cpp and c files in the src folder. This part is still in development and the final set of rules to be used is not determined. 

If you are using MSVC compiler you can use address sanitizer tool by setting the RNDR_SANITIZER flag when invoking the cmake command:

	cmake -S <path_to_lib_root> -B <build_dir> -DRNDR_SANITIZER=ON

## Build Configuration ##

The library currently offers following options for configuration:

 * RNDR_UNITY - Enables unity build of the library. By default ON.
 * RNDR_SANITIZER - Enables address sanitizer in the build. By default ON.
 * RNDR_DX11 - Use DX11 as the implementation of the rendering API. By default ON.
 * RNDR_SPDLOG - Use spdlog library as a default logger implementation if user doesn't provide the log callback. By default ON.
