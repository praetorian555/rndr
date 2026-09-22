# Forge test review

A review of `test/forge/` against the public API in `include/rndr/forge/`: what the suite covers, what it
does not, and what it covers twice. Line numbers refer to the working tree at `cffbe86` (2026-09-22) and
will drift; the case and section titles beside them will not.

---

## What is there

`test/forge/smoke-test.cpp` holds 81 headless cases over 247 sections; `test/forge/window-test.cpp` holds
7 windowed cases over 10 sections. Nearly every case ends in a readback compared against a value worked out
on the CPU, and every case asserts that the validation layer reported nothing.

Covered well:

- The lifetime contract (empty state, moves, `Destroy`) for all 16 handle types, each asked to do its job
  after a move rather than only to report itself valid.
- Buffers: update and read at an offset, host access kinds, device-only through the staging helpers.
- Compute dispatch, direct and indirect, with device-written arguments.
- Transfers: blits (scale, mirror, format conversion, linear filter), buffer to texture and back with
  sub-boxes, buffer offsets, row lengths and layer heights, mip generation over every layer, mip level
  sizes.
- Barriers: all three kinds in one batch, the presets, the tracked layout per subresource, the by-region
  flag, buffer ownership transfer between two queue families.
- Synchronization: fences and `WaitForAll`, binary and timeline semaphores host and device side, batched
  submits, every refusal of an empty or mismatched object.
- Descriptors: bindless with partially bound and variable count, immutable samplers, single-resource and
  by-name updates, pool reset and per-set free, several sets bound at once, layout checked against the
  shaders.
- Pipelines: specialization constants for graphics and compute, vertex input and push constants derived
  from reflection and checked against it, sample counts, dynamic state, a rejected pipeline leaving no
  layout behind.
- The fixed-function tables, each enumerator checked by its effect: stencil operations, comparators, blend
  factors and operations, address modes and border colours, colour write masks, cull and winding, fill
  modes, topologies, viewport and scissor, depth range and clamp, depth test, stencil per face.
- Sampling: 2D, 3D and cube, LOD clamp, separate sampler and image, storage image writes.
- Queues: async compute, dedicated transfer with the layouts it may reach, timestamps per family.
- Timestamps blocking and not, host reset, range checks.
- Debug names reaching the layer, labels and the GPU event macros.
- Shaders from SPIR-V in memory and from a file, the shader cache.
- `LoadMesh` including what assimp generates and what it refuses.
- Mesh shaders with and without the extension.
- Surface, swap chain and frame context: format support, presented frames read back, resize, a window
  with no client area, a device created before any surface.

---

## What is missing

In rough order of value.

1. **A graphics draw with a descriptor set.** No case binds a set at the graphics bind point; every
   `CmdBindDescriptorSet` in the suite sits next to a `CmdDispatch`. A fragment shader sampling a texture
   and a vertex shader reading a constant buffer is the main path of `samples/modern-vulkan` and is not
   tested.
2. **Multiple colour attachments.** Every pipeline names one. `DeviceFeatures::independent_blend`, a
   per-attachment write mask and a `color_attachment_formats` of more than one entry are untouched.
3. **Multisampling.** A `Count4` pipeline is created (3229) and nothing is drawn with it. No multisampled
   texture, no draw, no resolve - and Forge has no resolve command to test.
4. **The bitmap upload path.** `Texture::Create(device, queue, bitmap, ...)` and
   `CmdCopyBufferToTexture(buffer, bitmap, texture)` are the upload path docs/forge.md documents and the
   sample uses. Neither is called.
5. **`Shader::FromSource`** (the file overload) is never called. `ErrorCode::ShaderCompilationError` for
   source Slang refuses and `InvalidArgument` for empty source are never asserted.
6. **`CommandBuffer::Reset()` and `Begin(false)`.** Neither is called. docs/forge.md says `Reset` does not
   roll layout tracking back; nothing checks it. Submitting one command buffer twice is untested.
7. **`CmdCopyTexture` succeeding headless.** The only headless call is the one the layer rejects (671); the
   only successful one is the window test's copy into its readback texture. No content-verified copy, no
   sub-box, no layer or mip region.
8. **Texture ownership transfer** between queue families. The buffer version exists (7851).
9. **`AttachmentLoadOperation::Load` keeping contents** across two passes on one texture, and
   `AttachmentStoreOperation::DontCare`. `Load` appears once, as "accepted" (2386), never verified.
10. **Depth bias** affecting depth values, static and dynamic. `CmdSetDepthBias` is recorded (3259) and
    nothing reads the result. The positive paths of `wide_lines` and `depth_bias_clamp` are absent.
11. **`ImageLayout::DepthStencilReadOnly`** as an attachment layout. The docs allow it, no preset builds
    it, no test reaches it. A stencil attachment on a different texture from the depth one: none.
12. **Texture views over a subrange** (`TextureDesc::subresource_range`), `Texture1D`, `Texture1DArray`,
    `CubeArray`, and a `Texture2DArray` sampled in a shader rather than only copied.
13. **Descriptors:** an actual update after bind (update between `CmdBindDescriptorSet` and the submit),
    `UpdateUnusedWhilePending`, `set_index` other than zero with the shader check, the "sized for fewer
    descriptors than the shader indexes" refusal, and the `Update(name, buffer, ...)` overload (only the
    texture by-name overload is called, 3879).
14. **Push constants** at a non-zero offset, and a block declared by two stages. The merge section (3687)
    has one declarer; the fragment stage declares no push constants, so nothing is merged.
15. **Specialization** constants narrower than 32 bits (the `byte_size` path), 64-bit ones, and the
    too-wide value refusal.
16. **FrameContext error paths:** `GetCommandBuffer`, `GetColorTexture` and `EndFrame` outside a frame,
    `BeginFrame` twice (frame-context.cpp:163 logs it), `frames_in_flight = 0`. Two swap chains on one
    device, which docs/forge.md promises, is untested.
17. **Context desc:** a `required_instance_extensions` entry that is missing, the
    `max_stored_debug_messages` cap, `collect_debug_messages = false`, `GetDebugMessageCount` with a bad
    severity.
18. **`VkResultToErrorCode`.** A pure function needing no device, with no test. The cheapest addition here.
19. **Buffer edges:** `keep_memory_mapped = false`, `Read` out of bounds, the `offset` arguments of
    `UploadToBuffer` and `ReadBackBuffer`, a `CmdCopyBuffer` region past the end being refused, the default
    overload with unequal sizes.
20. **`PhysicalDevice::GetQueueFamilyIndex(flags, not_flags)`**, the `QueueFamilyIndices` helpers,
    `Device::Create` with an empty physical device.
21. **Task shaders** on the positive path; geometry and tessellation stage reflection. Both features exist
    in `DeviceFeatures` and nothing drives them.
22. **`SetDebugName`:** 15 overloads, 2 called (Texture and TimestampQueryPool). A wrong `VkObjectType` in
    any of the other 13 is silent. One loop naming every type and asserting no validation error covers all.
23. **Swap chain:** a `PresentMode` other than Fifo, `depth_pixel_format`, `AcquireTexture` twice without
    a Present.

---

## What is redundant

**Done in bucket A** (2026-09-22): everything in this section was removed or folded into a shared helper.
It stays as the record of what went and why.

Sections that assert something another case already asserts:

- **"Forge buffer survives a move"** (337) is inside the lifetime Buffer contract (4267), which also reads
  through the mapped pointer. Delete.
- **"A moved set carries what its layout declared"** (2265) duplicates the lifetime DescriptorSet check
  (4421). Delete.
- **"A move carries the layouts across"** (1783) duplicates the lifetime Texture check (4285). Delete.
- **"Forge blending"** (6773), sections 1 to 4: SrcAlpha over InvSrcAlpha, One and One, Zero and One,
  ReverseSubtract are each enumerated by the blend table (8589). Sections 5 and 6 - the alpha factors and
  the alpha operation read from their own fields - are the only unique ones, since the table sets the
  alpha state equal to the colour state. Keep those two.
- **"Wrapping and clamping differ at a coordinate outside the texture"** (7206): Repeat and Clamp at
  u = 1.25 are both in the address-mode table's coordinates (9047). Delete.
- **"Forge barrier presets"** sections 1 and 2 (ToShaderRead, ToTransferSource): both presets are called
  dozens of times elsewhere ahead of a readback. Keep `To`, the no-preset refusal, the depth aspect and
  ReadThenWrite.
- **"Forge stencil testing"** (6439) sections 1 and 2: Equal and Always at reference == stored are in the
  comparator table (8291). What is unique is the static pipeline reference and write mask against the
  dynamic ones the later cases use. Shrink to one section plus the "names no texture" refusal.
- **The bindless pair:** "variable count above the descriptor count" (1272, 1465) and "update after bind
  layout needs a pool that expects one" (1291, 1469) are the same check twice. Delete the second pair.
- **"Forge context and device"** (247) and **"Forge compute dispatch and readback"** (369) are pure
  smokes, subsumed by the lifetime cases and by everything else. Keep one as the canonical first case.
- **"Forge GPU event macros"** (1872): the macros are thin wrappers over labels tested at 1796, yet seven
  sections re-prove nesting and closing a region a refused command left open. "A name longer than a small
  string" asserts nothing about allocation. Cut to: scoped, begin and end, both arities compile.
- **"An ownership transfer naming one family on both sides is recorded"** (1678) is a no-op transfer; the
  real one is at 7851. Marginal.

Duplicated code, which costs the same as duplicated tests when something changes:

- The validation-report loop is hand-rolled seven times for cases that build their own context (1109,
  1300, 1479, 1599, 4246, 5334, window-test 817). One `REQUIRE_NO_VALIDATION_ERROR_IN(context)` removes
  them.
- Six probe helpers each build a context: `IsSoftwareDevice`, `IsIndexTypeUint8Supported`,
  `GetFirstPhysicalDeviceFeatures`, `IsMirrorClampToEdgeAvailable`, `IsMeshShaderAvailable`,
  `AreQueuesAvailable`. One `ProbeDevice(features)` covers all of them.
- The push-constant-address compute pipeline desc is built by hand about eight times though
  `MakeAddressPipeline` exists (4119). The wiped output buffer is built four times (3120, 4131, 5386,
  7618). The split-copy setup is copied verbatim between "batched submit" (685) and "timeline semaphores"
  (756).

---

## Stale wording and hygiene

The first two bullets and the doc drift were fixed in bucket A; the driver workaround and the cost stand.

- **"throws".** 53 section titles and about 63 comments in the tests say a call throws, as do four header
  comments: descriptor-set.hpp:293 and :298, swap-chain.hpp:173, synchronization.hpp:295. Nothing has
  thrown since `c163506`. "Is refused" or "reports" is what they mean.
- **A second `ForgeFixture` inside a section** at 2958, 5742, 6199 and 9120 leaves two contexts live and
  volk's table pointed at the newer one. docs/forge.md says that works and is "not something to rely on".
  "A mesh shader draw without the extension" (10061) does the same thing as a case of its own; move the
  four to match.
- **The blend table's driver workaround** (`constants_arrive_rotated`, 8697) skips 4 of 14 factors on an
  llvmpipe that rotates the constants, with only a `WARN`. CI loses those four silently.
- **Cost.** Catch2 re-runs a case body per section, and every case builds a context and a device at the
  top, so a full run creates a device about 257 times. Acceptable today; a context shared per binary with
  a device per case is the lever if the suite starts to feel slow.
- **Doc drift.** docs/forge.md line 200 says `EndFrame` takes the layout the texture was left in. The
  header has `EndFrame()` with no argument; the transition reads the tracked layout.

---

## Doing the work

The items above split into three buckets by how much Vulkan judgment each one needs, and each bucket has a
model and a reasoning effort that fit it. Do them in order: the cleanup first, because it shrinks the file
and sharpens the patterns the later work copies; the hard bucket last, because it changes Forge and wants
the noise gone before it does.

Whichever model does a bucket, hand it: the test section of CLAUDE.md (no exceptions, `REQUIRE` ends the
run, `SKIP` only in the case body), `RNDR_TEST_REQUIRE_VULKAN=1` so a skip is a failure, the configure line
with `RNDR_FORGE=ON`, `RNDR_ASSIMP=ON` and `RNDR_FORGE_VALIDATION=ON`, and this file as the checklist. One
commit per item; `test(Forge): ...` for the test, `fix(Forge): ...` in its own commit for whatever the test
found.

### Bucket A: mechanical cleanup

**Sonnet 5, low effort.** Every change here copies a pattern already in the file and needs no Vulkan
judgment. One session, one or two commits. The risk is breaking a shared setup when deleting a section, so
run the full `[forge]` and `[forge-window]` set after.

**Done** on 2026-09-22 in five commits, one per bullet below, each built and run against the full Forge set
on the way. The suite went from 87 cases and 16112 assertions to 89 cases and 15075: two smoke cases and 17
sections gone, four sections promoted to cases, and about 400 lines of setup folded into `CanCreateDevice`,
`REQUIRE_NO_VALIDATION_ERROR_IN`, `MakeAddressPipeline`, `MakeWipedOutput`, `RequireComputeWrote` and
`SplitCopy` at the top of `smoke-test.cpp`.

- Rename "throws" in the 53 section titles, the ~63 comments and the 4 header comments.
- Delete the sections and cases listed under "What is redundant".
- Collapse the 7 hand-rolled validation-report loops into one macro, the 6 probe helpers into one
  `ProbeDevice(features)`, and the duplicated address-pipeline, wiped-buffer and split-copy setups into the
  helpers that already exist.
- Move the 4 nested `ForgeFixture` sections into cases of their own.
- Fix the `EndFrame` sentence in docs/forge.md.

### Bucket B: pattern-following tests

**Sonnet 5, medium effort.** Each of these copies the shape of an existing case and asserts an error code
or a readback; the Forge side may turn out to lack a guard, which is a small fix. Two sessions.

Items 5, 6, 16, 17, 18, 19, 20, 22, 23, and the `Update(name, buffer, ...)` half of 13: `Shader::FromSource`
and the compile-error codes, `CommandBuffer::Reset` and `Begin(false)`, the FrameContext and context desc
error paths, `VkResultToErrorCode`, the buffer edges, the physical device helpers, the `SetDebugName` loop,
the swap chain modes.

**In progress**, one commit per item: 18 (`VkResultToErrorCode`), 20 (the physical device queue family
helpers), and 5 (`Shader::FromSource`) are done. 6, 13, 16, 17, 19, 22, 23 remain.

### Bucket C: new shaders, Vulkan semantics, likely Forge changes

**Opus 5, high effort; Fable for items 3 and 21.** Each needs a Slang shader written for it, a layout and
barrier sequence reasoned through, and a readback designed to isolate one thing. Several will find real
bugs. Item 3 needs a resolve command that does not exist, which is API design rather than test writing;
item 21 fights Slang and extension support on llvmpipe. One item per session for 1, 2, 3, 12 and 13; the
rest pair up.

Items 1, 2, 3, 4, 7, 8, 9, 10, 11, 12, 14, 15, 21, and the update-after-bind and `set_index` halves of 13:
the graphics draw with a descriptor set, multiple colour attachments, multisampling, the bitmap upload
path, `CmdCopyTexture` headless, texture ownership transfer, `Load` keeping contents, depth bias, the
read-only depth layout, texture views and shapes, push constants at an offset and across two stages, narrow
and wide specialization constants, task shaders and the other stages.
