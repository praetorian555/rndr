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
  `IsValid()` contract, and the error strategy. Forge deliberately departs from the rest of the repo here:
  failures throw rather than returning a status, and `RNDR_RETURN_ON_FAIL` is not used. Following the
  surrounding code from another subsystem will produce the wrong pattern.

- [docs/audio.md](docs/audio.md) — `src/audio/` reports failures the other way round: nothing there throws, and
  anything fallible returns `Opal::Expected<T, Rndr::ErrorCode>` with the detail in the log. It is the only
  subsystem that does, and the only user of `Rndr::ErrorCode`. Canvas and Forge throw; do not carry either
  convention across.

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

## Tests

Catch2, single `rndr-test` binary, run a subset by tag:

    ./build/msvc-debug/Debug/rndr-test.exe "[input]"

Tags in use: `[input]` `[canvas]` `[mesh]` `[bitmap]` `[fps]` `[init]` `[forge]` `[forge-window]` `[audio]`
`[audio-device]`.

`[forge]` is headless and runs anywhere a Vulkan device exists. `[forge-window]` needs a window system
as well, opens an offscreen window and presents to it, and covers `Surface`, `SwapChain` and
`FrameContext`. Both skip rather than fail on a machine that cannot run them, so a run that found no
device looks like a run that passed - set `RNDR_TEST_REQUIRE_VULKAN=1` to make that a failure instead.

`[audio]` is headless and deterministic: decoders and the mixer driven directly. `[audio-device]` opens the real
output endpoint and plays through it; it skips on a machine without one, and `RNDR_TEST_REQUIRE_AUDIO=1` makes that
a failure. Read [docs/audio.md](docs/audio.md) before touching `src/audio/` - the thread split and the clip-lifetime
rule are the whole design.

## Commits

`type(Scope): Sentence-case subject`, imperative, no trailing period — e.g.
`fix(Forge): Return descriptor sets to their pool`. Scopes are subsystems: `Forge`, `Canvas`, `Window`,
`CMake`.

Keep the body terse: a few lines on what changed and why, not a prose retelling of the diff. Do not cite
task or issue numbers — they go stale the moment the list they refer to is renumbered, and the commit has to
stand on its own. No `Co-Authored-By` or `Claude-Session` trailers.
