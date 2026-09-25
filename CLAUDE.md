# rndr

Two rendering APIs live side by side: **Canvas** (OpenGL 4.5, high level, on by default) and **Forge**
(Vulkan, low level, `RNDR_FORGE=OFF` by default). Current work is on Forge.

`Opal::` is first-party — [praetorian555/opal](https://github.com/praetorian555/opal), fetched by CPM at
configure time. It is not a third-party dependency, and it is not edited from this repo.

Read its source locally rather than on GitHub, which serves whatever `main` is today and not the commit
this build is pinned to. CPM unpacks it under a hash-named directory that changes with the pin, so ask the
generated cache where it went:

    grep OPAL_SOURCE_DIR build/msvc-debug/CMakeCache.txt

Worth doing before relying on any `Opal::` behaviour that is not obvious from the call site — whether a
container's size constructor value-initializes, whether a move steals the buffer, what `GetData()` returns.
Guessing at those produces code that compiles and is wrong.

## Read before editing

- [docs/forge.md](docs/forge.md) — conventions that hold across all of `src/forge/`: the empty-state /
  `IsValid()` contract, and the error strategy. Nothing in Forge throws. Objects are built by a static
  `Create` returning `Opal::Expected<T, Rndr::ErrorCode>`, anything else fallible returns an `Expected` or an
  `ErrorCode`, and the detail goes to the log. Canvas reports the same way — see the error handling section
  of [docs/canvas.md](docs/canvas.md).

- [docs/audio.md](docs/audio.md) — `src/audio/` reports the same way and got there first. `Rndr::ErrorCode` is
  shared between the two, so a code added for one is visible to the other.

- [docs/forge-api-gaps.md](docs/forge-api-gaps.md) — what the Forge API cannot express yet, in the order
  to close it, each with what a caller does about it today and what closing it takes. Read it before adding
  a Forge feature, since the item is usually already there with its test sketched, and before writing a
  Forge sample, since it says which workaround the sample will need. The test items the third suite review
  left open are at the end.

## Building here

There is no `CMakePresets.json`. Build directories in use: `build/msvc-debug`, `build/msvc-release`,
`build/msvc-test`.

Forge is off by default, so a stock configure compiles none of `src/forge/` and Forge changes appear to
build cleanly while being skipped entirely:

    cmake -S . -B build/msvc-debug -DRNDR_FORGE=ON -DRNDR_ASSIMP=ON -DRNDR_FORGE_VALIDATION=ON
    cmake --build build/msvc-debug --config Debug --target rndr-test

Always configure with `RNDR_ASSIMP=ON` and `RNDR_FORGE_VALIDATION=ON`. Without assimp the Forge sample dies
at mesh loading; without the validation layer a run proves nothing about correctness.

`RNDR_HARDENING` defaults to ON and instruments the build with AddressSanitizer. CI configures with it
OFF, and hardened builds refuse to install.

Android is a cross build with the NDK's toolchain file (`build/android-debug`) and, for the sample APK, a
Gradle project in `samples/android`. There is no Slang on the device, so `RNDR_SHADER_COMPILER` is off there
and shaders are compiled on the host at build time. The configure line, the Gradle invocation and what is and
is not checked yet are in [docs/android-plan.md](docs/android-plan.md); adb, logcat, stack traces and the rest of
running it on a phone are in [docs/android-debugging.md](docs/android-debugging.md).

The emulator always runs from F:, never C: - launch it with `ANDROID_AVD_HOME='F:\Android\avd'` as in
[docs/android-debugging.md](docs/android-debugging.md#the-emulator). It wants 12 GB free where the AVD lives
and C: does not have it. When done with it, shut it down (`adb -e emu kill`) and confirm it is gone - no
`emulator` or `qemu-system-*` process left in `Get-Process` - before moving on.

## Tests

Catch2, single `rndr-test` binary, run a subset by tag:

    ./build/msvc-debug/Debug/rndr-test.exe "[input]"

Tags in use: `[input]` `[window]` `[canvas]` `[mesh]` `[bitmap]` `[fps]` `[init]` `[forge]` `[forge-window]` `[audio]`
`[audio-device]`.

Catch2 aborts a test case by throwing and this binary has no exceptions, so `test/catch-no-exceptions.hpp`
replaces `SKIP` with one that records the same result and leaves by returning. It therefore only works in
the test case body - inside a helper or a lambda it returns from that, and no compiler will say so.
`REQUIRE` and `FAIL` keep the throw: too much of the suite asserts inside helpers that return a value, and
such a helper has nothing to return once the assertion fails. A failing assertion therefore ends the run
after reporting itself, and the cases after it do not run.

`[forge]` is headless and runs anywhere a Vulkan device exists. `[forge-window]` needs a window system
as well, opens an offscreen window and presents to it, and covers `Surface`, `SwapChain` and
`FrameContext`. Both skip rather than fail on a machine that cannot run them, so a run that found no
device looks like a run that passed - set `RNDR_TEST_REQUIRE_VULKAN=1` to make that a failure instead.

A Forge case builds its own context and device through `ForgeFixture` (`ForgeWindowFixture` in the windowed
file), skips through `IsForgeAvailable()` first, ends in a readback compared against a value worked out on
the CPU, and closes with `REQUIRE_NO_VALIDATION_ERROR` - `_AT_TEARDOWN` when it has to catch an object that
outlived the device. `ForgeTest::Unwrap` fails the case with the error code, so nothing checks `HasValue()`
by hand. Shaders are Slang source in a `constexpr const char*` beside the case, compiled through
`GetShaderCache()` so the per-section rerun does not recompile. A feature the machine may lack is probed and
skipped, never assumed. Catch2 re-runs the case body once per `SECTION`, so anything built above the
sections is built once per section.

`[audio]` is headless and deterministic: decoders and the mixer driven directly. `[audio-device]` opens the real
output endpoint and plays through it; it skips on a machine without one, and `RNDR_TEST_REQUIRE_AUDIO=1` makes that
a failure. Read [docs/audio.md](docs/audio.md) before touching `src/audio/` - the thread split and the clip-lifetime
rule are the whole design.

## Commits

`type(Scope): Sentence-case subject`, imperative, no trailing period — e.g.
`fix(Forge): Return descriptor sets to their pool`. Scopes are subsystems: `Forge`, `Canvas`, `Window`,
`CMake`. A change to the tests alone is `test(Forge): Cover the bitmap upload path`; one that fixes what the
test found is `fix(Forge): ...` in its own commit.

Keep the body terse: a few lines on what changed and why, not a prose retelling of the diff. Do not cite
task or issue numbers — they go stale the moment the list they refer to is renumbered, and the commit has to
stand on its own. No `Co-Authored-By` or `Claude-Session` trailers.
