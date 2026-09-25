# Android plan

## Context

The platform layer knows Windows and Linux: `Application`'s constructor picks the backend by `RNDR_WINDOWS` /
`RNDR_LINUX` (`src/application.cpp:62`), `PlatformApplication::CreateGenericWindow` does the same for the
window (`src/platform-application.cpp:43`), and Forge builds its surface from a Win32 or an XCB branch
(`src/forge/swap-chain.cpp:61`). Android is a third branch behind the same seam, the way the Linux port was,
with the X11 milestone (`docs/linux-windowing-plan.md`) as the template. Written against master `3557f95`
(rndr-0.5.11) on 2026-09-25.

Goal of the milestone: the `modern-vulkan` sample running on an arm64 Android device as a `NativeActivity`
APK, the `[forge]` suite running on that device, and a compile-only Android job in CI. Rendering is Forge
only: Canvas is OpenGL 4.5 over WGL and Android has GLES, so Canvas stays off there the way it does on Linux.

Three things decide the shape of the port, and each is settled below rather than left to the code:

- **Slang does not run on the device.** The v2026.10.2 release ships linux, macos, windows and wasm
  archives and nothing for Android, and building it from source for arm64 is a heavy build for a tool the
  device never needs. Shaders are compiled on the host and shipped as SPIR-V. Forge already loads that:
  `Shader::FromSpirvFile` / `FromSpirvInMemory` (`include/rndr/forge/shader.hpp:124`), with reflection from
  SPIRV-Reflect rather than from Slang. The consequence is a Forge build with no shader compiler, which is
  a new configuration on every platform and gets its own build knob (Phase 0).
- **Assets are extracted, not streamed.** `RNDR_CORE_ASSETS_DIR` is a compile-time absolute path
  (`src/CMakeLists.txt:268`) and every loader opens a path with `fopen`: `aiImportFile`
  (`src/forge/mesh.cpp:28`), `ktxTexture_CreateFromNamedFile` and `stbi_load` (`src/file.cpp:259,305`). An
  `AAssetManager` read path would have to reach into all three, and assimp opens a glTF's sibling `.bin`
  and textures by itself. The APK's `assets/` are copied into `internalDataPath` on launch instead, and
  nothing below `File` changes.
- **The OS owns the one window.** On the desktop the application creates a window and it lives until the
  application destroys it. On Android the activity is handed one `ANativeWindow` at `APP_CMD_INIT_WINDOW`,
  loses it at `APP_CMD_TERM_WINDOW` when it goes to the background, and gets a new one when it comes back,
  while the application keeps running. `GenericWindow` has nothing to say that its native handle changed,
  and `Forge::Surface` is built over that handle. The window object survives the native window; the
  handle change is an event; the application rebuilds the surface and the swap chain in it (Phase 1).

The other decisions:

- **`android_native_app_glue`, not GameActivity.** The glue ships with the NDK, is plain C, and needs no
  Java, no AndroidX and no Kotlin. GameActivity buys text input and a newer input path, neither of which
  the milestone needs, and costs a Gradle dependency and a Java class. House style is to wrap the platform
  directly (Win32, XCB, WASAPI, Vulkan), and the glue is the thinnest layer there is.
- **Vulkan 1.3 devices only.** Forge asks for `VK_API_VERSION_1_3` (`src/forge/graphics-context.cpp:273`)
  and requires timeline semaphores, synchronization2 and dynamic rendering (`src/forge/device.cpp:328`).
  Devices launching with Android 15 are required to ship 1.3, and the Adreno 7xx and Mali Valhall
  generations from 2022 on expose it. Anything older is refused by `Device::Create` with the same message
  it gives a desktop GPU that cannot, and the APK manifest says so up front with
  `android.hardware.vulkan.version` 1.3.
- **Touch is a mouse.** The input primitives are keyboard, mouse and gamepad (`include/rndr/input-primitives.hpp`),
  and the sample steers with a mouse. The first pointer down, move and up become the left button and mouse
  motion; other pointers are dropped. Touch as its own primitive is out of scope (see the last section).
- **The entry point belongs to the sample.** rndr is a library and defines no `main` on any platform;
  `android_main` lives in the sample and hands its `android_app*` to `Application::Create` through the
  desc. A library that owns the entry point is the wrong side of the seam.
- Error reporting is the repo convention throughout: static `Create` returning `Opal::Expected<T, ErrorCode>`,
  methods returning `ErrorCode`, what the platform cannot do reported as `FeatureNotSupported` and said so
  in the header, detail to the log, nothing throws.
- The test rules are the desktop ones: a case that needs what the machine lacks skips rather than fails,
  and `RNDR_TEST_REQUIRE_VULKAN=1` turns the skip into a failure so a run that found no device cannot look
  like a run that passed.

## Status

| Phase | State |
|---|---|
| 0 — toolchain and portability groundwork | Not started |
| 1 — AndroidApplication + AndroidWindow | Not started |
| 2 — Forge surface | Not started |
| 3 — shaders and assets | Not started |
| 4 — sample APK | Not started |
| 5 — tests on the device and CI | Not started |

Machine state on 2026-09-25: no Android SDK or NDK installed, no `ANDROID_HOME`, `JAVA_HOME` points at
JDK 10 (Gradle 8 needs 17). Nothing in Phase 0 can start before the toolchain is there.

## Phase 0 — toolchain and portability groundwork

Install, in this order: Android Studio or the command-line tools, then through its SDK manager NDK r28,
platform-tools (`adb`) and the platform for `targetSdk`; JDK 17; set `ANDROID_HOME` and `ANDROID_NDK_HOME`.
A device with developer mode and USB debugging on, or nothing yet - the phase ends at a build, not a run.

Groundwork in the tree, none of it Android code yet:

- `include/rndr/definitions.hpp`: an `RNDR_ANDROID` branch checked **before** the Linux one. Android
  defines `__linux__`, so Opal's `defines.h` says `OPAL_PLATFORM_LINUX` there and rndr's
  `#elif defined(OPAL_PLATFORM_LINUX)` would pull in the XCB backend. The check is `__ANDROID__`. The
  macro set (`RNDR_DEBUG_BREAK`, `RNDR_FORCE_INLINE`, `RNDR_ALIGN`, `RNDR_OPTIMIZE_OFF/ON`) is the Clang
  half of the Linux branch. Opal itself is left alone: its Linux sources are Bionic-clean (futex through
  `syscall`, pthread, `dirent`, BSD sockets, `mmap`) and `OPAL_PLATFORM_LINUX` is the right answer for
  them. An `OPAL_PLATFORM_ANDROID` upstream is a nicety, not a need, and would have to keep the Linux
  define alongside it or break Opal's own `#if`s.
- The Forge sources branch on `OPAL_PLATFORM_*` directly (`src/forge/graphics-context.cpp:5,470`,
  `src/forge/swap-chain.cpp`) and need the Android check first for the same reason. `src/file.cpp`,
  `src/canvas/context.cpp`, `src/audio/audio-device.cpp` and `src/imgui-system.cpp` also branch on the
  platform; the first takes its non-Windows path, and the other three are not built.
- CMake: the NDK toolchain file sets `ANDROID` and leaves `UNIX` true, so every `if (WIN32) / elseif (UNIX)`
  gains an `elseif (ANDROID)` **before** `UNIX`. Top-level `CMakeLists.txt` forces `RNDR_CANVAS` and
  `RNDR_AUDIO` off on Android with the same status message it prints on Linux, and forces `RNDR_HARDENING`
  off too - ASan on Android goes through `wrap.sh` and a copied runtime, and HWASan is the supported
  sanitizer on arm64; both are later items - and does it **before** `include(cmake/dependencies.cmake)`,
  which hands the value to Opal as `OPAL_HARDENING`. `src/CMakeLists.txt` gets the platform block for the
  Android sources (Phase 1), links `android` and `log`, and defines `VK_USE_PLATFORM_ANDROID_KHR` beside
  the other two. The `rndr-test` link of `PkgConfig::RNDR_XCB` and the XCB block in `dependencies.cmake`
  become `UNIX AND NOT ANDROID`.
- **The shader compiler becomes optional.** `dependencies.cmake`'s Slang block already refuses Android
  (`elseif (LINUX)` is false there, so it lands on the `FATAL_ERROR`); it skips instead, and an internal
  `RNDR_SHADER_COMPILER` is ON everywhere but Android and becomes a public compile definition. Behind it:
  `src/core/shader-compiler.{hpp,cpp}` are not built; `Shader::FromSource` and `FromSourceInMemory`
  answer `FeatureNotSupported` after the cache lookup (`src/forge/shader.cpp:475`), so a source whose
  compiled code is in the cache still loads - which is what Phase 5 runs the suite on; Canvas asserts at
  configure time that it has the compiler, since it cannot run without one. `shader-cache.cpp` keeps
  building; how it gets a build tag without Slang is Phase 3.
- `RNDR_CORE_ASSETS_DIR` stays as it is. It is a host path, harmless in an Android build and still what
  the desktop samples and `bitmap-text-renderer.cpp` read; the Android sample takes its root at run time
  (Phase 3).
- `rndr_deploy_runtime` is a no-op on Android already (`WIN32`-guarded), and stays one: there are no
  Slang libraries to copy.

The phase ends when this configures and builds `librndr.a`:

    cmake -S . -B build/android-debug -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DCMAKE_BUILD_TYPE=Debug \
      -DRNDR_FORGE=ON -DRNDR_FORGE_VALIDATION=ON -DRNDR_ASSIMP=ON -DRNDR_KTX=OFF \
      -DRNDR_BUILD_TESTS=OFF -DRNDR_BUILD_SAMPLES=OFF
    cmake --build build/android-debug --target rndr

`RNDR_KTX=OFF` to start, as the WSL build did: KTX-Software builds for Android upstream, but it is a long
build and the sample loads PNG. It comes on when something needs a KTX file.

Checked by: the Android build above, and the Windows and Linux suites unchanged - the compiler knob is
the one change that reaches the desktop builds, so `rndr-test` on `build/msvc-debug` and the Linux CI job
are the regression check. Portability fixes the first NDK build turns up are recorded under this phase
as they come, the way the Linux plan recorded its first GCC build.

## Phase 1 — AndroidApplication + AndroidWindow

- `include/rndr/platform/android-application.hpp`, `src/platform/android-application.cpp`
- `include/rndr/platform/android-window.hpp`, `src/platform/android-window.cpp`
- `include/rndr/platform/android-forward-def.hpp`: forward declarations of `android_app` and
  `ANativeWindow`, mirroring `windows-forward-def.hpp`, so no public header includes an NDK header.
- `${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c` compiled into rndr as a
  `SYSTEM` source with warnings off, the way `spirv_reflect.c` is.

`ApplicationDesc` gains `android_app* android_app = nullptr;` - Android only, ignored elsewhere, and
`Application::Create` on Android answers `InvalidArgument` when it is null, since there is no activity to
run in without one. The sample's `android_main` fills it in and calls the same `Run()` the desktop `main`
calls.

`AndroidApplication` owns the `android_app*`, installs itself as `userData` with static trampolines on
`onAppCmd` and `onInputEvent`, and pumps the looper: `ProcessSystemEvents(timeout_ms)` is
`ALooper_pollOnce` with the timeout (`k_infinite_timeout` is -1) followed by zero-timeout polls until
nothing is pending, so one call drains the queue the way the Win32 and XCB pumps do. It adds a
`LogcatSink : Opal::LogSink` writing through `__android_log_write` to the logger once, because an
activity's stdout goes nowhere; a test executable run from `adb shell` keeps the console sink.

**The window's life.** `CreateGenericWindow` pumps until the glue reports `APP_CMD_INIT_WINDOW` and
`app->window` is set (or `destroyRequested`, which is `PlatformError`), so the caller gets a window
synchronously the way it does everywhere else. There is one; a second call is `InvalidArgument`. From
then on:

- `APP_CMD_TERM_WINDOW`: the `GenericWindow` stays, `GetNativeHandle()` returns null, `IsMinimized()` is
  true (which is what `SelectExtent` in `swap-chain.cpp:374` already reads to stop presenting).
- `APP_CMD_INIT_WINDOW` again: the handle is the new `ANativeWindow`.
- Both fire a new `Application::on_window_native_handle_change(const GenericWindow&)` multi-delegate and a
  `SystemMessageHandler::OnWindowNativeHandleChanged` with a no-op default like `OnMonitorChange`. Desktop
  never fires it. The application's handler destroys the swap chain and the surface on the way out and
  rebuilds both on the way back in; Forge's objects already own nothing of the window but the handle
  they were created over.
- `APP_CMD_WINDOW_RESIZED` and `APP_CMD_CONFIG_CHANGED` report `OnWindowSizeChanged` and refresh the DPI
  scale from `AConfiguration_getDensity` (density / 160), which is `OnWindowDpiChanged`.
- `APP_CMD_GAINED_FOCUS` / `LOST_FOCUS` drive `IsFocused()`. `APP_CMD_DESTROY` goes through
  `OnWindowClose`, which marks the window closed, so the sample's `while (!window->IsClosed())` ends and
  `android_main` returns.

`AndroidWindow` maps `GenericWindow` onto what one full-screen native window can do. `GetSize()` is
`ANativeWindow_getWidth/Height`; `GetPosition()` is (0, 0); `GetNativeDisplayHandle()` returns the
`android_app*`, which is what a caller who wants the asset manager or `internalDataPath` needs.
`RequestClose()` is `ANativeActivity_finish`. `IsVisible()` is whether there is a native window,
`IsWindowed()` true, `IsResizable()`, `IsBorderlessFullscreen()` and `IsMouseHovering()` false.
`GetCursorClientPosition()` is the last touch position. Everything the platform has no notion of -
`Reshape`, `MoveTo`, `SetTitle`, `SetOpacity`, the decoration and taskbar toggles, `Minimize`, `Maximize`,
`Restore`, `Show`, `Hide`, `Focus`, `BringToFront`, `Enable`, `SetMode` - returns `FeatureNotSupported`
where it returns a code and does nothing where it does not, and each says so in `generic-window.hpp` the
way the Linux cursor-shape note does (`generic-window.hpp:197`).

Input, all from `onInputEvent`:

- `AINPUT_EVENT_TYPE_MOTION` from a touchscreen: pointer index 0's down, move and up are
  `OnMouseButtonDown(Left)`, `OnMouseMove` with the delta from the last position, and `OnMouseButtonUp`.
  From a mouse source (a USB or Bluetooth mouse on a tablet): the real buttons, and
  `AMOTION_EVENT_ACTION_SCROLL`'s vertical axis is `OnMouseWheel`.
- `AINPUT_EVENT_TYPE_KEY`: `AKEYCODE_*` through a `TranslateKey` table to `InputPrimitive`, modifiers
  from `AKeyEvent_getMetaState`, repeats from `AKeyEvent_getRepeatCount`. `AKEYCODE_BACK` is not a key:
  it goes through `OnWindowClose` like the desktop close button, vetoable through `on_window_close`,
  because leaving is what back means. `OnCharacter` is not reported - the NDK has no
  `getUnicodeChar` without JNI - so a hardware keyboard gives key events only.
- Cursor: `ShowCursor`, `IsCursorVisible`, `SetCursorPosition` are no-ops; `GetCursorPosition` is the last
  touch, which is in screen space already since the window is the screen.
- Clipboard: `FeatureNotSupported` - it is a `ClipboardManager` call through JNI, out of scope.
- Monitors: one, sized from the native window, `dpi_scale` from the density, `refresh_rate` 60 because
  the real figure is `Display.getRefreshRate` through JNI.
- Gamepads arrive in the same event stream (`AINPUT_SOURCE_GAMEPAD` / `JOYSTICK`, `AKEYCODE_BUTTON_*`,
  `AMOTION_EVENT_AXIS_*`) and would take a table each; out of scope for the milestone, noted because it
  is cheap.

Checked by: not by a unit test. `AndroidApplication` needs a live `android_app` with a window behind it,
and that exists only inside an activity - the glue's struct can be faked, but the `ANativeWindow` behind
`app->window` cannot be conjured without a `Surface`. So the phase compiles under the Phase 0
configuration, and its behaviour is checked by the sample in Phase 4 and by the runner APK in Phase 5,
where `[forge-window]` builds a window through it. `[init]` and `[input]` are headless and platform-neutral
and run on the device as an executable in Phase 5 as they are.

## Phase 2 — Forge surface

- `src/forge/graphics-context.cpp` `GetRequiredInstanceExtensions`: `VK_KHR_ANDROID_SURFACE_EXTENSION_NAME`
  under `RNDR_ANDROID`, beside the Win32 and XCB names.
- `src/forge/swap-chain.cpp` `Surface::Create`: a `VkAndroidSurfaceCreateInfoKHR` whose `window` is
  `GetNativeHandle()`, through `vkCreateAndroidSurfaceKHR` and the same `RNDR_FORGE_VK_CHECK_EXPECTED`. A
  null handle - the window is in the background - is `PlatformError` with a log line saying so, since a
  surface over no window is not something to retry from inside Forge.
- `DeviceDesc::enable_presentation` (`docs/forge.md`, "A device before any window"): Android has no
  surface-free presentation query, so as on XCB the graphics family stands in, and `SwapChain::Create`
  verifies the surface when there is one. One line in `src/forge/device.cpp` where the Linux branch is.
- `VK_USE_PLATFORM_ANDROID_KHR` is public like the other two. The pinned Vulkan-Headers carry
  `vulkan_android.h`, volk has the matching block and dlopens `libvulkan.so` on Android, so nothing in
  `dependencies.cmake` changes.

`SelectExtent` needs nothing: Android reports the window size in `currentExtent`, and the no-window state
is covered by the `IsMinimized()` check. What it does not do is pre-rotation: `preTransform` is
`VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR` (`swap-chain.cpp:531`), and on a portrait device held in landscape
that costs the compositor a rotation pass every frame. Honouring `currentTransform` means telling the
caller to rotate its projection, which is an API change and a later item.

The validation layer comes from the Khronos Vulkan-ValidationLayers release that matches the 1.4.335
headers - the android binaries archive - and rides in the APK's `jniLibs/arm64-v8a/`. The loader takes
layers from a debuggable app's own library directory, so `RNDR_FORGE_VALIDATION` enables
`VK_LAYER_KHRONOS_validation` by name exactly as it does now and nothing is registered on the device.

Checked by: `[forge]` is headless and does not reach this code; `[forge-window]` does, and runs through
the runner APK in Phase 5. Until then the sample is the check, with `collect_debug_messages` on and the
message count in the title where the FPS is.

## Phase 3 — shaders and assets

**Compiling on the host.** `slangc` from the same Slang archive `dependencies.cmake` already fetches for
the host, run at build time over each `.slang` the sample uses, one `.spv` per entry point. A CMake function
(`rndr_compile_shader(target source entry_point stage)` in `cmake/shaders.cmake`) does it, and an Android
configure downloads the **host** archive for it, keyed on `CMAKE_HOST_SYSTEM_NAME` rather than on the target.

The invocation has to produce what `ShaderCompiler` produces, or the sample renders from different
bytes than the desktop does. Its options are few (`src/core/shader-compiler.cpp:447-463`): target
`spirv`, profile `spirv_1_5`, `SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY`, and
`VulkanUseEntryPointName` so the entry point keeps its name for SPIRV-Reflect. Which is
`-target spirv -profile spirv_1_5 -emit-spirv-directly -fvk-use-entrypoint-name -entry <name> -stage
<stage>`. Both lists live in one comment block that names the other, and a test keeps them equal (below).
The alternative - a host tool built from rndr's own `ShaderCompiler` - cannot drift, but a cross-compiling
configure cannot build a host tool, and two configures per build is a worse trade than one test.

**The cache without a compiler.** The `[forge]` suite compiles Slang source in `constexpr` strings beside
each case through `GetShaderCache()` (`test/forge/smoke-test.cpp:54`), a `ShaderCache` over a directory.
That directory, filled by a desktop run and pushed to the device, is how the suite runs there: every lookup
hits, and `FromSourceInMemory` never reaches the compiler it does not have. Two things stand in the way,
both small:

- The key carries Slang's build tag (`ShaderCacheKey::Make`, `shader-cache.cpp:82`), which a build without
  Slang cannot ask for, and the file name is a hash over it. So the cache directory gets a `build-tag`
  file, written by `Store` with the tag of the compiler that filled it, and the key is made by the cache
  (`ShaderCache::MakeKey`) rather than by a static: with a compiler the tag is Slang's, without one it is
  the file's. `Shader::FromSourceInMemory` asks the cache for its key. A directory without the file, on a
  build without a compiler, is a cache in which nothing hits, and the shader reports `FeatureNotSupported`.
- The suite's paths are host paths baked in at compile time (`RNDR_CORE_ASSETS_DIR "/../build/shader-cache"`,
  `smoke-test.cpp:56`, and two more at 6706 and 14740). They read an `RNDR_TEST_DATA_DIR` environment
  variable first and fall back to the macro, so `adb shell` can point them at what was pushed.

**Assets.** The APK's `assets/` holds what the sample loads today - the Suzanne glTF, its `.bin` and PNGs,
and the `.spv` files from above. `AndroidApplication::ExtractAssets(const char* asset_directory, const
Opal::StringUtf8& destination)` walks it through `AAssetManager` and writes the files under
`internalDataPath`, skipping a file already there with the same size; returns `ErrorCode`. The sample
calls it once and reads everything through the paths it already uses. `File`, assimp, KTX and stb are not
touched.

The sample gets one seam for both: an assets root that is the macro on the desktop and the extracted
directory on Android, and `Shader::FromSpirvFile` on Android where the desktop keeps `FromSource` - the
desktop path exercises the cache, and losing that coverage to make the sample uniform would be backwards.

Checked by:

- A `[forge]` case on the desktop, "slangc and ShaderCompiler produce the same module": compile one entry
  point of a source both ways - `ShaderCompiler` in process, `slangc` from `SLANG_RUNTIME_DIR` (it sits
  beside `slang.dll`) through `std::system` into a scratch file - and compare the bytes. Skips when
  `slangc` is not there. This is the test that pins the option lists to each other.
- A `[forge]` case, "a cache directory names the compiler that filled it": `Store` writes `build-tag`
  matching `spGetBuildTagString`, and a second `ShaderCache` over the same directory finds the entry.
  The compiler-less half of the lookup cannot run on the desktop, where Slang is always present; it is
  what the Phase 5 device run exercises.
- `FromSpirvInMemory` and `FromSpirvFile` are already covered (`smoke-test.cpp:14765`).

## Phase 4 — sample APK

`samples/android/` is a Gradle project - `settings.gradle.kts`, `build.gradle.kts`, `app/build.gradle.kts`,
`app/src/main/AndroidManifest.xml` - and nothing else: no Java, no Kotlin, `android:hasCode="false"`.

- `externalNativeBuild.cmake.path` is the repository's root `CMakeLists.txt`, with the Phase 0 arguments
  plus `RNDR_BUILD_SAMPLES=ON`, and `abiFilters` is `arm64-v8a` alone. `minSdk` 29: nothing lower has a
  Vulkan 1.3 driver, and 29 is where the NDK's Vulkan headers stop needing care.
- The manifest declares `android.app.NativeActivity` with `android.app.lib_name` = `modern-vulkan`,
  `configChanges="orientation|screenSize|keyboardHidden"` so a rotation is a resize rather than a restart,
  and `<uses-feature android:name="android.hardware.vulkan.version" android:version="0x403000"
  android:required="true"/>` so the store and the installer refuse a device Forge would refuse.
- `modern-vulkan` is `add_library(... SHARED)` under `if (ANDROID)` and links with
  `-u ANativeActivity_onCreate`, which keeps the glue's entry symbol from being dropped out of the static
  `librndr.a`. A `rndr_android_activity(target)` helper beside `rndr_deploy_runtime` carries that line so a
  downstream project does not have to know it.
- The validation layer `.so` goes into `app/src/main/jniLibs/arm64-v8a/`, fetched by a Gradle task from
  the pinned Khronos release rather than committed.
- Assets: a Gradle `sourceSets` entry points `assets/` at the repository's `assets/sample-models/Suzanne`
  and at the `.spv` output directory, so nothing is copied into the tree.

The sample itself: `android_main` sets up the desc, calls `Run()`, and returns. `Run()` gains the
`on_window_native_handle_change` handler that tears down and rebuilds the surface and the swap chain,
which is the one thing the desktop loop never had to do. `RNDR_CORE_ASSETS_DIR` in it becomes the assets
root from Phase 3.

    cd samples/android && ./gradlew installDebug
    adb logcat -s Rndr

Checked by hand, since none of it is reachable from a test binary - the list the Linux plan kept for the
same reason:

1. The app launches, Suzanne renders, the FPS in the log is plausible for the device.
2. A touch drag turns the camera (the mouse-look path), and no validation message is collected.
3. Home, then back to the app: the surface is rebuilt, rendering resumes, no validation message and no
   crash - this is the lifecycle decision from Context under load.
4. Rotate the device: the resize path, and the swap chain recreated at the new extent.
5. Back closes the app cleanly, with `android_main` returning rather than the process being killed.

## Phase 5 — tests on the device and CI

Two routes onto the device, because the loader gives a plain executable no validation layer:

- **The executable.** `RNDR_BUILD_TESTS=ON` under the NDK toolchain builds `rndr-test` as a position
  independent executable. Push it, the shader cache directory a desktop run filled, and what the cases
  read from `RNDR_TEST_DATA_DIR`, then run it from `adb shell` under `/data/local/tmp`. `[init]`,
  `[input]`, `[bitmap]`, `[fps]`, `[mesh]` run as they are; `[forge]` runs against the real GPU through the
  cache, and the cases that exercise compilation itself skip on a `ForgeTest::HasShaderCompiler()` probe.
  What this run cannot have is the validation layer: on a retail device the loader takes layers only for a
  debuggable app (and from `/data/local/debug/vulkan` on a userdebug build), so `REQUIRE_NO_VALIDATION_ERROR`
  passes on nothing there. That makes it a GPU smoke run - the readbacks still compare against the CPU
  values - and not the correctness run. Whether a particular device does load the layer for an executable
  is checked with `vulkaninfo` pushed the same way, not assumed.
- **The runner APK.** `samples/android/` gets a second module, a `NativeActivity` whose `android_main`
  extracts the cache and the data with `ExtractAssets`, runs `Catch::Session` with arguments from an intent
  extra (`adb shell am start ... --es args "[forge]"`), writes the report to `filesDir` for `adb pull`, and
  mirrors it to logcat. Debuggable, with the layer in `jniLibs`, so this is the validated run, and it is the
  one that reaches `[forge-window]` - the fixture's hidden, undecorated window is simply the activity's
  window there, and `Reshape` returning `FeatureNotSupported` makes the resize sections skip the way the
  X11 zero-size case does.

**CI.** An `android` job on `ubuntu-24.04`, compile-only like `linux-clang`: the runner image carries an
NDK (`ANDROID_NDK_LATEST_HOME`), the configure is the Phase 0 line with `RNDR_BUILD_TESTS=ON` and
`RNDR_BUILD_SAMPLES=ON`, and the build target is everything, so the whole Android surface meets `-Werror`
on every push. No run: the runners have no device, and the emulator's software Vulkan under KVM is a
later experiment whose 1.3 support has to be verified first.

Checked by: the executable run green on a device with `RNDR_TEST_REQUIRE_VULKAN=1`, the runner APK's
`[forge]` and `[forge-window]` green with the layer loaded (the log shows the layer's own banner, which is
the equivalent of CI's `vulkaninfo` check), and the CI job green.

## Out of scope (explicit, so nothing half-lands)

Canvas on Android (GLES against a GL 4.5 API); an AAudio device behind `AudioDevice::Create`
(`src/audio/audio-device.cpp:7` is where it slots); gamepad (same event stream, two tables); touch as its
own input primitive (multi-touch, gestures, pressure); text input and `OnCharacter` (JNI); clipboard (JNI);
`imgui-system` on Android (`imgui_impl_android.cpp` exists upstream); pre-rotation; immersive mode behind
`SetMode(BorderlessFullscreen)`; HWASan; GameActivity; the `x86_64` ABI for the emulator; Slang on the
device. Each slots behind the seams above later.
