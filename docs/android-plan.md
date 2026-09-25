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
| 0 — toolchain and portability groundwork | Done |
| 1 — AndroidApplication + AndroidWindow | Compiles; not run on a device |
| 2 — Forge surface | Compiles; not run on a device |
| 3 — shaders and assets | Host side checked on Windows and Linux; asset extraction not run on a device |
| 4 — sample APK | The APK builds; not installed - no device |
| 5 — tests on the device and CI | `rndr-test` builds for Android and the CI job is written; runner APK and device runs not started |

Machine state on 2026-09-25: SDK at `F:\Android\Sdk` with NDK 30.0.16248370, platform-tools and the
android-37 platform; JDK 21 at `F:\Program Files\Java\jdk-21.0.12.1`. All three variables are set for the
user, not the machine, so a shell opened before the install still sees `JAVA_HOME` at JDK 10 and no
`ANDROID_HOME`. The SDK's own `cmake` package is not installed: the configure below runs the CMake and Ninja
that Visual Studio ships, and Gradle's `externalNativeBuild` (Phase 4) fetches the package itself (CMake 3.31.6
since Phase 4 asks for it). No device is attached and no emulator image is installed, so nothing below has run
on Android yet.

## Phase 0 — toolchain and portability groundwork

Install, in this order: Android Studio or the command-line tools, then through its SDK manager the current
stable NDK (r30 at the time of writing; nothing here depends on the version), platform-tools (`adb`) and
the platform for `targetSdk`; JDK 21 (the LTS Studio bundles - a newer JDK is only as usable as the Gradle
that runs on it); set `ANDROID_HOME` and `ANDROID_NDK_HOME`.
A device with developer mode and USB debugging on, or nothing yet - the phase ends at a build, not a run.

Groundwork in the tree, none of it Android code yet:

- `include/rndr/definitions.hpp`: an `RNDR_ANDROID` branch checked **before** the Linux one. Android
  defines `__linux__`, so Opal's `defines.h` says `OPAL_PLATFORM_LINUX` there and rndr's
  `#elif defined(OPAL_PLATFORM_LINUX)` would pull in the XCB backend. The check is `__ANDROID__`. The
  macro set (`RNDR_DEBUG_BREAK`, `RNDR_FORCE_INLINE`, `RNDR_ALIGN`, `RNDR_OPTIMIZE_OFF/ON`) is the Clang
  half of the Linux branch. `OPAL_PLATFORM_LINUX` stays the right answer for Opal's own
  sources, which are Linux code Bionic mostly accepts (futex through `syscall`, pthread, `dirent`, BSD
  sockets, `mmap`). An `OPAL_PLATFORM_ANDROID` upstream is a nicety, not a need, and would have to keep
  the Linux define alongside it or break Opal's own `#if`s. "Mostly" is the first finding below.
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

What the first NDK build turned up (2026-09-25, NDK 30, Clang 21):

- **Opal 0.6.3 does not build for arm64.** `hash-table-base.h` included `<emmintrin.h>` unconditionally
  for the SSE2 group probe, so every translation unit that reaches a hash map failed; and `thread.cpp`
  called `pthread_setaffinity_np`, which Bionic does not have. Both are fixed in Opal 0.6.4, which rndr
  pins - a NEON probe on aarch64 with a scalar loop for anything else, and `sched_setaffinity` on the
  kernel thread id the handle already carries - checked by Opal's full suite under `qemu-aarch64` (a
  clang-20 cross build in WSL against Ubuntu's arm64 cross sysroot, unpacked without root) and on MSVC
  and GCC. To try an Opal change before it is released, an arm64 configure adds
  `-DCPM_opal_SOURCE=D:/Dev/opal`, and it has to be `--fresh`: a build directory that already fetched
  Opal keeps compiling the fetched copy.
- The same qemu run fails two Opal cases that have nothing to do with the port. "Matrix 4x4 inverse
  operator / duplicated row" expects `InvalidArgument` for a singular matrix and gets a value: Clang
  contracts the determinant into FMAs on aarch64, the result is a rounding error away from zero rather than
  zero, and with `-ffp-contract=off` the case passes. That is an Opal bug on any FMA target, device
  included. "Socket UDP over loopback / empty datagram" looks like qemu-user refusing a null buffer of
  length zero, and needs a device to say for sure.
- rndr's own sources needed nothing beyond what is listed above. With Opal fixed, the arm64 build of
  `rndr` fails in exactly three translation units, all by design: `application.cpp` and
  `platform-application.cpp` have no backend to construct (Phase 1) and `swap-chain.cpp` no surface to
  create (Phase 2). So `librndr.a` links at the end of Phase 2, not this phase, and this phase is checked
  by those three being the only failures. An `x86_64` Android configure, where SSE2 exists, gives the
  same three with the released Opal.

On a Windows host the configure is the one above with the NDK's toolchain file by its Windows path, and
`-G Ninja` finds the `ninja.exe` Visual Studio ships under `Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja`
once that is on `PATH`.

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

As built (2026-09-25), where it differs from the above or the above left it open:

- The desc field is `ApplicationDesc::android_application`. A member named `android_app` of type `android_app*`
  changes the meaning of the type name inside the class, which GCC rejects.
- `~AndroidApplication` finishes the activity if nothing did, then pumps until `destroyRequested`, with the
  callbacks unhooked. The glue's activity thread waits on `android_main`'s thread for every window and input
  queue change, so an `android_main` that returns while the activity lives leaves that thread waiting for good.
- `CloseWindow` is the one close path - back and `RequestClose` both - and finishes the activity when the
  handler does not veto. `APP_CMD_DESTROY` (swiped away from recents) cannot be refused, so a veto there is
  logged and overruled.
- A mouse's buttons are diffed from `AMotionEvent_getButtonState` on every event, since naming the button an
  `ACTION_BUTTON_PRESS` is about takes `AMotionEvent_getActionButton`, which is API 33.
- The glue's include directory is public on Android: the application's `android_main` reads `android_app`.
- `ExtractAssets` (Phase 3) lives here too, beside `WaitForNativeWindow` and `CloseWindow`.

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
is covered by the `IsMinimized()` check. The swap chain is pre-rotated: `preTransform` is the surface's
`currentTransform`, the textures are created in the display's natural orientation (the extent turned back
for 90 and 270), and `SwapChain::GetRotation` / `GetPreRotation` tell the caller to turn its projection to
match. Identity was not an option on Android: a surface whose `preTransform` differs from
`currentTransform` is reported suboptimal on every present, and a phone held sideways recreated its swap
chain every frame until the process aborted. Desktop surfaces report identity, so nothing changes there.

The validation layer comes from the Khronos Vulkan-ValidationLayers release that matches the 1.4.335
headers - the android binaries archive - and rides in the APK's `jniLibs/arm64-v8a/`. The loader takes
layers from a debuggable app's own library directory, so `RNDR_FORGE_VALIDATION` enables
`VK_LAYER_KHRONOS_validation` by name exactly as it does now and nothing is registered on the device.

Checked by: `[forge]` is headless and does not reach this code; `[forge-window]` does, and runs through
the runner APK in Phase 5. Until then the sample is the check, with `collect_debug_messages` on and the
message count in the title where the FPS is.

As built: as above. The one thing added is that `Surface::Create` answers `PlatformError` for a window with no
native handle, and says so in `swap-chain.hpp`.

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

As built (2026-09-25):

- The option list is `RNDR_SLANGC_OPTIONS` in `cmake/shaders.cmake`, and the test is handed that same list as
  `RNDR_TEST_SLANGC_OPTIONS` rather than a copy of it, so it pins what the build runs. "Forge slangc and
  ShaderCompiler produce the same module" passes on Windows and on Linux: the bytes are identical. With
  `-fvk-use-entrypoint-name` dropped from the list it fails, so it has teeth.
- The host compiler is `RNDR_SLANGC`: from the Slang archive already fetched when the target is the host, from
  a second `slang-host` package keyed on `CMAKE_HOST_SYSTEM_NAME` when it is not. `rndr_compile_shader` writes
  under `RNDR_SPIRV_DIR`, default `${CMAKE_BINARY_DIR}/spirv`, settable so the Gradle build can name it.
- `ShaderCache::MakeKey` and the `build-tag` file are as planned. `Store` also rewrites the file when it has gone
  missing, not only when the tag changed - the new section of "Forge shader cache" found that. `ShaderCacheKey::Make`
  stays, and is what `MakeKey` calls on a build with the compiler.
- The three suite paths go through `ForgeTest::GetTestDataPath` (`forge-test-common.hpp`), which reads
  `RNDR_TEST_DATA_DIR` and falls back to the build directory.
- `ExtractAssets` is not recursive - `AAssetDir` lists the files of a directory and not its subdirectories - and
  skips a file already there with the same size. The sample's assets are flat, so one call does.

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

As built (2026-09-25) - the APK builds; none of the list above has run:

- The module is `samples/android/modern-vulkan/` rather than `app/`, since Phase 5's runner is the second module
  of the same project. Gradle 9.8.0 through a committed wrapper, AGP 9.4.1, compile and target SDK 37, JDK 21.
- `externalNativeBuild` pins CMake 3.31.6: AGP otherwise fetches 3.22.1, and rndr needs 3.28.
- Built and installed with `JAVA_HOME` at JDK 21 and `ANDROID_HOME` set:

      cd samples/android
      ./gradlew assembleDebug
      adb install -r modern-vulkan/build/outputs/apk/debug/modern-vulkan-debug.apk

- `rndr_android_activity` is `LINKER:--undefined=ANativeActivity_onCreate` plus an empty `DEBUG_POSTFIX`:
  assimp sets `CMAKE_DEBUG_POSTFIX` to `d` for everything, and the activity loads the library by exact name.
- AGP merges assets before it runs the native build, and the SPIR-V is written by the native build, so
  `mergeDebugAssets` is made to depend on `externalNativeBuildDebug`. The layer comes from a
  `fetchValidationLayer` task into the debug `jniLibs`.
- The debug APK is 33 MB: the validation layer is 23.7 MB of it, `libmodern-vulkan.so` 5.7 MB stripped. The
  library needs nothing but system libraries - libc++ is static and volk opens `libvulkan.so` itself.
- The sample's handler holds what it rebuilds through one pointer: Opal's delegates keep the callable inline in
  32 bytes, and a capture of the seven objects does not fit. In the background, with no frame context, the loop
  waits in `ProcessSystemEvents(k_infinite_timeout)` instead of spinning. The camera starts steerable, since a
  phone has no F1, and the title line goes to the log every two seconds.

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

As built so far (2026-09-25):

- `rndr-test` builds and links for arm64. The one thing in the suite that called the compiler directly,
  `CompileToSpirv`, takes the code from the suite's shader cache on a build without one, and fails there if the
  pushed cache lacks the entry.
- The `android` CI job is in `ci.yml`, compile-only, arm64, tests and samples on. It needs Opal 0.6.4, the
  first release that builds for arm64, which is what rndr pins.
- Not started: the runner APK, `ForgeTest::HasShaderCompiler()` and the skips behind it - the first device run
  says which cases need one - and every run on a device.

## Out of scope (explicit, so nothing half-lands)

Canvas on Android (GLES against a GL 4.5 API); an AAudio device behind `AudioDevice::Create`
(`src/audio/audio-device.cpp:7` is where it slots); gamepad (same event stream, two tables); touch as its
own input primitive (multi-touch, gestures, pressure); text input and `OnCharacter` (JNI); clipboard (JNI);
`imgui-system` on Android (`imgui_impl_android.cpp` exists upstream); pre-rotation; immersive mode behind
`SetMode(BorderlessFullscreen)`; HWASan; GameActivity; the `x86_64` ABI for the emulator; Slang on the
device. Each slots behind the seams above later.
