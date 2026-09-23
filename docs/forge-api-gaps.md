# Forge API gaps

What `include/rndr/forge/` cannot express yet, found by asking what a deferred renderer and a Nanite-style
GPU-driven one would need of it. Written against `ab39c9c` (2026-09-23). Each gap says what is missing, what
a caller does about it today, and what closing it takes - a Forge change and the test that goes with it, in
the shape `test/forge/` already uses.

The short version: a classic deferred renderer can be written on Forge today. A GPU-driven one cannot, and
the reasons are the first four items below.

**Status, 2026-09-23:** all twelve are closed, each by a `feat(Forge)` commit and a `test(Forge)` commit
named on the item. What is left open is the two costs under the list and the deferred test items at the end.

---

## What is there

Everything a forward or deferred renderer of the usual kind asks for, and every one of these has a test:

- Several colour attachments and a depth attachment in one dynamic rendering pass, blend state per target,
  any `PixelFormat`, HDR targets, a depth-only pass with no fragment shader.
- Attachments sampled in the next pass - colour through `ToShaderRead`, depth through a depth-aspect view -
  bound directly, by name, or through a bindless array with partially bound and variable count descriptors.
- Stencil with the full operation table per face, static and dynamic, which is what light volumes want.
- Compute with storage buffers, device addresses, push constants, specialization constants, indirect
  dispatch, and an async compute queue with ownership transfer in both directions.
- Mesh and task shaders, geometry and tessellation, each gated by a device feature and refused without it.
- Timestamps per frame in flight, debug names and labels, the shader cache, reflection that checks a layout
  and a vertex input against the shader.

---

## Gaps, in the order to close them

The first four are what stands between Forge and a GPU-driven renderer. The rest are what a deferred
renderer works around today, or what the next feature will trip over. A closed item keeps its text and says
where it was closed, so the order and the reasoning stay readable.

1. **Indirect draws with a device-written count.** `CmdDrawIndirect` and `CmdDrawIndexedIndirect` take
   `draw_count` at record time; `vkCmdDrawIndirectCount` and `vkCmdDrawIndexedIndirectCount` (core 1.2, the
   `drawIndirectCount` feature) read it from a buffer. A culling pass that writes the visible list has no way
   to say how long it is. *Today:* a fixed maximum count with `instance_count = 0` in the culled slots, which
   costs a command per slot whether it draws or not. *Takes:* `DeviceFeatures::draw_indirect_count` mapped
   onto `VkPhysicalDeviceVulkan12Features::drawIndirectCount`, `CmdDrawIndirectCount` and
   `CmdDrawIndexedIndirectCount` taking the count buffer, its offset and a `max_draw_count`, run through
   `ValidateIndirectRange` and a count-buffer range check of their own; a test on `HalvesFixture` where a
   compute shader writes both the commands and the count. *Closed* in `bf19ffc`, tested in `fe12e60`.
2. **Mesh task draws driven from a buffer.** Only the direct `CmdDrawMeshTasks` exists;
   `vkCmdDrawMeshTasksIndirectEXT` and `vkCmdDrawMeshTasksIndirectCountEXT` are what a cluster list feeds.
   *Today:* nothing; the mesh-shader cluster path cannot be fed by a culling pass at all. *Takes:* the two
   commands, a `DrawMeshTasksIndirectCommand` beside the other command structs, the same range checks, the
   extension guard `CmdDrawMeshTasks` already has; a test writing the workgroup counts from a compute shader.
   *Closed* in `12dd33d`, tested in `3537200`.
3. **64-bit atomics.** `DeviceFeatures` has `shader_int64` and no `shaderBufferInt64Atomics` or
   `shaderSharedInt64Atomics` (core 1.2). A software rasterizer packs depth, cluster and triangle into one u64
   and resolves it with `InterlockedMax`, which needs the buffer one. `DeviceDesc::extensions` cannot turn it
   on: that list enables extension names, and Forge builds the feature chain itself. *Today:* two 32-bit
   atomics for depth and id, which race. *Takes:* `DeviceFeatures::shader_buffer_int64_atomics` and
   `shader_shared_int64_atomics` mapped onto the two bits; a test dispatching a u64 `InterlockedMax` over a
   storage buffer and reading the maximum back, and the layer refusing the same shader with the field off,
   the way the scalar block layout case does. *Closed* in `3dd1379`, tested in `26cfea7`.
4. **A view apart from its texture.** A `Texture` owns one image and one view, and the view covers what
   `TextureDesc::subresource_range` says. Everything that wants two views of one image is stuck: a depth
   pyramid written one mip at a time as storage images, an array shadow map rendered into one layer per pass,
   a texture sampled whole and written by level. *Today:* the wrap overload `Texture::Create(device, VkImage,
   desc)` over the texture's own `GetNativeImage()`, once per view - it works, and every wrapper carries a
   layout tracker of its own that starts at `Undefined` and never learns what the barriers on the original
   did. *Takes:* a `TextureView` type, or `Texture::CreateView(desc)`, that names the texture it was taken
   from and reads the tracked layout off it; `RenderingAttachmentDesc` and `DescriptorSetUpdateBinding` taking
   a view where they take a texture; a test writing a mip chain by level and sampling it whole. This also
   unblocks item 5. *Closed* in `0e99738` as `TextureView`, tested in `50148a1`. An attachment view is
   refused over more than one mip level; a view over several layers waits on item 5.
5. **Layered rendering.** `VkRenderingInfo::layerCount` is hardcoded to one (command-buffer.cpp) and there is
   no view mask, so one pass cannot render into the layers of an array attachment - a cube shadow map is six
   passes, a cascaded shadow map is one per cascade, and multiview is out of reach. *Takes:*
   `RenderingDesc::layer_count` and `view_mask`, with `layerCount` read off the attachment views (item 4) or
   the desc; `DeviceFeatures::multiview`; a test rendering into two layers of one array texture in one pass
   with a geometry shader writing `SV_RenderTargetArrayIndex`, read back by layer. *Closed* in `02e9578`,
   tested in `f50aa8c` - with `DeviceFeatures::shader_output_layer` so the vertex stage picks the layer rather
   than a geometry stage.
6. **Filling a buffer from the command stream.** No `vkCmdFillBuffer` or `vkCmdUpdateBuffer`. Every counter,
   draw count and cluster list head has to be zeroed each frame, and today that is a dispatch or a copy from
   a buffer that holds zeros. *Takes:* `CmdFillBuffer(buffer, value, offset, size)` with the range check
   `ValidateBufferRange` already makes and the `TransferDestination` usage check; a test filling a range and
   reading the edges back. *Closed* in `53b909b` with `CmdUpdateBuffer` beside it, tested in `293fa1e`.
7. **Comparison samplers.** `SamplerDesc` has no compare operation, so a shadow map cannot be sampled with
   `SampleCmp` and there is no hardware PCF. *Today:* sample the depth and compare in the shader, one tap per
   texel. *Takes:* `SamplerDesc::compare_enabled` and `compare_operator` (a `Comparator`, which
   `ToVkCompareOp` already translates), and the view for a combined depth-stencil format restricted to one
   aspect; a test sampling a depth texture with `SampleCmp` at a reference above and below the stored depth.
   `SamplerDesc` is shared with Canvas, so the two fields have to mean nothing there or the same thing.
   *Closed* in `0d1a903`, tested in `bc656a3`; Canvas ignores both.
8. **A resolve at the end of the pass.** `CmdResolveTexture` is a standalone transfer; a tiled device wants
   `VkRenderingAttachmentInfo::resolveImageView`, and it is the only way to resolve depth, and the only way to
   resolve a transient multisampled colour attachment, since the standalone resolve reads a transfer source
   and Vulkan allows no such usage beside the transient one. *Takes:* `RenderingAttachmentDesc::resolve_texture`
   and a `ResolveMode`; a test drawing at four samples into a transient attachment and reading the resolved
   texture back, the way the multisampled draw case reads its standalone resolve. *Closed* in `a5a11d9`,
   tested in `e2fbcd8`, with `resolve_view` beside the texture the way `view` sits beside `texture`.
9. **Input attachments.** No `DescriptorType::InputAttachment`, and reading one inside a dynamic rendering
   pass needs `VK_KHR_dynamic_rendering_local_read`, which Forge does not ask for. Desktop deferred shading
   does not need it - sampling the G-buffer in a second pass is the same picture - and a tiled device does.
   *Takes:* the descriptor type, the extension behind a `DeviceFeatures` field, and `TextureUsageBits::
   InputAttachment` meaning something; a test reading the colour attachment of the current pass in its
   fragment shader. *Closed* in `567cff5` as `DeviceFeatures::dynamic_rendering_local_read`, tested in
   `f609023`. Input attachments are read in `General`; `RENDERING_LOCAL_READ` is not a Forge layout.
10. **Min and max sampling.** No `VK_EXT_sampler_filter_minmax`, so a depth pyramid is walked with four taps
    and a `max` by hand rather than one sample with a reduction mode. Small: a `SamplerDesc::reduction` field
    and the extension; a test sampling between two texels and getting the larger rather than the average.
    *Closed* in `c0f12a2` - core since 1.2, so a feature rather than an extension - tested in `7f83059`.
11. **A timeout on acquire.** `AcquireTexture` hands `vkAcquireNextImageKHR` a hardcoded `UINT64_MAX`, so a
    surface with too few images, or an acquire without a present between, hangs rather than reports.
    *Takes:* `SwapChainDesc::acquire_timeout`, and `VK_TIMEOUT` / `VK_NOT_READY` on the value side of the
    `Expected`, the way `OutOfDate` is; a windowed test acquiring twice without a present and getting the
    status rather than the hang. *Closed* in `dd9be1c`, tested in `59d6122`; `BeginFrame` returns the
    status too, so a frame loop tests for anything but `Success`.
12. **8 and 16 bit shader scalars.** `DeviceFeatures` has no `shader_int8` or `shader_float16`. A shader
    declaring an 8 bit specialization constant, or doing arithmetic in `half`, needs the `Int8` or `Float16`
    capability, which the layer refuses at `vkCreateShaderModule` without the feature - so the one-byte
    `byte_size` shader.hpp describes cannot be reached from a test or a caller. *Takes:* the two fields mapped
    onto `VkPhysicalDeviceVulkan12Features::shaderInt8` and `shaderFloat16`; the odd-widths specialization
    case gains a `uint8_t` constant. *Closed* in `2cb9d37`, tested in `65a6fb8` in a case of its own, so a
    device without 8-bit integers still runs the odd-widths one.

Two more that are not capabilities so much as costs, listed so nobody looks for them:

- **No pipeline cache.** `VkPipelineCache` is not exposed, so every pipeline is built from scratch each run.
  The shader cache takes the Slang cost, which is the large one; this is the remainder, and a
  `Device`-owned cache written to disk beside the shader cache would take it.
- **Primary command buffers only.** `CommandBuffer::Create` allocates a primary, so recording cannot be
  split across threads. A GPU-driven design records almost nothing per frame and does not miss it; a
  CPU-driven scene with thousands of draws would.

### What a deferred renderer hits

Items 7 and 4 or 5: shadows compare by hand and a cube or cascaded shadow map is a pass per face. Nothing
else. Item 8 only if it goes multisampled, item 9 only on a tiled device. All of them closed now.

### What a Nanite-style renderer hits

Items 1, 2, 3 and 4, in that order of pain, then 6 and 10. The compute path - persistent-thread culling into
a visible cluster list, a software rasterizer writing a visibility buffer, a material pass reading it back -
needs 1, 3, 4 and 6; the mesh-shader path needs 2 as well. All of them closed now; what such a renderer still
meets is the two costs above, and neither stops it.

---

## Test items deferred from the third review

`docs/forge-test-review.md` was retired once its buckets were done or judged not worth the time. What it
left open, one line each, kept here so the reason for skipping it is on record. Line numbers are gone with
the document; the case titles hold.

- The long forms of six barrier presets, `ToGeneral` with a non-default accessor, `GetCurrentLayout(range)`
  called directly.
- `PartiallyBound` without its feature, a `CubeArray` view without `image_cube_array`, a mesh pipeline handed
  a geometry stage, an attachment naming a transfer-only texture, `Device::GetQueue` for a family the device
  lacks, `SwapChain::Create` without a present queue, a present mode or format the surface does not offer.
- The three blit format refusals, which want a format the device cannot blit and may only skip.
- The desc-level refusals with no device: a pool type or a binding index added twice, immutable samplers on
  a storage buffer or with the wrong count, `AddAttribute` for a binding never added, a pool with no
  descriptor, `PhysicalDevice::Create` of a null handle, `BufferDesc::host_access` outside the enum.
- Arguments still at their defaults: `CmdDispatch` and `CmdDrawMeshTasks` in y and z, vertex and index
  buffer offsets, `CmdDrawIndexedIndirect` with several commands.
- `mip_map_filter = Linear` between two levels, the static stencil compare masks.
- `FrameContext` moves and a second window on one device; `EndFrame` leaving a frame that already moved its
  texture to `Present` alone.
- A multisampled depth attachment beside a multisampled colour one.
- A fence created signalled, the other outcome of each `TryWaitForAll`, `GetElapsedMilliseconds` for an end
  before its start, a partial `CmdResetQueryPool`, a `GetDebugMessageCount` mask that excludes the message.
- A `uint16_t` specialization constant; the video decode and encode queues; `prefer_discrete = false`.
- A descriptor or push constant read by a geometry, tessellation, task or mesh stage.
- Implicit LOD through a fragment draw with derivatives: `lod_bias`, `max_anisotropy`, the mipmap filter
  under `Sample` rather than `SampleLevel`.

Each checks a cast, a copied field or a one-line guard that a first real use would show wrong. Pick one up
when the area it covers is being worked on.
