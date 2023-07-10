# Render API in C++ #

## About ##

Basic wrapper around different render APIs. Currently, only __OpenGL__ is supported.

## Setup ##

To install all necessary tools you will need to run Setup.ps1 PowerShell script:

* Start a PowerShell as an administrator and navigate to the root of the project.
* Enable execution of scripts by running `Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process`.
* Run `.\Setup.ps1` script.

To generate a build system go to the root of a project and use:

	cmake -S <path_to_lib_root> -B <path_to_build_dir>

To build project using the cmake from command line:

	cmake --build <path_to_build_dir> --config <config_name>

If you installed the _clang-format_ tool you will have access to _clang-format_ and target. It can be run either by
building it in IDE or with following directive in command-line:

	cmake --build <build_dir> --target clang-format

The _clang-format_ target will apply formatting to all files under apps, include and src folders. To see the rules
applied take a look at _.clang-format_ file at the project's root.

Project also has _.clang-tidy_ file to be used with _clang-tidy_ tool in your favourite IDE.

If you are using MSVC compiler you can use address sanitizer tool by setting the __RNDR_SANITIZER__ flag when invoking
the cmake command:

	cmake -S <path_to_lib_root> -B <build_dir> -DRNDR_SANITIZER=ON

## Build Configuration ##

The library currently offers following options for compile-time configuration:

* __RNDR_OPENGL__ Use OpenGL API as a rendering backend. Default is ON.
* __RNDR_SANITIZER__ Enable address sanitizer tool. Default is OFF.
* __RNDR_TRACER__ Use built-in frame-time profiler. Will pull in spdlog library as a dependency. Default is ON.
* __RNDR_DEFAULT_LOGGER__ Use default logger implementation. Will pull in spdlog library as a dependency. Default is ON.
* __RNDR_IMGUI__ Include ImGUI library as a dependency that can be used to create UI features. Default is ON.
* __RNDR_ASSIMP__ Include assimp library as a dependency that can be used to import 3D models. Default is ON.
* __RNDR_ETC2COMP__ Include etc2comp library as a dependency that can be used to compress textures. Default is ON.
* __RNDR_MESHOPTIMIZER__ Include meshoptimizer library as a dependency that can be used to optimize meshes. Default is ON.