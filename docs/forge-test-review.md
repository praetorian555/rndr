# Forge test review

The third review of `test/forge/` against the public API in `include/rndr/forge/`: what the suite covers,
what it does not, and what it spells twice. It replaces the second review, whose every item was done or
struck with a reason. Line numbers refer to the working tree at `379856b` (2026-09-23) and will drift; the
case and section titles beside them will not.

Read together with `docs/forge.md`, which is what the claims below are checked against: a promise that
document or a header makes and no test asserts is listed here as a gap whether or not the code keeps it.

---

## What is there

`test/forge/smoke-test.cpp` holds 127 headless cases over 345 sections and 20131 assertions;
`test/forge/window-test.cpp` holds 11 windowed cases over 16 sections and 943 assertions. On an RTX 5060 Ti the
headless set runs in 118 seconds and the windowed one in 9. Nearly every case ends in a readback compared
against a value worked out on the CPU, and every case asserts that the validation layer reported nothing.

Covered well, and not worth another pass:

- The lifetime contract (empty state, moves, `Destroy`) for all 16 headless handle types, each asked to do
  its job after a move; `Surface` and `SwapChain` moves in the windowed file.
- Buffers: update and read at an offset, every host access kind, unmapped memory, device-only through the
  staging helpers, the edges of every range.
- Compute dispatch, direct and indirect with device-written arguments; the async compute and dedicated
  transfer queues; ownership transfer of a buffer and of a texture between two families.
- Transfers: blits that scale, mirror, convert, filter and read part of the source; buffer to texture and
  back with sub-boxes, offsets, row lengths and layer heights; texture to texture by level and by layer; mip
  generation over every layer; the bitmap upload path; mip level sizes; a multisampled colour draw resolved
  at every sample count the device has; the depth and the stencil aspect of a combined format read back.
- Barriers: all three kinds in one batch, the short form of every preset, the tracked layout per
  subresource, `Reset` not rolling it back, the by-region flag, the read-only depth layout written by hand.
- Synchronization: fences and `WaitForAll`, binary and timeline semaphores host and device side, batched
  submits, a command buffer submitted twice, every refusal of an empty or mismatched object.
- Descriptors: all six kinds reflected and checked against the shader, each refused as every other kind, a
  layout sized under what the shader indexes; bindless with partially bound and variable count, update after
  bind and update while pending, immutable samplers, single-resource, array and by-name updates, a set index
  other than zero, pool reset and per-set free, several sets bound at once, a set bound for a graphics draw.
- Pipelines: specialization constants of every `SpecializationType`, vertex input and push constants derived
  from reflection and checked against it, push constants at an offset and across two stages, several colour
  attachments with and without `independent_blend`, sample counts, dynamic state, a rejected pipeline
  leaving no layout behind.
- Every stage: vertex and fragment everywhere, compute everywhere, mesh, task in front of mesh, geometry,
  both tessellation stages, and every gated stage refused on a device that did not enable it.
- The fixed-function tables, each enumerator checked by its effect, with a section proving no two
  enumerators answer alike: all six stencil operation slots, comparators, blend factors and operations,
  address modes and border colours, colour write masks, cull and winding, fill modes, every topology,
  viewport and scissor, depth range, clamp, constant and slope bias and bias clamp, depth test, load and
  store operations, stencil per face static and dynamic.
- Sampling: 2D, 3D, 1D, every array shape, cube and cube array, a view over one mip level and over one
  layer, LOD clamp, separate sampler and image, storage image writes, a BC1 block uploaded and decoded.
- Device features: every field that maps onto a Vulkan bit is asked for by some case, and where the layer
  checks the bit, the field is turned off and the layer objects. A transient attachment and where it lands.
- Timestamps blocking and not, host reset, the range checks, timing on another family's valid bits.
- Debug names on every type, labels and the GPU event macros, the message log and its cap.
- Shaders from source in memory and on disk, from SPIR-V in memory and on disk, the shader cache in every
  way it can miss. `LoadMesh` including what assimp generates and what it refuses.
- Surface, swap chain and frame context: format support, present modes, colour spaces, the depth format,
  presented frames read back, the acquired texture reset to `Undefined`, the frame loop past the end of its
  timeline, resize, a window with no client area, a device created before any surface, `CanPresentTo` both
  ways, every frame context error path.

---

## What is missing

In rough order of value. The first two are families of guards that stand between a caller and undefined
behaviour in the driver, and not one of them is asserted.

1. **The transfer region guards.** `ResolveTextureRegion`, `ResolveBlitBox`, `ToVkBufferImageCopy` and the
   two usage checks in command-buffer.cpp (277-485) refuse a copy or blit region that names a mip level the
   texture does not have (358), array layers past the end (364), a negative offset or extent (392), a box that
   starts outside the level (400) or reaches past it (410, for a copy - only the blit source at 6336 is
   tested), an empty blit box (447), a buffer offset that is not a multiple of four (485), a buffer too small
   for the rows and layers the region describes (290 through 497), and each of `CmdCopyBufferToTexture`,
   `CmdCopyTextureToBuffer`, `CmdCopyTexture`, `CmdBlitTexture` and `CmdResolveTexture` handed a texture or a
   buffer created without the usage it needs (265, 277). Every one is record-time and fits the
   status-out-of-the-recorder shape at 720 and 799. One case, one section per guard, asserting the code the
   guard returns - `InvalidArgument` and `OutOfBounds` are used deliberately and a swap between them is a
   documented behaviour lost.
2. **The indirect argument guards.** `ValidateIndirectRange` (1263) refuses a buffer without
   `IndirectBuffer` usage, an offset that is not a multiple of four, a stride under one command or misaligned,
   and commands that run past the buffer; only the multi-draw feature guard at 1305 is asserted (1707).
   `HalvesFixture` has everything a section per guard needs.
3. **Refusals asserted by shape rather than by code.** In the headless file 127 refusals are
   `REQUIRE_FALSE(...HasValue())` or `!= ErrorCode::Success` and 20 assert the code; the windowed file is
   10 to 12. Where the header names the code, assert it - a wrong code is
   a caller switching on the wrong branch. Reading them found one disagreement already: `Buffer::Create` with
   initial data larger than the buffer is `InvalidArgument` in buffer.hpp:43 and `OutOfBounds` in
   buffer.cpp:70, and `docs/forge.md` sides with the code; the section at 744 would have said so. The ones
   the headers document: `Semaphore::Signal` (1513), `Fence::WaitForAll` (1632, 1639), `LoadMesh`'s
   `FileNotFound` and `UnsupportedFormat` (13663, 13696, 13704), `FromSpirv*`'s `CorruptData` and
   `InvalidArgument` (13234-13279), `GetMipLevelSize` (6774, 6779), `DescriptorSet::Create` (1905-1928),
   `ReadBackTexture` (902), the sampler and texture feature refusals. `UnsupportedFormat` and `CorruptData`
   are asserted nowhere in either file.
4. **What reflection refuses.** shader.cpp names three things Forge does not model and answers
   `UnsupportedFormat` for: a descriptor kind (256 - a `Buffer<float4>` texel buffer), a shader stage (368 - a
   `[shader("raygeneration")]` entry point) and a specialization constant type (414 - a `half`). All three
   are decided before `vkCreateShaderModule`, so none needs a device feature, and none has a test. One shader
   each, and the code asserted.
5. **The flag enums that claim to mirror Vulkan.** `docs/forge.md` and types.hpp say the flag enums are a
   cast, and every translation in command-buffer.cpp and texture.cpp is one; nothing checks. A
   `static_assert` per enumerator against its `VK_*` value costs nothing and needs no device:
   `BufferUsageBits`, `TextureUsageBits`, `ColorWriteMaskBits`, `StencilFaceBits`, `ImageLayout`,
   `ImageAspectBits`, `PipelineStageBits` (27), `PipelineStageAccessBits` (21), `DependencyFlagBits`,
   `DescriptorBindingFlagBits`, and the four `k_*` sentinels. `PixelFormat` belongs with them:
   `ToVkFormat` and `FromVkFormat` are casts (pixel-format.cpp:4-12) over 107 enumerators, `FromVkFormat` is
   what reflection reports a vertex format through (shader.cpp:197), and no test in the repository touches
   either. Checked by hand today, every one mirrors; the value is the next enumerator added.
6. **Barrier ranges.** A `TextureBarrier` whose `subresource_range` names mips or layers the texture lacks
   is refused by `ResolveRange` (texture.cpp:453), which both `SetCurrentLayout` and
   `GetCurrentLayout(range)` go through, and asserted nowhere. A `BufferBarrier` whose
   `offset` and `size` run past the buffer is not checked at all - `ToVkBufferBarrier` (command-buffer.cpp:91)
   copies both through, and the layer is what objects. A check beside the transfer ones, and a test for each.
7. **The long forms of the barrier presets.** Seven presets have an `old_layout` form and only
   `ToTransferDestination`'s is ever called (2360). `ToColorAttachment`, `ToDepthStencilAttachment`,
   `ToShaderRead`, `ToTransferSource`, `ToGeneral` and `ToPresent` given the layout are six separate
   functions in synchronization.cpp, unreached. `run_preset` at 6931 takes a lambda and already reads the
   tracked layout, so each is a section handing that layout in explicitly. `ToGeneral` with a non-default
   accessor and `Texture::GetCurrentLayout(range)` (texture.hpp:117) have no direct caller either.
8. **Feature and shape refusals with no assertion.** `PartiallyBound` without the feature
   (descriptor-set.cpp:461 - 7182 covers the other two flags); a `CubeArray` view without `image_cube_array`
   (texture.cpp:297 - 10693 skips instead); a mesh pipeline handed a geometry or tessellation stage
   (pipeline.cpp:694); `CmdGenerateMips` on a one-level texture (823); a rendering attachment naming a
   transfer-only texture, which has no view (933); `Device::GetQueue` for a family the device was not created
   with (device.cpp:822 - positive only, window 941); `SwapChain::Create` on a device with no present queue
   (swap-chain.cpp:487 - the case at window 1025 has the device and the surface); a present mode, and a
   format and colour space pair, the surface does not offer (436, 421), skipping when it offers every one.
9. **The blit format refusals.** `SupportsBlit` and `SupportsLinearFilter` are only ever used to skip. The
   three `FeatureNotSupported` answers at command-buffer.cpp:774-785 - cannot blit from, cannot blit into,
   cannot filter linearly - want a format the device reports it cannot, probed over a short list (BC1,
   `D32_SFLOAT`, `D24_UNORM_S8_UINT`) and skipped when every one is supported. `CmdGenerateMips` shares them.
10. **Desc-level refusals, no device needed.** `DescriptorPoolDesc::Add` twice for one type (85),
    `AddBinding` twice at one index (102), immutable samplers on a storage buffer binding (110), fewer
    samplers than descriptors (115), an empty sampler reference (131), `VertexInputDesc::AddAttribute` for a
    binding never added (pipeline.cpp:275) - six pure functions that belong beside the ones at 447. With a
    device: `DescriptorPool::Create` from a desc naming no descriptor (169), `PhysicalDevice::Create` of a
    null handle (physical-device.cpp:12), `BufferDesc::host_access` outside the enum (buffer.cpp:52), the way
    600 hands `GetDebugMessageCount` a severity of 99.
11. **Arguments still at their defaults.** `CmdDispatch` and `CmdDrawMeshTasks` with y or z above one
    (always 1; the two exchanged is invisible, and the indirect dispatch at 3887 reads 4, 1, 1);
    `CmdBindVertexBuffer` and `CmdBindIndexBuffer` with a non-zero offset (always 0 - the right half of
    `HalvesFixture` sits six vertices in, and the padding indices six indices in, so an offset reaches either
    without `first_vertex` or `first_index`); `CmdDrawIndexedIndirect` with more than one command or a stride
    (only `draw_count` 1 at 3820, where its unindexed twin has both).
12. **Sampler state half driven.** `mip_map_filter = Linear` never reaches `ToVkSamplerMipmapMode` (both
    samplers at 10236 are `Nearest`): a `SampleLevel` at 0.5 between a red level and a blue one is half of
    each under `Linear` and one of them whole under `Nearest`. `DepthStencilDesc::front_compare_mask` and
    `back_compare_mask` are never set - the compare mask is only ever the dynamic one (9767), where the write
    mask has both a static (9523) and a dynamic path.
13. **Windowed lifetimes and a second window.** `FrameContext` has no lifetime section where `Surface` (352)
    and `SwapChain` (428) do: move-construct, move-assign, run a frame through the moved one, `Destroy` twice.
    `docs/forge.md` says one device drives as many swap chains as there are windows, and no test opens two -
    a second window, surface, swap chain and frame context on the fixture's device, each presenting a frame.
    `EndFrame` leaving alone a frame that already moved its texture to `Present` (frame-context.hpp:96) is a
    one-line variant of `RecordClearFrame`.
14. **Multisampled depth.** `TextureDesc::sample_count` above one is only ever on a colour texture (4060,
    4142), so a pipeline `sample_count` beside a depth format, and a multisampled depth attachment, have never
    been built. A `Count4` colour-plus-depth pass with the near and far quads, colour resolved and read back,
    says the depth test ran at four samples.
15. **Small synchronization promises.** A fence created signalled answers `TryWait(0)` true
    (`create_signaled = true` is used only for the refusal lists at 1630 and 1637); `Fence::TryWaitForAll`
    timing out and `Semaphore::TryWaitForAll` succeeding (each has only the other outcome, 1435 and 1618);
    `GetElapsedMilliseconds` answering zero for an end query earlier than its start (query.hpp:125); a
    `CmdResetQueryPool` over part of a pool followed by writes into that part; `GetDebugMessageCount` with a
    type mask that leaves the message out (`General` after a validation error, unchanged).
16. **A sixteen bit unsigned specialization constant.** The odd-widths case (4575) covers `int16_t` and
    `int64_t`; a `uint16_t` is the `UInt32` report the header promises (shader.hpp:38) and rides on the same
    `shader_int16`. An eight bit one is not reachable - see the API gaps.
17. **Queues nothing asks for.** `use_decode_queue` and `use_encode_queue` (device.cpp:649-675) are never
    set: either a device with a `QueueFamily::Decode` queue whose family reports
    `VK_QUEUE_VIDEO_DECODE_BIT_KHR`, or `FeatureNotSupported`, and both outcomes are assertable in one
    section. `prefer_discrete = false` (device.hpp:207) is never passed; on a one-device machine it asserts
    only that the two answers agree.
18. **A descriptor or a push constant read by another stage.** Every `AddBinding` names `Compute`, `Fragment`
    or `Vertex` and every push range `Compute`, `Fragment` or `Vertex | Fragment`, so the
    `ToVkShaderStageFlags` entries for geometry, tessellation, task and mesh (command-buffer.cpp:1173,
    pipeline.cpp:204) are reached by no layout and no range. A mesh shader reading a storage buffer through a
    binding that names `ShaderTypeBits::Mesh`, and a push constant the geometry stage reads.
19. **Implicit LOD.** `SamplerDesc::lod_bias` was struck last time because every sampling shader uses
    `SampleLevel`; `max_anisotropy` above one and `mip_map_filter` under an implicit LOD are unproven for the
    same reason. All three want one fragment shader: a full-screen quad sampling a two-level texture with
    `Sample` at a UV scale that puts the footprint on level one, read back as which colour arrived; a
    `lod_bias` of minus one pulls it back to level zero. Bucket C.
20. **The `StringUtf8` overloads of the labels.** `CmdBeginDebugLabel`, `CmdInsertDebugLabel` and
    `ScopedDebugLabel` each have a `const Opal::StringUtf8&` form beside the `const char*` one, and only the
    `const char*` forms are called. One line each in "Forge debug labels" (2402).

---

## What is spelled twice

Setup written out where a helper exists or should. The helpers named below all exist; most of the copies
predate them.

- **The colour-only pass.** `CmdBeginRendering(rendering_desc)` is written out 28 times. About eleven of them
  are `RenderRaster` (7493) with a different clear colour - the colour write mask (3054), `DrawTwoTargets`
  (3445), the resolve (4069), specialization (4349), push constants read by two stages (8585), blending
  (9810), the blend table (12249), a draw that reads a descriptor set (10922), the transient attachment
  (14705), the writing pass of the read-only depth case (11489). A clear-colour parameter on `RenderRaster`,
  defaulting to the red it clears to now, folds them.
- **The fullscreen pipeline desc.** A two-float position binding, its attribute, one blend attachment, one
  colour format and `Face::None` is spelled eleven times (3044, 3495, 3563, 3930, 4038, 4329, 5022, 8051,
  10869, 14220) beside `MakeRasterPipeline` (7596), which came later than most of them.
- **The sampling body.** The second review folded the pool and the layout into `SampleHarness` (10110); what
  is left - a `Vector4f` buffer zeroed, a set, two updates, a submit, a read - is still written six times:
  `sample_with` (10167), `sample_shape` (10463), `sample_cube` (10744), `sample_at` (12596), the separate
  sampler case (10347) and the BC loop (14627). One `SampleOnce(fixture, harness, pipeline, texture, sampler,
  push_bytes)` returning the `Vector4f`.
- **A dispatch through one set.** Bind pipeline, bind set, dispatch, read words back is written 17 times.
  `MakeSetPipeline` and `DispatchIntoStorageBuffer` (14368-14401) came with bucket C and serve three cases at
  the end of the file; single resource (2764), by name (5381), the late update pair (7101, 7138), the set
  index pair (7244), several sets (7359, 7378), pool recycling (11007) and the every-kind pipeline (5333) are
  the same shape with the set prepared first.
- **The depth attachment texture.** A `D32_SFLOAT` depth target with `DepthStencilAttachment |
  TransferSource` is spelled nine times (8296, 8833, 8954, 9067, 9177, 11464 and the transient case);
  `MakeStencilTarget` (11416) is the `D24_UNORM_S8_UINT` one. A `MakeDepthTarget(device, side, format)` beside
  `MakeColorTarget` (7478).
- **The stencil pass preamble.** `run_pass` in "stencil testing" (9540-9565) and `run` in "stencil masks set
  per draw" (9689-9712) spell out what `BeginTableRendering` (9429) does, at the same side; both predate it.
- **`texel_at`** is defined twice, identically (8611, 8735), and `GetTexel` (3341) is the same function with
  `HalvesFixture::k_side`, which is the side both callers use.
- **The vertex and fragment pair.** 45 vertex and 41 fragment `FromSourceInMemory` calls, nearly all in
  pairs over one source with `.cache = GetShaderCache()`. Minor, and broad.
- The nine windowed preambles - skip without a window, skip without the format - are inherent: `SKIP` only
  works in the case body, so they cannot be a helper.

---

## Stale wording and hygiene

- **Three numbered references survived the last cleanup**: "the half of 1.7 nothing runs otherwise" (5876),
  "Rendering with one is 3.16" (6985), "1.1, which nothing checked" (window 819). Replace each with the fact.
- **A header disagrees with its code.** buffer.hpp:43 says `InvalidArgument` for initial data that does not
  fit; buffer.cpp:70 returns `OutOfBounds`, which is what `docs/forge.md` says a range that does not fit is.
  The header is the one to change, and the assertion at 744 to tighten - item 3.
- **A doc comment in two halves.** `MakeStencilWritePipeline` (9330-9339) has one `/** */` block ending and
  a second opening for the `@param` lines; a tool keeps the second only.
- **A title that outgrew its case.** "Forge sampler filtering and addressing" (10134) has filtering, the LOD
  clamp and the immutable sampler; the address modes moved to "Forge the sampler address modes and border
  colours" (12557).
- **What CI proves less of than a workstation**, unchanged: `IsSoftwareDevice()` skips the non-uniform
  indexing sections of the two bindless texture cases on lavapipe, the four constant blend factors `WARN`
  and sit out on a driver that rotates the constants, and `MirrorOnce` `WARN`s on a device without the
  feature.
- **Cost.** 118 seconds headless and 9 windowed, one device per section; unchanged from the last review and
  the lever, a context per binary with a device per case, is unchanged too.

---

## Doing the work

Three buckets again, by how much Vulkan judgment each needs. Do the cleanup first: items 1, 2 and 8 land on
the helpers it introduces, and item 3 is easier once fewer refusals are spelled out by hand.

Whichever model does a bucket, hand it: the test section of `CLAUDE.md` (no exceptions, `REQUIRE` ends the
run, `SKIP` only in the case body), `RNDR_TEST_REQUIRE_VULKAN=1` so a skip is a failure, the configure line
with `RNDR_FORGE=ON`, `RNDR_ASSIMP=ON` and `RNDR_FORGE_VALIDATION=ON`, and this file as the checklist. One
commit per item; `test(Forge): ...` for the test, `fix(Forge): ...` in its own commit for whatever the test
found.

### Bucket A: mechanical cleanup

**Sonnet 5, low effort.** Every change copies a pattern already in the file. One session, one commit per
bullet, the full `[forge]` and `[forge-window]` set run after each.

- Replace the three numbered references with the fact each one stands for; join the split doc comment;
  retitle the sampler filtering case.
- Fix buffer.hpp:43 to say `OutOfBounds`, and assert it at 744.
- Give `RenderRaster` a clear-colour parameter and move the eleven colour-only passes onto it.
- Move the eleven fullscreen pipeline descs onto `MakeRasterPipeline`.
- Add `SampleOnce` and move the six sampling bodies onto it.
- Move the dispatch-through-one-set cases onto `DispatchIntoStorageBuffer`, or a variant taking a set.
- Add `MakeDepthTarget`; move the two stencil preambles onto `BeginTableRendering`; drop the second
  `texel_at`.
- The static_assert table of item 5, in "Forge pure functions of the public headers".
- The three `StringUtf8` label overloads, item 20.

### Bucket B: pattern-following tests

**Sonnet 5, medium effort.** Each copies the shape of an existing case and asserts a readback or a code.
Two sessions.

Items 1, 2, 3, 6 (the texture half), 7, 8, 9, 10, 11, 12, 13, 15, 16, 17 and 18: the transfer and indirect
guards, the codes, the barrier range, the preset long forms, the feature and shape refusals, the blit
format refusals, the desc refusals, the arguments, the sampler halves, the windowed lifetimes and the second
window, the synchronization promises, the unsigned narrow constant, the video queues, the other stages.

### Bucket C: new shaders, Vulkan semantics, likely Forge changes

**Opus 5, high effort.** Each needs a shader written for it or a value worked out from the specification,
and one of them changes Forge. One item per session.

Items 4, 6 (the `BufferBarrier` check and its test), 14 and 19: what reflection refuses, the buffer barrier
range, multisampled depth, and implicit LOD.

### API gaps found on the way

Not test gaps, since nothing in the API reaches them, but worth a line each so the next feature has a list:

- A resolve at the end of a dynamic rendering pass (`VkRenderingAttachmentInfo::resolveImageView`), which is
  the resolve a tiled device wants and the only way to resolve depth. `CmdResolveTexture` is the standalone
  command. It is also what a transient multisampled colour attachment needs: `CmdResolveTexture` reads its
  source as a transfer source, which Vulkan does not allow beside the transient usage.
- Input attachments: no `DescriptorType::InputAttachment`, and reading one inside a dynamic rendering pass
  needs `VK_KHR_dynamic_rendering_local_read`.
- Comparison samplers: `SamplerDesc` carries no compare operation, so a shadow map cannot be sampled with
  `SampleCmp`.
- `AcquireTexture` twice without a `Present` hands `vkAcquireNextImageKHR` a hardcoded `UINT64_MAX`, so a
  surface with too few images hangs rather than reports. A timeout on `SwapChainDesc` would make it
  testable.
- No `shader_int8` or `shader_float16` in `DeviceFeatures`. A shader declaring an 8 bit specialization
  constant needs the `Int8` capability, which the layer refuses at `vkCreateShaderModule` without
  `shaderInt8`, so the one-byte `byte_size` shader.hpp:37 describes cannot be reached from a test, or from a
  caller.
- `BufferBarrier::offset` and `size` are not checked against the buffer - item 6 above, listed here because
  it is the one Forge change this review asks for.
