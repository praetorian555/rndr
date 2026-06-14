# Render API in C++ #

## About ##

Rendering library that provides window and input management as well as a high-level API for GPU rendering. Currently, only __Windows__ platform and __OpenGL__ graphics API is supported.

Core features:

* __Canvas API__ High-level rendering abstraction with command-list based drawing, automatic shader reflection, and GPU resource management (shaders, textures, buffers, meshes, render targets). Shaders are written in Slang and compiled to SPIR-V at runtime.
* __Built-in Renderers__ PBR renderer with instanced batching, 2D shape renderer, bitmap text renderer, cubemap skybox renderer, and infinite grid renderer.
* __Input System__ Stack-based input context model with support for keyboard, mouse, gamepad, text input, combos, and hold timers.
* __Window Management__ Native window creation and event handling.

## Setup ##

To generate a build system go to the root of a project and use:

	cmake -S <path_to_lib_root> -B <path_to_build_dir>

To build project using the cmake from command line:

	cmake --build <path_to_build_dir> --config <config_name>

Dependencies are fetched at configure time by [CPM](https://github.com/cpm-cmake/CPM.cmake). By default each
dependency is downloaded into the build directory, so every build directory re-downloads them. Set
_CPM_SOURCE_CACHE_ to a shared folder to download each dependency only once and reuse it across build
directories:

	cmake -S <path_to_lib_root> -B <path_to_build_dir> -DCPM_SOURCE_CACHE=<path_to_cache>

Alternatively, set the _CPM_SOURCE_CACHE_ environment variable.

If you installed the _clang-format_ tool you will have access to _clang-format_ and target. It can be run either by
building it in IDE or with following directive in command-line:

	cmake --build <build_dir> --target clang-format

The _clang-format_ target will apply formatting to all files under apps, include and src folders. To see the rules
applied take a look at _.clang-format_ file at the project's root.

Project also has _.clang-tidy_ file to be used with _clang-tidy_ tool in your favourite IDE.

## Build Configuration ##

The library currently offers following options for compile-time configuration:

* __RNDR_CANVAS__ Enable the Canvas API, a simplified graphics API. Default is ON.
* __RNDR_FORGE__ Enable the Forge API, a more advanced and powerful rendering API. Default is OFF.
* __RNDR_SHARED_LIBS__ Build as a shared library. Default is OFF.
* __RNDR_HARDENING__ Enable hardened mode. Default is ON.
* __RNDR_BUILD_TESTS__ Build tests. Default is ON.
* __RNDR_BUILD_SAMPLES__ Build sample executables. Default is ON.
* __RNDR_ASSIMP__ Pull in the Assimp dependency and enable mesh/model loading (`Forge::LoadMesh`,
  `Canvas::PbrRenderer::LoadModel`). When OFF, Assimp is not fetched and those functions throw if
  called. Default is ON.
* __RNDR_KTX__ Pull in the KTX-Software dependency and enable loading of KTX textures. When OFF, KTX
  is not fetched and `.ktx`/`.ktx2` files are not supported (other formats still load via stb_image).
  Default is ON.

## Runtime dependencies ##

On Windows, rndr links against runtime libraries that must sit next to any executable that links it —
Slang (`slang.dll`, `slang-glslang.dll`, ...) and, for hardened builds, the AddressSanitizer runtime.
For every executable target that links `rndr`, call `rndr_deploy_runtime()` so these DLLs are copied
next to it after the build:

	add_executable(my-app main.cpp)
	target_link_libraries(my-app PRIVATE rndr)
	rndr_deploy_runtime(my-app)

This applies both to rndr's own samples/tests and to downstream projects consuming rndr via CPM or
`add_subdirectory`. The Slang runtime DLLs are also placed in the `bin` directory by `cmake --install`
(see below), so installed executables are self-contained as well.

## Install ##

To install the library, its public headers and runtime DLLs into a prefix:

	cmake --install <path_to_build_dir> --config <config_name> --prefix <install_dir>

This installs `rndr` (into `lib`), its headers (into `include`) and the Slang runtime DLLs (into
`bin`). Bundled dependencies (opal, assimp, ktx, ...) are statically linked and not installed
separately.

Hardened builds cannot be installed. A build configured with __RNDR_HARDENING=ON__ (the default) is
AddressSanitizer-instrumented and is not meant to be distributed, so `cmake --install` fails with an
error. Reconfigure with `-DRNDR_HARDENING=OFF` before installing.

## Documentation ##

* [Canvas API](docs/canvas.md) — High-level rendering API, GPU resources, and built-in renderers.
* [Input System](docs/input-system.md) — Stack-based input contexts, actions, bindings, and combos.
* [Hardware](docs/hardware.md) — Hardware and platform details.
* [Vulkan](docs/vulkan.md) — Vulkan backend notes.