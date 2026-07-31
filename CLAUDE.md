# rndr

Two rendering APIs live side by side: **Canvas** (OpenGL 4.5, high level, on by default) and **Forge**
(Vulkan, low level, `RNDR_FORGE=OFF` by default). Current work is on Forge.

`Opal::` is first-party — [praetorian555/opal](https://github.com/praetorian555/opal), fetched by CPM at
configure time. It is not a third-party dependency, and it is not edited from this repo.

## Read before editing

- [docs/forge.md](docs/forge.md) — conventions that hold across all of `src/forge/`: the empty-state /
  `IsValid()` contract, and the error strategy. Forge deliberately departs from the rest of the repo here:
  failures throw rather than returning a status, and `RNDR_RETURN_ON_FAIL` is not used. Following the
  surrounding code from another subsystem will produce the wrong pattern.
- [docs/forge-tasks.md](docs/forge-tasks.md) — the Forge work queue.

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

Tags in use: `[input]` `[canvas]` `[mesh]` `[bitmap]` `[fps]` `[init]`.

## Commits

`type(Scope): Sentence-case subject`, imperative, no trailing period — e.g.
`fix(Forge): Return descriptor sets to their pool`. Scopes are subsystems: `Forge`, `Canvas`, `Window`,
`CMake`.

Keep the body terse: a few lines on what changed and why, not a prose retelling of the diff. Do not
reference the task list — `docs/forge-tasks.md` records that, and a commit that cites a task number goes
stale the moment the list is renumbered. No `Co-Authored-By` or `Claude-Session` trailers.
