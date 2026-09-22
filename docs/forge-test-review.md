# Forge test review

The second review of `test/forge/` against the public API in `include/rndr/forge/`: what the suite covers,
what it does not, and what it spells twice. It replaces the first review, whose every item was done or
recorded as left out on purpose. Line numbers refer to the working tree at `6cfb128` (2026-09-22) and will
drift; the case and section titles beside them will not.

Read together with `docs/forge.md`, which is what the claims below are checked against: a promise that
document makes and no test asserts is listed here as a gap whether or not the code keeps it.

---

## What is there

`test/forge/smoke-test.cpp` holds 113 headless cases over about 300 sections; `test/forge/window-test.cpp`
holds 9 windowed cases over 15. On an RTX 5060 Ti the headless set runs in 113 seconds and the windowed one
in 13. Nearly every case ends in a readback compared against a value worked out on the CPU, and every case
asserts that the validation layer reported nothing.

Covered well, and not worth another pass:

- The lifetime contract (empty state, moves, `Destroy`) for all 16 handle types, each asked to do its job
  after a move.
- Buffers: update and read at an offset, every host access kind, unmapped memory, device-only through the
  staging helpers, the edges of every range.
- Compute dispatch, direct and indirect, with device-written arguments; the async compute and dedicated
  transfer queues; ownership transfer of a buffer and of a texture between two families.
- Transfers: blits that scale, mirror, convert and filter; buffer to texture and back with sub-boxes,
  offsets, row lengths and layer heights; texture to texture by level and by layer; mip generation over
  every layer; the bitmap upload path; mip level sizes; a multisampled draw resolved to one sample.
- Barriers: all three kinds in one batch, the presets, the tracked layout per subresource, `Reset` not
  rolling it back, the by-region flag, the read-only depth layout written out by hand.
- Synchronization: fences and `WaitForAll`, binary and timeline semaphores host and device side, batched
  submits, a command buffer submitted twice, every refusal of an empty or mismatched object.
- Descriptors: bindless with partially bound and variable count, update after bind and update while
  pending, immutable samplers, single-resource and by-name updates, a set index other than zero, pool reset
  and per-set free, several sets bound at once, a set bound for a graphics draw.
- Pipelines: specialization constants for graphics and compute at 16, 32 and 64 bits, vertex input and
  push constants derived from reflection and checked against it, push constants at an offset and across two
  stages, several colour attachments with and without `independent_blend`, sample counts, dynamic state, a
  rejected pipeline leaving no layout behind.
- Every stage: vertex and fragment everywhere, compute everywhere, mesh, task in front of mesh, geometry,
  both tessellation stages, and every gated stage refused on a device that did not enable it.
- The fixed-function tables, each enumerator checked by its effect, with a section proving no two
  enumerators answer alike: stencil operations, comparators, blend factors and operations, address modes and
  border colours, colour write masks, cull and winding, fill modes, line and point topologies, viewport and
  scissor, depth range, clamp, bias and bias clamp, depth test, load and store operations, stencil per face.
- Sampling: 2D, 3D, 1D, every array shape, cube and cube array, a view over one mip level, LOD clamp,
  separate sampler and image, storage image writes.
- Timestamps blocking and not, host reset, the range checks, timing on another family's valid bits.
- Debug names on every headless type, labels and the GPU event macros, the message log and its cap.
- Shaders from source in memory and on disk, from SPIR-V in memory and on disk, the shader cache in every
  way it can miss.
- `LoadMesh` including what assimp generates and what it refuses.
- Surface, swap chain and frame context: format support, present modes, the depth format, presented
  frames read back, the frame loop past the end of its timeline, resize, a window with no client area, a
  device created before any surface, every frame context error path.

---

## What is missing

In rough order of value. The first five are where a wrong translation table entry or a dropped field would
still pass today.

1. **Reflection of the other four descriptor kinds.** `DescriptorSetLayoutDesc::shaders` is the check that a
   layout matches what the shader declares, and `ToDescriptorType` in shader.cpp maps six SPIR-V kinds onto
   `DescriptorType`. Only two of them ever reach the check: `CombinedImageSampler` through
   `k_reflected_source` (4436, 4621) and `StorageBuffer` through `k_named_storage_source` and
   `k_two_set_source` (4744, 6512). `Sampler`, `SampledImage`, `StorageImage` and `ConstantBuffer` are bound
   and dispatched through elsewhere, but always by a layout built without `shaders`, so a swapped entry for
   any of the four would go unreported. One shader declaring all six kinds, one layout naming them, and the
   "declared as the wrong kind" refusal per kind.
2. **A layout sized for fewer descriptors than the shader indexes.** Promised by `docs/forge.md` under what
   a layout given `shaders` refuses, implemented at descriptor-set.cpp:329, and asserted nowhere. A shader
   indexing `textures[4]` against a layout declaring two; the array bindings in the bindless cases (1720,
   1847) are built without `shaders`, so they never reach it.
3. **The stencil fail and depth-fail operations.** `DepthStencilDesc::front_stencil_fail`,
   `front_depth_fail` and the two back-face fields are never set; every stencil case drives the operation
   table through `*_pass` alone. Those are three separate slots of `VkStencilOpState`, and a fail operation
   wired into the pass slot, or the two fail slots exchanged, is invisible today. Needs a draw whose stencil
   test fails (`Comparator::Never` with a `Replace` fail operation) and one whose stencil passes and depth
   fails, each read back through `ReadStencilValue` (10310).
4. **The slope factor of the depth bias.** `RasterizerDesc::depth_bias_slope_factor` is never non-zero and
   the third argument of `CmdSetDepthBias` is never given; every biased quad is flat, so the slope term
   contributes nothing whatever it is set to. A quad tilted in z with a slope factor and no constant factor,
   read back at two texels and compared against the formula in the specification.
5. **A blit of part of the source.** `TextureBlitRegion::source_offset` and `source_extent` are never set:
   every blit in "Forge blits" (5487) reads the whole source level, so the source side of `ResolveBlitBox`
   has only ever been asked for the default box. A blit of the top right quarter of the source into the
   whole destination is the counterpart of the copy case at 5896.
6. **Specialization constants of floating point and unsigned 64-bit type.** `Float32` reaches a shader only
   as the wrong type that is refused (4190); `Float64` and `UInt64` never appear. Four of the seven
   `SpecializationValue` constructors and the `Store` path for them are unexercised. The 64-bit pair wants
   `DeviceFeatures::shader_float64`, which is item 10.
7. **The strip topologies.** `PrimitiveTopology::LineStrip` and `TriangleStrip` are never drawn; two of the
   six entries of `ToVkPrimitiveTopology` are checked only by the pipeline building. A three-vertex line
   strip covers two rows where a line list of the same vertices covers one, which is the shape the topology
   case at 7023 already counts.
8. **Draw arguments passed by position and never varied.** `CmdDraw` is never given a `first_vertex` or a
   `first_instance`; `CmdDrawIndexed` never an `instance_count` above one; `CmdDrawIndirect` never a stride
   other than the default, although `ValidateIndirectRange` (command-buffer.cpp:1263) has a rule for it; no
   indirect command carries an `instance_count` above one. `HalvesFixture` (3125) already tells vertices,
   instances and halves apart, so each of these is a section on an existing case.
9. **Windowed claims of `docs/forge.md` with no assertion.** `AcquireTexture` resetting the texture it
   hands out to `Undefined` - read `GetCurrentLayout` off the acquired texture before the first barrier;
   `Device::CanPresentTo` on a device created without presentation answering false rather than failing;
   `SwapChainDesc::color_space` other than `SrgbNonlinear`, skipping when the surface offers none;
   `SetDebugName` for `SwapChain` and `FrameContext`, the two overloads left out of the headless loop at
   1105.
10. **Device features nothing asks for.** `texture_compression_bc`, `shader_float64`,
    `runtime_descriptor_array`, `variable_descriptor_count` and `scalar_block_layout` each map onto a
    Vulkan feature bit in device.cpp and are never requested, so a mapping onto the wrong bit is silent
    until a shader needs it. `shader_float64` pairs with item 6. `texture_compression_bc` wants a BC
    texture uploaded and sampled, since `GetMipLevelSize` refuses to size one for a readback.
    `scalar_block_layout` wants a shader whose block only lays out under it. `variable_descriptor_count`
    and `runtime_descriptor_array` are what the bindless cases rely on through
    `descriptor_indexing`; whether they can be asked for alone is the question.
11. **A texture view over an array layer sub-range.** `TextureDesc::subresource_range` is tested for a mip
    sub-range only (9505). A view over layer one of a two-layer array, sampled as a flat texture, is the
    other half of that field.
12. **Sample counts other than one and four.** The resolve case (3827) probes `Count4` only. Parameterising
    it over every count the device reports in `framebufferColorSampleCounts` covers the rest of
    `ToVkSampleCount` for the price of a loop.
13. **`TextureUsageBits::TransientAttachment`.** Never used. A lazily allocated multisampled attachment with
    a `DontCare` store is what it is for, and whether the allocation lands in lazily allocated memory is
    readable off VMA. `InputAttachment` has no `DescriptorType` behind it and no way to read one in a
    dynamic rendering pass without `VK_KHR_dynamic_rendering_local_read`, so it is an API gap rather than a
    test gap - see the end of this document.
14. **`SamplerDesc::lod_bias`.** Translated to `mipLodBias` at texture.cpp:536 and never set. The LOD clamp
    section at 9078 has the two-level texture it needs: a bias of one with the clamp open reads the lower
    level from a request for the upper one.
15. **Pure functions in public headers with no direct test.** `ResolveAspectMask`, `IsDepthFormat`,
    `IsStencilFormat`, `ImageLayoutToString` in types.hpp and `VkResultToString` in vulkan-result.hpp.
    `ResolveAspectMask` decides the aspect of every copy on a depth or stencil format, and a table over the
    formats in `PixelFormat` costs nothing and needs no device, the way "Forge mip level sizes" (6020) and
    the `VkResult` mapping (366) need none.
16. **The two instance extension queries.** `GraphicsContext::GetRequiredInstanceExtensions` and
    `GetSupportedInstanceExtensions` are public statics with no caller in the suite. That the required list
    is a subset of the supported one on this machine is the whole of what they can be asked.
17. **Readback of the depth aspect of a combined format.** `ImageAspectBits::Stencil` is named once
    (`ReadStencilValue`); `Depth` is never named, so a copy out of the depth aspect of `D24_UNORM_S8_UINT`
    is untested, and so is the size such a copy wants.
18. **Two small refusals the headers promise.** `Buffer::Create` with initial data larger than the buffer
    (buffer.cpp:69) - the tested refusal at 566 is `Update`, not `Create`. `GetDebugMessageCount` for `Info`
    counting what `GetDebugMessages` does not store, which `docs/forge.md` states under Debugging.
19. **Two descs never driven off their defaults.** `VertexInputDesc::FromShader` with a binding other than
    zero or a per-instance rate; `DescriptorSetUpdateBinding::TextureInfo` through the array overload of
    `Update`, which only ever carries `BufferInfo` (1758).

---

## What is spelled twice

Nothing here is redundant in the sense of the first review, which found sections asserting what another
section asserted. What is left is setup written out where a helper exists or should:

- **The three bindless cases** (1720, 1847, 2002) each enumerate physical devices, call
  `FindPhysicalDevice`, `SelectPhysicalDevice` and `Device::Create` by hand, about twelve lines apiece. They
  predate `CanCreateDevice` and `ForgeFixture(features)`, which is what every case written since uses. The
  present-preset case (6333) builds its device by hand too, for a reason - it needs
  `DeviceDesc::extensions` - and a fixture that took an extension list would fold it as well.
- **The colour-plus-depth pass** is written out in "viewport depth range" (7395), "depth testing" (7950),
  "depth bias" (8106), "a clamped depth bias" (8242) and "a depth attachment that is read and not written"
  (10470): the same two barriers, the same `RenderingDesc` with a depth attachment, the same viewport and
  scissor, about thirty lines each. `RenderRaster` (6799) is the colour-only version of this; a
  `RenderWithDepth` beside it removes five copies.
- **The stencil write pipeline** - `Replace` on `Always`, colour masked off - is built four times: the
  `mask_pipeline` lambdas in "stencil testing" (8336) and "stencil masks set per draw" (8486), the
  `stamp_pipeline` in "stencil state that differs between the faces" (11723), and `MakeStencilWritePipeline`
  (10336), which came last and could serve the other three given a dynamic-state argument.
- **The sampling harness** - pool, layout, pipeline, output buffer, set, dispatch, read a `Vector4f` back -
  is repeated in "sampler filtering" (8993), "separate sampler" (9171), "texture shapes" (9314), "a cube
  array view" (9547) and "address modes" (11515), about twenty five lines each.
- **Provoking one validation error** by copying between two formats of different texel size is spelled out
  in "debug names reach the validation layer" (1050) and again in the `max_stored_debug_messages` section of
  "context desc" (463).
- **`ReadBackTexture` into a `k_side * k_side * 4` array left in `TransferSource`** appears 47 times. A
  `ReadBackPixels(fixture, texture, side)` would shorten most of them by three lines.
- **One section asserts nothing past `Success`.** "A texture written the short way records without
  complaint" (2638) writes a combined sampler descriptor and checks the return code. The same overload is
  proven through a shader by "sampler filtering" and by the graphics draw at 9704, so the section can go.

---

## Stale wording and hygiene

- **A doc comment sits over the wrong case.** The comment beginning "Every StencilOperation, checked by what
  it leaves in the stencil buffer" (10458) heads `TEST_CASE("Forge a depth attachment that is read and not
  written")`; the case it describes is "Forge the stencil operations" at 10699, which has no comment of its
  own. The read-only depth case was inserted between the two.
- **Nine comments cite numbers from a task list that no longer exists**: "3.11 found three of them" (4944),
  "the member 1.4 found" (5136), "when 3.18 gave it a preset" (6273), "3.16's business" (8507), "3.3 wrote
  it ... 3.6 enabled it" (12575), "the one call 3.21 left" (window 312), "4.5 from the other side" (window
  402, 589). The commit guidance in `CLAUDE.md` says why: the list they refer to has been renumbered out of
  existence, and each one now reads as a reference to nothing. Replace each with what the number meant.
- **`Forge::Sampler` ignores two `SamplerDesc` fields.** `base_mip_level` and `max_mip_level` are read by
  Canvas and never by texture.cpp, which maps `min_lod`, `max_lod` and `lod_bias`. A caller setting them on
  a Forge sampler gets nothing and no message. Either map them onto the LOD range or say in the header that
  they are Canvas's.
- **Two sections skip on a software device with no witness in CI.** `IsSoftwareDevice()` (192) skips the
  non-uniform indexing sections of the two bindless texture cases on lavapipe. A skip is reported, which
  is better than the `WARN` the blend table gives its four constant factors on the same driver, but both
  mean CI proves less than a workstation does, and neither is written down anywhere but here.
- **Cost.** 100 fixture constructions in the source, one per section at run time, so a full run creates a
  device several hundred times; 113 seconds headless is the price today. A context shared per binary with a
  device per case remains the lever if that grows.

---

## Doing the work

Three buckets again, by how much Vulkan judgment each needs. Do the cleanup first: the depth and stencil
items in the last bucket land on the helpers it introduces.

Whichever model does a bucket, hand it: the test section of `CLAUDE.md` (no exceptions, `REQUIRE` ends the
run, `SKIP` only in the case body), `RNDR_TEST_REQUIRE_VULKAN=1` so a skip is a failure, the configure line
with `RNDR_FORGE=ON`, `RNDR_ASSIMP=ON` and `RNDR_FORGE_VALIDATION=ON`, and this file as the checklist. One
commit per item; `test(Forge): ...` for the test, `fix(Forge): ...` in its own commit for whatever the test
found.

### Bucket A: mechanical cleanup

**Sonnet 5, low effort.** Every change copies a pattern already in the file. One session, one commit per
bullet, the full `[forge]` and `[forge-window]` set run after each.

- Move the stencil operations comment onto its case.
- Replace the nine numbered references with the fact each one stands for.
- Rebuild the three bindless cases on `CanCreateDevice` and `ForgeFixture(features)`; give the fixture an
  extension list so the present-preset case can use it too.
- Add `RenderWithDepth` beside `RenderRaster` and move the five depth passes onto it.
- Give `MakeStencilWritePipeline` a dynamic-state parameter and move the three other stencil write
  pipelines onto it.
- Fold the sampling harness into one struct used by the five sampling cases.
- One helper for provoking a validation error, one for reading a colour target back.
- Delete the section that asserts only `Success`.
- Note in the header that `SamplerDesc::base_mip_level` and `max_mip_level` are Canvas fields, or map
  them.

### Bucket B: pattern-following tests

**Sonnet 5, medium effort.** Each copies the shape of an existing case and asserts a readback or a code.
Two sessions.

Items 2, 7, 8, 9, 11, 12, 14, 15, 16, 17, 18 and 19: the undersized layout refusal, the strip topologies,
the draw arguments, the four windowed claims, the layer sub-range view, the other sample counts, the LOD
bias, the pure function tables, the instance extension queries, the depth aspect readback, the two header
refusals, and the two desc defaults.

### Bucket C: new shaders, Vulkan semantics, likely Forge changes

**Opus 5, high effort.** Each needs a shader written for it or a value worked out from the specification,
and two of them will likely change Forge. One item per session.

Items 1, 3, 4, 5, 6, 10 and 13: a shader declaring every descriptor kind and the layout check against it;
the stencil fail and depth-fail operations; the slope factor with a tilted quad; a blit of part of the
source; the floating point and 64-bit specialization constants; the five device features nothing asks for;
the transient attachment.

### API gaps found on the way

Not test gaps, since nothing in the API reaches them, but worth a line each so the next feature has a list:

- A resolve at the end of a dynamic rendering pass (`VkRenderingAttachmentInfo::resolveImageView`), which is
  the resolve a tiled device wants and the only way to resolve depth. `CmdResolveTexture` is the standalone
  command.
- Input attachments: no `DescriptorType::InputAttachment`, and reading one inside a dynamic rendering pass
  needs `VK_KHR_dynamic_rendering_local_read`.
- Comparison samplers: `SamplerDesc` carries no compare operation, so a shadow map cannot be sampled with
  `SampleCmp`.
- `AcquireTexture` twice without a `Present` hands `vkAcquireNextImageKHR` a hardcoded `UINT64_MAX`, so a
  surface with too few images hangs rather than reports. A timeout on `SwapChainDesc` would make it
  testable.
