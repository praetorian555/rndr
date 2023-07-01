# Render API in C++ #

## About ##

Basic wrapper around different render APIs. Currently, only __OpenGL__ is supported.

## Requirements ##

To setup this project you will need:

* _CMake_ version 3.20 or heigher.
* Any version of _Visual Studio_ and _MSVC_ compiler.

Optionally you should install _clang-format_ and add it to the path.

## Setup ##

To generate a build system go to the root of a project and use:

	cmake -S <path_to_lib_root> -B <path_to_build_dir>

To build project using the cmake from command line:

	cmake --build <path_to_build_dir> --config <config_name>

If you installed the _clang-format_ tool you will have access to _clang-format_ and target. It can be run either by building it in IDE or with following directive in command-line:

	cmake --build <build_dir> --target clang-format

The _clang-format_ target will apply formatting to all files under apps, include and src folders. To see the rules applied take a look at _.clang-format_ file at the project's root.

Project also has _.clang-tidy_ file to be used with _clang-tidy_ tool in your favourite IDE. 

If you are using MSVC compiler you can use address sanitizer tool by setting the __RNDR_SANITIZER__ flag when invoking the cmake command:

	cmake -S <path_to_lib_root> -B <build_dir> -DRNDR_SANITIZER=ON

## Build Configuration ##

The library currently offers following options for configuration:

 * __RNDR_SANITIZER__ - Enables address sanitizer in the build. By default OFF.
 * __RNDR_OPENGL__ - Use OpenGL for the implementation of the rendering API. By default ON.
 * __RNDR_SPDLOG__ - Use spdlog library as a default logger implementation if user doesn't provide the log callback. By default ON.
 * __RNDR_IMGUI__ - Enables ImGuiWrapper class which can be used to render imgui UI. By default ON.
