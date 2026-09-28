# Forge device features plan

## Context

`DeviceFeatures` (`include/rndr/forge/device.hpp:53`) is 39 bools, every one off by default but
`sampler_anisotropy` and `buffer_device_address`. Vulkan only lets a feature be enabled at `vkCreateDevice`, so
a caller has to predict at creation everything any later object will ask of the device: wireframe, wide lines,
a cube array view, a BC texture, a mirror-once sampler, an 8-bit index buffer, a host query reset, an indirect
count draw, a 64-bit atomic, multiview, a workgroup size from a specialization constant. 24 guard sites across
eight files (`pipeline.cpp`, `texture.cpp`, `command-buffer.cpp`, `descriptor-set.cpp`, `shader.cpp`,
`query.cpp`, `buffer.cpp`, `synchronization2.cpp`) read `Device::GetEnabledFeatures()` and refuse with
`ErrorCode::InvalidArgument` naming the field, and 20 doc comments across the headers say "needs
`DeviceFeatures::x`". Each guard is right on its own. Together they make the struct a checklist the caller
reconstructs by reading every comment or by failing at runtime, and `samples/forge-practice/06-split-screen.cpp:12`
says so in as many words: "Wireframe and wide lines are device features; request them in DeviceFeatures or the
pipeline is refused".

The struct mixes two kinds of field. A few are real choices the caller owns, because a device may not have
them and a renderer is designed around whether it does: mesh and task shaders, tessellation, geometry,
`shader_float64`, the 64-bit atomics, local read, `sampler_filter_minmax`. The rest have no reason to be off on
a device that has them. Enabling a supported feature costs nothing at runtime - the only Vulkan features with a
documented performance cost are the robustness ones, and Forge exposes none of them - and the one thing
leaving it off buys, a creation that succeeds on a device lacking it, is exactly what a device that *has* it
does not need.

`src/forge/device.cpp` already knows the better rule. On the 1.1 build it turns on
`shaderStorageImageReadWithoutFormat` and `WriteWithoutFormat`, takes `VK_KHR_format_feature_flags2`,
`VK_KHR_timeline_semaphore`, `VK_KHR_synchronization2` and `VK_KHR_dynamic_rendering` "when the device has it,
not the caller's to ask for". That rule never spread to the other 39 fields. This plan spreads it.

Written against master `cc565a8` on 2026-09-28.

## Decisions

- **Requirements stay in `DeviceFeatures`; the rest floats.** A field set true is a requirement: the device
  has to have it, `SelectPhysicalDevice` picks a device that does, and creation fails naming it otherwise -
  unchanged. A new `DeviceDesc::enable_supported_features`, on by default, additionally turns on every field
  the physical device supports. So `DeviceDesc{}` gets everything the machine can do, `{.mesh_shader = true}`
  gets everything and insists on mesh shaders, and a test of a refusal turns the flag off and gets exactly
  what it named.
- **A bool and a flag, not a tri-state per field.** A per-field `Auto | Required | Off` is the fuller design,
  and it is not needed yet: no caller wants one feature forced off while the rest float, and it would cost a
  second struct (the tri-state ask beside the bool answer the guards read), every `constexpr DeviceFeatures
  k_features{.x = true}` in the suite, `BindlessFeatures()`, `Fill`, and the `require` lambda in
  `FindUnsupportedFeature`. The flag reaches the same place with the 24 guards and every existing test
  untouched. Tri-state is the follow-up if a caller ever needs it.
- **`Device::GetEnabledFeatures()` reports what is on; `GetDesc()` keeps the ask.** The guards already read
  `GetEnabledFeatures()`, so none of them moves; they start answering "what does this device have" instead of "what
  was asked", which is the question they were asking all along. A new `m_features` member holds the merged set
  so `m_desc` stays what the caller passed.
- **Requirements are checked before the merge.** `ReportUnsupportedFeatures` runs on the ask, so a missing
  requirement is reported by its name. Run after the merge it would still pass - the merged set is a subset of
  what the device supports - and the message would name nothing.
- **The two true defaults stay.** With the flag on they are redundant, and with it off they are requirements
  that fail creation on a device lacking them, the same as today. Flipping them to false would make every
  `ForgeFixture` and `HalvesFixture` built with explicit features - there are 15 - lose device addresses, which
  13 buffers in the suite ask for, and anisotropy, which 9 samplers do. Flipping them is what a tri-state
  would do naturally, so it waits for that.
- **Test fixtures turn the flag off.** `MakeHeadlessDeviceDesc` and `ForgeFixture` build a device with exactly
  the features named, so the twelve "on a device without the feature is refused" cases and the sections
  that lean on the default fixture having a feature off (independent blend at `smoke-test.cpp:4337`, the
  static depth bias clamp at `10763`, the plain descriptor pool at `9004`) stay valid without an edit. The
  windowed fixture (`window-test.cpp:94`) and the two devices built by hand there keep the default: nothing in
  `[forge-window]` tests a refusal, and it puts the auto path under presentation on every run.
- **The 20 per-desc cross references stay.** "Needs `DeviceFeatures::wide_lines`" is still true, it names the
  field the log names, and the struct's own comment says once that a field is on wherever the device has it
  unless the flag is off. Rewording twenty comments to say the same thing is churn.
- **Extension-backed features come with their extension, as now.** `CollectDeviceExtensions` reads the merged
  set, so an auto-enabled `mesh_shader` pulls in `VK_EXT_mesh_shader` and on 1.1 an auto-enabled
  `draw_indirect_count` pulls in `VK_KHR_draw_indirect_count`. A feature only reads as supported when its
  structure was chained, and `FeatureChain` only chains a structure whose extension the device reports, so the
  extension check that follows passes by construction for everything the merge added and still catches a name
  in `desc.extensions` the device lacks.
- **Enabling everything is safe, and the flag is the escape hatch.** None of the 39 changes the meaning of an
  operation that does not use it: descriptor indexing only touches bindings that carry a flag, local read adds
  two commands with defaults equal to the old behaviour, maintenance4 relaxes a few layer rules and adds
  nothing. A driver that reports a feature it then misbehaves with is a driver bug; a sample meeting one turns
  the flag off and names what it needs, which is today's behaviour exactly.

## Status

| Phase | State |
|---|---|
| 1 - the flag and the merge | **Done** in `bdb0338` |
| 2 - the test | **Done** in `227f189` |
| 3 - docs and samples | **Done** |
| 4 - verify on both builds and on Android | **Done** but for the Galaxy A56, which was not attached |

Verified on 2026-09-28 on an RTX 5060 Ti with the validation layer: `[forge]` 151 cases passing in
`build/msvc-debug`, 150 and one skip in `build/msvc-vk11` (the vertex-stage layer case, which that build never
supports); `[forge-window]` 11 and one skip in both (a surface colour space this monitor does not offer). The
windowed fixture keeps the flag on, so it is the auto path under presentation.

Phase 4, the same day:

- `modern-vulkan` on the RTX 5060 Ti with the layer loaded, its desc asking for nothing but a surface, so every
  feature the driver has is turned on: ten seconds of frames, a clean exit, no validation message.
- On CI's lavapipe (Mesa 24.3.2): the new case passes in both builds; `[forge]` is 144 passing and 7 skipped on
  1.3, 143 and 8 on 1.1. Every skip is a missing queue family or the known lavapipe non-uniform indexing bug, plus
  the vertex-stage layer case on 1.1; none is about a feature.
- The emulator `rndr-api36` (gfxstream over the same GPU): the debug APK with the layer loaded ran at 60 Hz with no
  validation message and nothing in the crash buffer. The emulator was killed afterwards and confirmed gone.
- Not run: the Galaxy A56, which was not attached. Xclipse is the driver most likely to report a feature it
  mishandles, so a run there is still owed before release.

Phase 3 changed less than planned in the samples: only `06` and `11` said something now wrong ("request it or
the pipeline is refused", "enable independent_blend"). The others name a field or say to skip where the device
lacks it, which is still true.

## Phase 1 - the flag and the merge

One `feat(Forge)` commit. It changes what the fixture builds, so the fixture edit rides with it to keep the
suite green.

1. `include/rndr/forge/device.hpp`: `DeviceDesc::enable_presentation` gains a sibling
   `bool enable_supported_features = true;` with a comment saying what it does and when to turn it off, and
   the `OPAL_CLONE_FIELDS` list at line 247 gains the field. `Device` gains `DeviceFeatures m_features;` and
   `GetEnabledFeatures()` (line 412) returns it, with its comment changed from "the features this device was created
   with" to "the features that are on: what the desc asked for, plus what `enable_supported_features` found".
2. `src/forge/device.cpp`: `FeatureChain::Read(Forge::DeviceFeatures&) const` beside `Fill` (line 233), the
   inverse map. It reads `vk11`, `vk12`, `vk13` and the three extension structures, which on 1.1 `Query`
   already fills from the extension structures, so it is written once for both builds. Two rules that make it
   not a mirror image of `Fill`: a Forge field that fans out to several Vulkan bits
   (`update_after_bind_descriptors` to four, `non_uniform_descriptor_indexing` to four) is on only when every
   bit is, since `Fill` sets all of them and `vkCreateDevice` fails on any one the device lacks; and it sets
   rather than ORs, which is the same thing once the requirements have been checked. `shader_output_layer`
   stays off on 1.1 because `Query` says `VK_FALSE` there, for the reason its comment gives.
3. `Device::Create` (line 750) in this order: the version check and `CollectQueueFamilies` as now;
   `m_features = desc.features`; `ReportUnsupportedFeatures(m_physical_device, m_features)`; then, when
   `desc.enable_supported_features`, a `FeatureChain supported` over `IsExtensionSupported`, `Query`, and
   `Read(m_features)`; then `CollectDeviceExtensions(m_physical_device, desc, m_features)` and the extension
   check as now; `enabled_features.Fill(m_features)` at line 814. `CollectDeviceExtensions` (line 371) takes
   the features as a third argument instead of reading `desc.features`, and `FindUnmetRequirement` (line 613)
   passes `desc.features`, so device selection keeps judging requirements only.
4. The move constructor (line 1075) and move assignment (line 1118) carry `m_features`.
5. `test/forge/smoke-test.cpp`: `MakeHeadlessDeviceDesc` (line 79) and the desc in `ForgeFixture` (line 119)
   add `.enable_supported_features = false`. `HalvesFixture` (line 4049) goes through `ForgeFixture` and needs
   nothing. `window-test.cpp` is left alone.
6. `[forge]` and `[forge-window]` green in `build/msvc-debug` and `build/msvc-vk11`, both with
   `RNDR_TEST_REQUIRE_VULKAN=1`. The "Forge device features" case at `smoke-test.cpp:2396` becomes the witness
   that the flag off leaves exactly the ask - its first section asserts the defaults through
   `MakeHeadlessDeviceDesc` - and gets a comment saying so.

Commit: `feat(Forge): Turn on every feature the device supports unless told not to`.

## Phase 2 - the test

One `test(Forge)` commit: a case beside "Forge device features", built with `MakeHeadlessDeviceDesc` and the
flag turned back on, in the shape the file uses - skip through `IsForgeAvailable()`, `ForgeTest::Unwrap`, a
`REQUIRE_NO_VALIDATION_ERROR` at the end.

- *What is on matches what the device reports.* For `fill_mode_non_solid`, `wide_lines`, `geometry_shader`,
  `shader_int64` and `image_cube_array`, `GetEnabledFeatures().x == (GetPhysicalDevice().GetFeatures().vkX == VK_TRUE)`.
  `GetDesc().features.fill_mode_non_solid` is still false, which is the ask staying the ask. If
  `GetEnabledFeatures().mesh_shader` then `IsExtensionEnabled(VK_EXT_MESH_SHADER_EXTENSION_NAME)`, which is the
  extension coming with the feature.
- *The bit reached `vkCreateDevice`.* Skipped unless the physical device reports `wideLines`. A command buffer
  on the auto device records `CmdSetLineWidth(2.0f)` between `Begin` and `End`. Forge's guard passing shows
  the field is on in `m_features`; the layer staying silent shows the feature is on in the device, since
  `vkCmdSetLineWidth` with a width other than one has a VUID against a device without it. No pipeline, no
  shader, no submit.
- *On 1.1, a promoted extension the device has is enabled.* Under `#if defined(RNDR_FORGE_VULKAN_1_1)`: if
  `IsExtensionSupported(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME)` then `IsExtensionEnabled` of the same. That
  extension carries its feature without a structure, so supported means the feature reads true, means the
  merge turns it on, means the extension is collected.

Commit: `test(Forge): Cover a device taking every feature it supports`.

## Phase 3 - docs and samples

One `docs(Forge)` commit, or a `docs` and a `samples` one if the stub edits grow.

- `include/rndr/forge/device.hpp`: the `DeviceFeatures` comment (line 53) says a field is a requirement, that
  everything else is on wherever the device has it unless `DeviceDesc::enable_supported_features` is off, and
  that `Device::GetEnabledFeatures()` is what to ask afterwards. `BindlessFeatures()` (line 214) is "the requirements
  a bindless renderer hands `SelectPhysicalDevice`". The per-field comments stay.
- `docs/forge.md`, the `DeviceFeatures` paragraph after "Vulkan is still visible in two deliberate places":
  the same three sentences, and the two defaults explained as requirements that stay for the fixture's sake.
- `samples/forge-practice`: the headers of `02`, `06`, `11`, `15`, `s1`, `s2` and `s3` say "request it in
  DeviceFeatures" or "skips where the device lacks it". They become "on wherever the device has it; check
  `device.GetEnabledFeatures().x` after creation and skip if it is off", which also retires the pattern of probing
  with a throwaway device that `CanCreateDevice` uses in the suite - a sample has one device and asks it.
- `docs/forge-api-gaps.md` and `CLAUDE.md` need nothing.

Commit: `docs(Forge): Say which features a device turns on by itself`.

## Phase 4 - verify on both builds and on Android

Nothing to commit; what the flag changes is what a real driver gets asked to enable, and only a run shows it.

- `[forge]` and `[forge-window]` in `build/msvc-debug` and `build/msvc-vk11` with `RNDR_TEST_REQUIRE_VULKAN=1`.
- `modern-vulkan` on the desktop with validation on: its `DeviceDesc{.surface = surface}` now gets everything
  the driver has, which is the first time a real driver is asked for every feature at once.
- The same on lavapipe through the recipe in `ci-lavapipe-local-repro`, since CI will run it there.
- Android: `build/android-debug` and the APK per `docs/android-plan.md`, `modern-vulkan` on the Galaxy A56
  (Xclipse) and on the `rndr-api36` emulator (gfxstream, launched from F: as `docs/android-debugging.md`
  says, killed and confirmed gone afterwards), and `[forge]` on the device. This is where a driver that
  reports a feature it cannot really enable would show, and either is a driver that has surprised before.
  The remedy, if one does, is the flag off in that sample with the features it needs named, not a change to
  Forge.

## Risks

- **A driver misbehaves with a feature merely enabled.** Not seen on any of the three desktop drivers or the
  two Android ones so far, because none of them has been asked. Phase 4 asks. The flag is the way round it.
- **`vkCreateDevice` fails with everything on where it passed with two.** Would mean the device reported a
  feature it will not enable, which the layer names; a driver bug, same remedy.
- **A refusal hides behind an auto-enabled feature.** Cannot, in the suite: every headless fixture runs with
  the flag off. A sample that expected a refusal and got a wireframe instead is the point.
- **maintenance4 and local read relax what the layer accepts.** Both are on wherever the device has them once
  the flag is. Forge's own checks (push constant ranges, input attachment bindings) do not lean on the layer
  for what they refuse, and the refusal cases run flag-off regardless.

## Out of scope

- A tri-state per field. When a caller needs one feature off with the rest floating.
- Flipping `sampler_anisotropy` and `buffer_device_address` to false. Waits for the tri-state, for the reason
  under Decisions.
- Logging the auto-enabled set at creation. `GetEnabledFeatures()` answers, and the names live in
  `FindUnsupportedFeature` only as strings beside each bit.
- `VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT` at `device.cpp:887`, set whether or not the feature is on.
  Harmless (VMA only adds the allocate flag to buffers with the usage) and untouched.
- Limits and properties (`maxDescriptorSetUpdateAfterBind*`, the resolve modes). Not features, not here.
