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
| 2 — Forge surface | **Done**: the XCB surface builds, and `[forge]`/`[forge-window]` run against a real device |
| 3 — build and verify in WSL2 | Tests green; manual sample checks and an upstream Mesa report remain |

Verified on 2026-08-24 in WSL2 Ubuntu 24.04 with GCC 13 and Ninja. With Forge off and ASan on, all 92
test cases pass. With Forge on (`VULKAN_SDK` pointed at the Windows SDK, `RNDR_HARDENING=OFF`,
`RNDR_TEST_REQUIRE_VULKAN=1`), `[forge]` reports 74 of 79 cases passing with 5 skipped, and
`[forge-window]` 5 of 6 with 1 skipped (the empty-client-area case, which X11 cannot express - see
"Fixed along the way"). The device is Mesa's **llvmpipe** software
rasterizer - there is no dozen (`dzn`) ICD on this box - so these runs prove correctness against
the validation layer, not against a GPU.

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

## Phase 2 — Forge surface (done)

- `src/forge/graphics-context.cpp` `GetRequiredInstanceExtensions` pushes
  `VK_KHR_XCB_SURFACE_EXTENSION_NAME` under `OPAL_PLATFORM_LINUX`, guarded like the win32 one.
- `src/forge/swap-chain.cpp` `Surface::Create` fills a `VkXcbSurfaceCreateInfoKHR` from
  `GetNativeDisplayHandle()` (the connection) and `GetNativeHandle()` (the window id) and calls
  `vkCreateXcbSurfaceKHR` through the same `RNDR_FORGE_VK_CHECK_EXPECTED` macro.

Forge met GCC for the first time here, which took a warning policy decision and one real bug fix:

- `-Wmissing-field-initializers` and `-Wsign-compare` (843 and 63 reports) are switched off for
  GCC/Clang in `cmake/compiler-warnings.cmake`. Both fire on idioms the codebase uses throughout -
  partially initialized Vulkan structs and `i32` loop indices against a `u64` GetSize() - and MSVC
  accepts both at /W4 /WX, so turning them into 900 edits would be churn rather than a fix.
- The Vulkan SDK include directories are `SYSTEM` now, and `spirv_reflect.c` is compiled with `-w`:
  VMA and SPIRV-Reflect are vendored SDK code, not ours to edit.
- Real fix: `Rndr::u64` is `unsigned long long` while Linux `uint64_t` and `VkDeviceSize` are
  `unsigned long`. Same width, different type, so a `u64*` does not convert - four call sites in
  `synchronization.cpp` and `command-buffer.cpp` now hand Vulkan a variable of its own type. This is
  a portability bug that Windows cannot see, since both types are `unsigned long long` there.

**The SDK question is settled**: point `VULKAN_SDK` at the Windows install from inside WSL and the
build needs no change. Everything Forge takes from the SDK is portable text - `Include/` (headers,
including `vulkan_xcb.h`, plus the bundled volk and VMA) and `Source/SPIRV-Reflect/spirv_reflect.{h,c}`.
Both compile under GCC straight off the `/mnt/c` mount, whose case-insensitivity covers the
`Include`/`Source` capitalization. No Vulkan library is ever linked: `volk-implementation.cpp` means the
loader is dlopened at run time, so the Windows import library is never wanted and apt's `libvulkan1`
serves. Validation likewise comes from apt (`vulkan-validationlayers`); the SDK's layer DLLs are
Windows binaries and are neither usable nor needed.

    VULKAN_SDK=/mnt/c/VulkanSDK/<version> cmake -S . -B build/linux-debug -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug -DRNDR_FORGE=ON -DRNDR_FORGE_VALIDATION=ON -DRNDR_ASSIMP=ON -DRNDR_KTX=OFF

Header/loader version skew is fine as long as `GraphicsContext` keeps asking for `VK_API_VERSION_1_3`
(`src/forge/graphics-context.cpp:273`): SDK 1.4 headers against an apt 1.3 loader only declare entry
points the code never calls. Reading headers across the `/mnt/c` 9p mount is slow, so copying
`Include/` and `Source/SPIRV-Reflect/` into the WSL filesystem is a fair trade if compile times bite.

## Phase 3 — build and verify in WSL2 (partially done)

Done:

- `build/linux-debug` configured with Ninja/GCC and `RNDR_FORGE=OFF`; apt deps installed
  (`libxcb1-dev libxcb-randr0-dev libxcb-xfixes0-dev libxcb-xkb-dev libxkbcommon-dev
  libxkbcommon-x11-dev`).
- `./build/linux-debug/rndr-test` — 92 test cases, 395 assertions, all passing, ASan clean.
- Windows regression: `build/msvc-debug` full build plus `rndr-test.exe`, 219 passed / 1 skipped.

Remaining:

1. Run the `modern-vulkan` sample under WSLg: window appears, resizes, ESC closes, WASD+mouse fly camera
   works (exercises the ResetToCenter warp, motion deltas and key translation).
2. Window behaviours that no test tag covers, driven manually since `window-sample` is Canvas-bound and
   does not build on Linux: fullscreen toggle, minimize/maximize/restore, title, opacity, always-on-top,
   taskbar visibility, the resize lock, and the close veto. Either extend `modern-vulkan` with a few
   key bindings or add a small Forge-free windowing sample.
3. Multi-monitor `GetMonitors` sanity check on a real X11 session - WSLg reports a single monitor, so
   the RandR path is only lightly exercised so far.
4. Report the llvmpipe blend-constant rotation below to Mesa, ideally with a Forge-free repro.

### Fixed along the way

`LinuxWindow::Reshape` waits for the window manager to apply the new size before returning (polling the
geometry, half a second at most, a warning if it expires). `xcb_configure_window` is a request rather
than a change, while the Windows `MoveWindow` it has to match has already applied by the time it
returns, so a caller that read the size straight back read the old one - which is exactly what the
swap-chain resize test does. The same function now rejects a zero width or height with
`InvalidArgument`, since X11 answers `BadValue` for a window without a client area.

The empty-client-area swap chain cases (`test/forge/window-test.cpp`, "no client area") now probe the
platform instead of assuming it: `ForgeWindowFixture::ResizeWindow` returns what `Reshape` said, and a
refusal skips the section. X11 has no window without a client area - a zero size is a protocol error,
and an iconified window keeps its geometry, so the surface never reports the zero extent the cases are
about. The recovery logic they cover is platform-neutral and stays covered by the Windows run.

`SwapChain` also treats a minimized window as having no client area regardless of what the surface
reports (`SelectExtent` asks `GenericWindow::IsMinimized` first). Windows reports a zero
`currentExtent` for a minimized window on its own; an iconified X11 window keeps its last geometry, and
without the check Forge would keep presenting into a window nobody can see. Not testable in `[forge-window]` -
minimize needs a window manager managing a mapped window, and the fixture's window is deliberately never
shown - so it belongs to the manual checks in item 3 above: verify rendering pauses while minimized and
resumes on restore.

The uint8 index-type validation error was never the missing-extension bug it looked like. Forge did
enable the extension, but it preferred the VK_KHR_index_type_uint8 name where the device offers both,
and the apt validation layer (1.3.275) predates that promotion, so it did not credit the KHR name with
VK_INDEX_TYPE_UINT8 and reported every 8-bit bind as invalid. Mesa 25.2's llvmpipe advertises both
names. `FindIndexTypeUint8Extension` now prefers the EXT name - same feature structure, same index
type value, and every layer knows it - with KHR kept as the fallback for a driver that one day drops
the EXT name. Invisible on Windows only because the newer validation layer there knows both names.

### The blend constants arrive rotated on llvmpipe (diagnosed, worked around, unreported upstream)

The ConstColor failure was a driver bug, not a Forge one, and bigger than it looked. On llvmpipe
(Mesa 25.2.8, LLVM 20.1.2) the blend constants arrive rotated one channel to the right: a constant of
(r, g, b, a) blends as (a, r, g, b), so ConstColor weighs red by the constant's alpha, green by its
red, and so on. Measured by splitting the colour and alpha factors apart in a scratch section: every
one of the four constant factors is wrong the same way - ConstAlpha splats the constant's blue, the
inverses are rotated too - and the original run only ever reported ConstColor because `REQUIRE` aborts
a section at its first failure, so the factors after it never ran.

What ruled Forge out: the constants are submitted in spec order (`src/forge/pipeline.cpp:874`, checked
against the struct), validation is clean, feeding them through `VK_DYNAMIC_STATE_BLEND_CONSTANTS` and
`vkCmdSetBlendConstants` instead produces the identical rotation, `LP_NATIVE_VECTOR_WIDTH=128` changes
nothing, and the same code blends correctly on a Windows GPU. Mesa's own state plumbing reads
correctly (lavapipe memcpys the four floats, `lp_setup.c` stores them r,g,b,a), so the rotation lives
in llvmpipe's JIT blend codegen. No matching upstream issue found; reporting it is on the remaining
list.

The regression window is bracketed by CI: the GitHub runners have no GPU, so `ci.yml` registers
mesa-dist-win **24.3.2**'s lavapipe as the Vulkan driver on the Windows runners, and master is green
there with this test in it - 24.3.2 blends the constants correctly, so the bug arrived between Mesa
24.3.2 and 25.2.8.

The blend-factor test now probes for the rotation at run time - one ConstColor blend compared against
the rotated model - and only where it measures it does it leave the four constant factors out, with a
warning; the ten factors that never read the constants keep running regardless. Probing rather than
matching the device name is what keeps CI's older lavapipe fully covered, and hands the coverage back
on any Mesa that fixes it without anyone editing a version check. The "constants are not zero"
regression section still runs everywhere - rotated constants are still not zero, and it guards the
case where they are dropped entirely.

## Running the Forge tags on Linux

Three environment traps, all of which cost time once already:

- **Do not run the Forge tags from an ASan build.** llvmpipe renders into system memory and ASan's
  redzones and quarantine multiply it, which took the WSL VM to its 15.5 GB ceiling and left the whole
  distro unreachable. Configure with `-DRNDR_HARDENING=OFF`, matching CI, and peak resident memory is
  about 166 MB.
- **Do not cap the address space with `ulimit -v`.** Mesa reserves large virtual ranges up front and
  blocks rather than failing when it cannot, which looks exactly like a hung test at 0% CPU.
- **Do not write logs to `/tmp`.** The distro shuts itself down once the last process exits and `/tmp`
  is tmpfs, so logs vanish between runs. Write them into the build directory instead.

## Out of scope (explicit, so nothing half-lands)

Wayland-native backend; Canvas GL on Linux (GLX/EGL); gamepad via evdev; Linux audio device;
`imgui-system` on Linux; XInput2 raw mouse (`EnableHighPrecisionCursorMode` tracks the flag, deltas come
from regular motion events). Each slots behind the same seams later.
