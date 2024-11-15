# Render API in C++ #

## About ##

Rendering library that provides window and input management as well as low level API for GPU rendering. Currently, only __Windows__ platform and __OpenGL__ graphics API is supported.

## Setup ##

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

## Build Configuration ##

The library currently offers following options for compile-time configuration:

* __RNDR_HARDENED__ Use hardened version of the library. Will enable address sanitizer. Default is ON.
* __RNDR_SHARED_LIBS__ Builds the project as a shared library. Default is OFF.
* __RNDR_TRACER__ Use built-in frame-time profiler. Will pull in spdlog library as a dependency. Default is OFF.
* __RNDR_DEFAULT_LOGGER__ Use default logger implementation. Will pull in spdlog library as a dependency. Default is OFF.
* __RNDR_BUILD_TESTS__ Build tests. Will pull in Catch2 library as a dependency. Default is ON. If this is a top level project, tests will be included and the option will be ignored.