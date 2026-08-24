# Linux windowing (X11/XCB) plan

## Context

The platform layer was Windows-only: `Application`'s constructor hit `#error "Platform not supported!"`
on anything else, and Forge hardcoded `vkCreateWin32SurfaceKHR`. Goal of this milestone: a raw XCB
backend behind the existing `PlatformApplication`/`GenericWindow` seam, good enough to run `[init]`,
`[input]`, `[forge]`, `[forge-window]` and the `modern-vulkan` sample on Linux, developed and tested in
WSL2 (WSLg gives X11 via XWayland plus GPU Vulkan through the dozen driver).

Decisions:

- **X11 first** — works everywhere via XWayland; a Wayland-native backend can come later behind the
  same seam. Wayland-first was rejected because xdg-shell forbids global cursor position, `MoveTo` and
  `SetCursorPosition`, which the `GenericWindow` contract relies on.
- **Raw XCB** + `libxkbcommon` — house style; the repo wraps Win32, WASAPI and Vulkan directly, so no
  SDL/GLFW middleware.
- **Scope**: window + input + monitors + Forge surface. Canvas GL, gamepad (evdev), audio, Wayland and
  ImGui-on-Linux are explicitly out (see the last section).
- Error reporting follows the repo convention throughout: static `Create` returning
  `Opal::Expected<T, ErrorCode>`, methods return `ErrorCode` (`WindowAlreadyClosed`, `PlatformError`,
  `InvalidArgument`), detail to the log, nothing throws.

## Status

| Phase | State |
|---|---|
| 0 — portability groundwork | **Done** |
| 1 — LinuxApplication + LinuxWindow | **Done** |
| 2 — Forge surface | Not started, blocked on the Vulkan SDK question below |
| 3 — build and verify in WSL2 | Partially done: `build/linux-debug` builds and all 92 test cases pass with Forge OFF |

Verified on 2026-08-24: WSL2 Ubuntu 24.04, GCC 13, Ninja, ASan, configured
`-DRNDR_FORGE=OFF -DRNDR_ASSIMP=ON -DRNDR_KTX=OFF`. `[init]` and `[input]` pass under WSLg, and the
Windows `build/msvc-debug` still builds clean with all tests passing (219 passed, 1 skipped).

## Phase 0 — portability groundwork (done)

- `include/rndr/definitions.hpp`: `RNDR_LINUX` branch for `RNDR_DEBUG_BREAK` (`__builtin_trap()`),
  `RNDR_FORCE_INLINE`, `RNDR_ALIGN`, `RNDR_OPTIMIZE_OFF/ON`; the bare `#pragma warning(disable : 4231)`
  and the `RNDR_EXTERN_TEMPLATE` block are behind `RNDR_WINDOWS`.
- `cmake/compiler-warnings.cmake`: GCC/Clang warning list plus `-Werror`, behind a `$<CXX_COMPILER_ID>`
  genex like the MSVC one.
- `cmake/compiler-options.cmake`: `/MP` and `-DUNICODE=1` are MSVC-only now; they were reaching GCC.
- `cmake/sanitizers.cmake`: `RNDR_HARDENING` on non-MSVC is `-fsanitize=address -g` with the same flag
  at link time (no DLL copy step).
- Top-level `CMakeLists.txt`: on `UNIX`, force `RNDR_CANVAS=OFF` and `RNDR_AUDIO=OFF` with a status
  message; `window-sample` and `text-rendering-sample` are gated on `RNDR_CANVAS` since both render
  through it.
- `src/CMakeLists.txt`: platform sources split into `if (WIN32) / elseif (UNIX)` blocks, with
  `imgui-system.{hpp,cpp}` (includes `imgui_impl_win32.h`), glad and the `User32/Shcore/Xinput` links on
  the Windows side and the Linux platform sources plus `PkgConfig::RNDR_XCB` on the other;
  `VK_USE_PLATFORM_XCB_KHR` replaces the win32 define on Linux.
- `cmake/dependencies.cmake`: `pkg_check_modules(RNDR_XCB REQUIRED IMPORTED_TARGET xcb xcb-randr
  xcb-xfixes xcb-xkb xkbcommon xkbcommon-x11)` behind `UNIX`. Slang already had the Linux download path.

### Portability fixes the first GCC build turned up

Not in the original plan; all of them are shared code that only ever saw MSVC:

- `src/file.cpp` / `include/rndr/file.hpp`: `fopen_s` and `struct _iobuf*` are MSVC-only. The handle is
  a plain `FILE*` now and opening goes through one `OpenFile` helper. `file.hpp` also included
  `canvas/renderers/pbr-renderer.hpp` unconditionally, which does not exist in a Canvas-off build.
- `src/canvas/projections.cpp` moved out of the `RNDR_CANVAS` block: it is pure math with no GL, and
  `projection-camera.cpp` (always built) calls `Canvas::Perspective`/`Canvas::Orthographic`.
- Vendored headers (catch2, glad, stb_image) are included as `SYSTEM` so their warnings do not hit
  `-Werror`.
- `extern/stb_image/include/stb_image/stb_image_write.h`: the non-`__STDC_LIB_EXT1__` branch called
  `sprintf_s`, which only exists on MSVC. In-repo edit to a vendored header - re-apply on any update.
- `include/rndr/math.hpp` includes `<cstdint>`: Opal's `bounds2.h`/`bounds3.h` use `uint8_t` without
  including it, and GCC's headers do not leak the declaration the way MSVC's do. Worth fixing upstream
  in opal instead.
- Warning fixes: `fly-camera.cpp` member init order, `pixel-format.cpp` missing `EnumCount` case,
  `trace.cpp` unused parameter in the Canvas-off path, sign-compare in `application.cpp` and
  `platform-application.cpp`.

Two Opal behaviours worth remembering: `Opal::Ref` has a deleted copy assignment (assign `.GetPtr()`),
and `MonitorInfo` is not copyable because `Opal::StringUtf8` is not, so it has to be moved out of a
local array.

## Phase 1 — LinuxApplication + LinuxWindow (done)

- `include/rndr/platform/linux-application.hpp`, `src/platform/linux-application.cpp`
- `include/rndr/platform/linux-window.hpp`, `src/platform/linux-window.cpp`

`LinuxApplication` owns the `xcb_connection_t*`, the `xkb_context`/keymap/state and the interned atoms;
windows borrow them, the way the Win32 pair shares the window class. Its destructor drains the window
list before `xcb_disconnect`, because the base destructor would otherwise tear windows down against a
dead connection.

`GenericWindow::GetNativeDisplayHandle()` was added as the one new virtual: `xcb_connection_t*` on
Linux, `GetModuleHandle(nullptr)` on Windows. That keeps `Forge::Surface::Create(context, window)`
unchanged for Phase 2.

Mapping of the `GenericWindow` API onto X11 came out as planned (`_NET_WM_STATE` for
fullscreen/maximize/above/skip-taskbar, `_MOTIF_WM_HINTS` for decorations and functions,
`WM_CHANGE_STATE` for iconify, `WM_NORMAL_HINTS` min==max for the resize lock,
`_NET_WM_WINDOW_OPACITY` for opacity, `xcb-randr` for monitors, `Xft.dpi` for the DPI scale). Three
places needed something the plan did not spell out:

- **Repeat detection**: `xcb_xkb_per_client_flags` with `DETECTABLE_AUTO_REPEAT` makes a press while the
  key is already down mean a repeat, which is the signal `WM_KEYDOWN`'s repeat bit carries.
- **Double clicks**: X11 has none, so two presses of the same button within 500 ms and 4 px are folded
  into `OnMouseDoubleClick`.
- **Key translation**: a shifted keysym names a different symbol than the key (colon vs semicolon), so
  translation falls back to level 0 of the same key before reporting the keysym as unsupported.
- `xcb/xkb.h` names a struct field `explicit`, so the include is wrapped in
  `#define explicit explicit_field` / `#undef explicit`.

`SetCloseSupported(false)` is tracked and vetoed in the `WM_DELETE_WINDOW` handler (X11 cannot gray out
a close button), and `Enable(false)` drops that window's input events in the pump.

## Phase 2 — Forge surface (remaining)

- `src/forge/graphics-context.cpp` `GetRequiredInstanceExtensions` — push
  `VK_KHR_XCB_SURFACE_EXTENSION_NAME` under `RNDR_LINUX`, guarded like the win32 one
  (`src/forge/graphics-context.cpp:468`).
- `src/forge/swap-chain.cpp` `Surface::Create` (line 51) — `VkXcbSurfaceCreateInfoKHR{ connection =
  window.GetNativeDisplayHandle(), window = window.GetNativeHandle() }` and `vkCreateXcbSurfaceKHR`
  through the same `RNDR_FORGE_VK_CHECK_EXPECTED` macro.

**Blocked on a build question**: `src/CMakeLists.txt` compiles SPIRV-Reflect out of
`$ENV{VULKAN_SDK}/Source/SPIRV-Reflect`, and it puts `${VULKAN_SDK_PATH}/Include` on the public include
path. The WSL box has apt's `libvulkan-dev` (headers in `/usr/include/vulkan`) and no SDK, so a
Forge-on-Linux configure needs one of: install the LunarG SDK tarball in WSL and keep the current
layout, or teach the build to fall back to system Vulkan headers plus a vendored/CPM SPIRV-Reflect.
Decide this before writing any Phase 2 code - it decides whether `VULKAN_SDK` stays required.

## Phase 3 — build and verify in WSL2 (partially done)

Done:

- `build/linux-debug` configured with Ninja/GCC and `RNDR_FORGE=OFF`; apt deps installed
  (`libxcb1-dev libxcb-randr0-dev libxcb-xfixes0-dev libxcb-xkb-dev libxkbcommon-dev
  libxkbcommon-x11-dev`).
- `./build/linux-debug/rndr-test` — 92 test cases, 395 assertions, all passing, ASan clean.
- Windows regression: `build/msvc-debug` full build plus `rndr-test.exe`, 219 passed / 1 skipped.

Remaining:

1. Reconfigure with `-DRNDR_FORGE=ON -DRNDR_FORGE_VALIDATION=ON` once the SDK question above is
   settled, then run `[forge]` and `[forge-window]` with `RNDR_TEST_REQUIRE_VULKAN=1` so a missing
   device fails loudly instead of skipping.
2. Run the `modern-vulkan` sample under WSLg: window appears, resizes, ESC closes, WASD+mouse fly camera
   works (exercises the ResetToCenter warp, motion deltas and key translation).
3. Window behaviours that no test tag covers, driven manually since `window-sample` is Canvas-bound and
   does not build on Linux: fullscreen toggle, minimize/maximize/restore, title, opacity, always-on-top,
   taskbar visibility, the resize lock, and the close veto. Either extend `modern-vulkan` with a few
   key bindings or add a small Forge-free windowing sample.
4. Multi-monitor `GetMonitors` sanity check on a real X11 session - WSLg reports a single monitor, so
   the RandR path is only lightly exercised so far.

## Out of scope (explicit, so nothing half-lands)

Wayland-native backend; Canvas GL on Linux (GLX/EGL); gamepad via evdev; Linux audio device;
`imgui-system` on Linux; XInput2 raw mouse (`EnableHighPrecisionCursorMode` tracks the flag, deltas come
from regular motion events). Each slots behind the same seams later.
