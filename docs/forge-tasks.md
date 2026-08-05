# Forge (Vulkan) API — Task List

Tasks for the `forge` branch, ordered by priority. Each one lists the files it touches and what "done"
means. Priority 1 is broken behaviour, priority 2 is the naming and shape of the API while it still has a
single consumer, priority 3 is missing capability, priority 4 is convenience on top of a correct API.

The only consumer today is `samples/modern-vulkan/modern-vulkan.cpp`. Every task that changes a public
signature has to update that sample in the same commit.

---

## Priority 1 — Broken

### 1.1 Fix swap chain recreation — DONE

Teardown is split into `DestroyImages()` and `DestroySwapChain()`, with `Destroy()` on top of both for the
full release. `Recreate()` waits for the device to go idle, passes the previous handle as `oldSwapchain` and
retires it after `vkCreateSwapchainKHR`, whether that call succeeded or not. A window with no client area,
as while minimized, leaves the object without a swap chain; the next `AcquireImage` tries again.

`AcquireImage` returns an `AcquiredImage` — a `SwapChainStatus` plus an image index — and `Present` returns
the status, so `SwapChainStatus::OutOfDate` tells the caller its cached image views, image count and extent
are stale. Present recreates on `VK_SUBOPTIMAL_KHR` as well; acquire renders the frame it already got an
image for and lets the matching present do it.

Verified by driving `modern-vulkan` through four resizes plus minimize and restore with the validation layer
on: the swap chain is rebuilt each time and no validation message is reported. The layer is behind the
`RNDR_FORGE_VALIDATION` CMake option, off by default.

### 1.2 Assign `m_extent` in `Recreate` — DONE

`Recreate()` stores the extent it created the swap chain with, and the sample renders at
`swap_chain.GetExtent()` instead of `window->GetSize()`, which can be a frame ahead of it.

### 1.3 Honour the offset in `Forge::Buffer::Update` — DONE

Both the mapped and the map-on-demand path apply the offset. The bounds check is written as
`offset > size || data.GetSize() > size - offset` so that a large offset cannot overflow the sum and pass,
and the initial data of the constructor is checked against the size of the buffer as well.

### 1.4 Carry `m_mapped_memory` across moves — DONE

The move constructor and move assignment transfer the pointer and null it in the source, and `Destroy()`
clears it. `Destroy()` now decides whether to unmap from the pointer instead of from
`m_desc.keep_memory_mapped`, so a moved-from buffer cannot unmap memory it no longer owns.

### 1.5 Flush non-coherent writes — DONE

A private `Flush(offset, size)` runs after the initial upload and after every `Update`, on both paths. VMA
compares the memory type against `HOST_COHERENT` and skips the flush when it is not needed, and rounds the
range out to `nonCoherentAtomSize` itself, so no coherency query is kept on the buffer.

### 1.6 Default-initialize every field of every desc — DONE

Every field of every desc in `include/rndr/forge/` now has a default. `Forge::SwapChainDesc` already had
one for `depth_pixel_format`; the rest were fixed:

- `BufferDesc::size` / `::usage` — zero, which the bounds check of task 1.3 turns into a thrown exception
  rather than a write into a buffer of unknown size.
- `RenderingAttachmentDesc` — all five fields. `image_layout` defaults to `ImageLayout::Undefined` so that
  an attachment left unconfigured fails loudly instead of rendering to the wrong layout, and `clear_value`
  is opaque black.
- `RenderingDesc::render_area_extent` — `Opal::Vector2` performs no initialization in its default
  constructor, so this needed an explicit `{0, 0}` rather than `= {}`.
- `VertexInputDesc::Attribute` and `::Binding`, `DescriptorSetLayoutDesc::Binding`,
  `PushConstantRange::shader_stages`, `DescriptorSetUpdateBinding::ImageInfo::image_layout`.
- `ImageBarrier` — the four sync fields to `None`, both layouts to `Undefined`.
- `SwapChainSupportDetails::capabilities` and the `VkDevice` held by `DescriptorSet`, which had the same
  problem without being descs.

`ShaderTypeBits` has no zero value, so the two fields of that type default to `AllGraphics` instead of an
"unset" state.

### 1.7 Free descriptor sets — DONE

`DescriptorPoolDesc::free_individual_sets` drives `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`, off by
default because a pool that is reset or destroyed as a whole is both cheaper and the common case. It is or-ed
into the flags rather than assigned, which the update-after-bind flag was not.

`DescriptorSet` holds an `Opal::Ref<const DescriptorPool>` and `Destroy()` calls `vkFreeDescriptorSets` when
that pool has the flag — calling it on a pool without the flag is invalid, so otherwise only the handle is
dropped, as before. The reference is carried across moves.

`DescriptorPool::Reset()` wraps `vkResetDescriptorPool` for the recycle-per-frame pattern. It invalidates
every set that came out of the pool, which is documented on the method; the sets are not tracked, so nothing
enforces it.

Verified against the validation layer by temporarily flipping the pool of `modern-vulkan` to
`free_individual_sets = true`: no message, clean exit.

### 1.8 Ignore queue family ownership in barriers — DONE

Neither barrier path wrote `srcQueueFamilyIndex` or `dstQueueFamilyIndex`, so both are zero-initialized to
family 0 rather than `VK_QUEUE_FAMILY_IGNORED`. On an image created `VK_SHARING_MODE_EXCLUSIVE` a source
equal to the destination means "no ownership transfer" whatever the value, which is why this was never seen
here. Swap chain images are created `VK_SHARING_MODE_CONCURRENT` whenever the graphics and present families
differ (`src/forge/swap-chain.cpp:335`), and a barrier on a concurrent image must pass
`VK_QUEUE_FAMILY_IGNORED` for both. On a GPU whose present family is not the graphics family that would be a
validation error on every transition of every frame.

Both fields are now written explicitly, in one `ToVkImageBarrier` the single and the batched path share.
Actual ownership transfer between families is part of 3.12.

### 1.9 Fix the resource pointers and the buffer range in `UpdateDescriptorSets` — DONE

`buffer_infos` and `image_infos` were built with one element per update and then `PushBack`-ed into, so they
grew past the size they were constructed with and the `pBufferInfo` / `pImageInfo` pointers already handed to
earlier writes followed the old allocation. Opal's `DynamicArray(count)` sets capacity equal to count and
grows by 1.5x, so the first push reallocates and a later one reallocates again once the pushes pass
`1.5n + 1` — five updates was enough. Both arrays are now written by index, which is what the pre-sizing was
for.

Measured rather than assumed: with five updates the old code left three of the five writes pointing into a
freed block, and the validation layer said nothing, because the read happens inside the driver where neither
it nor AddressSanitizer instruments it. Only a range check against the live array caught it.

`DescriptorSetUpdateBinding::BufferInfo::offset` and `::size` were ignored - every write named the whole
buffer from zero. They are honoured now, with `size` defaulting to `k_whole_buffer` the way
`BufferBarrier::size` already does, so an unset field keeps meaning the whole buffer. A range reaching past
the end of the buffer throws, written so a large offset cannot overflow the sum and pass, and a zero size
throws rather than reaching `vkUpdateDescriptorSets`, where a zero range is invalid.

---

## Priority 2 — Shape of the API

Do these while `modern-vulkan` is still the only consumer. Each one gets more expensive with every new user.

### 2.1 Rename `Advanced*` to the `Forge` namespace — DONE

Every `Rndr::Advanced*` type is now `Rndr::Forge::*`, the headers live in `include/rndr/forge/` and the
sources in `src/forge/`, both without the `advanced-` file name prefix. `include/rndr/forge/forward.hpp`
holds the forward declarations, since the previous inline `Opal::Ref<class Device>` style declares the type
in the wrong namespace on MSVC.

Left open, deliberately:

- `Rndr::Forge::Mesh` is CPU-side data with no Vulkan in it and could move next to the other asset loading
  code.
- `ApiComplexity::Basic` / `::Advanced` in `include/rndr/projection-camera.hpp` still uses the old wording
  for what is really Canvas versus Forge clip space.
- `ImageLayout`, `PipelineStageBits`, `ImageSubresourceRange` and friends were Vulkan shaped but lived in the
  shared `rndr/graphics-types.hpp`. Task 2.4 moved them to `rndr/forge/types.hpp`.

### 2.2 Pick one error strategy — DONE

The count decided it: about 90 throw sites against two functions returning `Opal::Expected` and one
returning an empty container after logging. Throwing wins, and the rule is written down in the error
handling section of `docs/forge.md`:

1. Failures throw, from ordinary methods as well as from constructors. `VulkanException` for a `VkResult`,
   `Opal::Exception` for misuse of the API.
2. Outcomes that are not failures are return values. A resized window is not an error, so acquire and
   present return `SwapChainStatus`; a device with no matching queue family is not a broken device, so
   `PhysicalDevice::GetQueueFamilyIndex` and `::GetPresentQueueFamilyIndex` return `Opal::Optional<u32>`.
   The `VkResult` they used to report was invented at the call site and carried nothing.
3. Never log and return a default. `GraphicsContext::EnumeratePhysicalDevices` throws instead, so that an
   empty list keeps its one meaning, and `RNDR_RETURN_ON_FAIL` is no longer used anywhere in Forge.

`Opal::Optional` has the same `HasValue()` / `GetValue()` shape as `Opal::Expected`, so the call sites in
`Device::CollectQueueFamilies` did not change.

### 2.3 Pick one parameter convention — DONE

References at the API surface are plain C++ references, and `Opal::Ref` is left for the places that have to
*store* one - members, desc fields, container elements. That a constructor keeps a reference to its argument
does not change its parameter type; every Forge constructor already took its device as `const Device&` while
storing an `Opal::Ref<const Device>`, and the rest now match. The rule is written down in the parameters
section of `docs/forge.md`.

- `SwapChain::Present` takes `DeviceQueue&` and `const Semaphore&`, `AcquireImage` takes `const Semaphore&`,
  and `Surface` takes `const GenericWindow&`. `Opal::Ref` is move-only, so passing one by value was what
  forced `present_queue.Clone()` at the call site.
- `Device::GetQueue` returns `DeviceQueue&` / `const DeviceQueue&`. It throws when the device has no such
  queue, so there was no absent case for `Opal::Ref` to carry. `SwapChain::Recreate` compares addresses now
  that it holds references.
- Ranges are `Opal::ArrayView<const T>` by value: `CmdImageBarriers`, `UpdateDescriptorSets` - which took a
  `const Opal::DynamicArray&` and so refused every other container - `Fence::WaitForAll` and the initial data
  of `Buffer`, which was a view over mutable `u8`. For a range of objects the element type is
  `Opal::Ref<const T>`, as in `CmdBindDescriptorSets`, since an array of references is not a thing.

Verified by running `modern-vulkan` with the validation layer for ten seconds and closing the window: clean
exit, no validation message.

### 2.4 Decide how much Vulkan leaks into the descs — DONE

Wrapped, which is what the rest of the codebase does. No desc field names a `Vk` type any more:

- `BufferDesc::usage` is `BufferUsageBits`, `TextureDesc::usage` is `TextureUsageBits` — both flag enums whose
  values mirror the Vulkan ones, so the mask translates as a cast, the way `PipelineStageBits` already did.
  `TextureDesc::image_usage` was renamed to `::usage` to match `BufferDesc`.
- `TextureDesc::image_type` is `TextureDimension`, `::view_type` is `TextureViewType`, `::sample_count` is
  `SampleCount`, `SwapChainDesc::present_mode` is `PresentMode` and `::color_space` is `ColorSpace`. These are
  translated by a `ToVk*` switch in the source file that uses them, so a value with no Vulkan counterpart
  cannot be cast into one by accident.

This also settles the question task 2.1 left open. The split is by what a type describes, not by who uses it:
`ImageLayout`, `ImageAspectBits`, `ImageSubresourceRange`, `PipelineStageBits` and `PipelineStageAccessBits`
are Vulkan concepts and moved to the new `include/rndr/forge/types.hpp` under `Rndr::Forge`, next to the new
enums. What any graphics API would recognize — `PixelFormat`, `ShaderTypeBits`, `IndexSize`, `Comparator` —
stays in `rndr/graphics-types.hpp` under `Rndr` and shared with Canvas. The reasoning is written down in the
"How much Vulkan is visible" section of `docs/forge.md`.

Left raw on purpose: `GetNative*()`, which is the escape hatch; `Surface::GetSwapChainSupportDetails` and the
queue family queries of `PhysicalDevice`, which report what the driver said rather than describe an object;
the `VkImage` constructor of `Texture`, which is how the swap chain wraps images it does not own; and
`DeviceDesc::features`, which is task 3.6. `RenderingAttachmentDesc::image_view` is still a raw `VkImageView`
and should become a texture reference when task 4.5 makes the depth attachment optional.

Verified by running `modern-vulkan` with the validation layer for ten seconds and closing the window: clean
exit, no validation message.

### 2.5 Consistent validity and accessors — DONE

`IsValid()` is now on every type. Beyond the six the task named, `DescriptorSetLayout`, `Device`,
`DeviceQueue`, `Sampler` and `Shader` were missing it too. `Forge::Texture::GetDesc()` returns a const
reference, matching every other `GetDesc`.

The default constructors stay, since the types have to live in containers and as members that are filled in
later, and the empty state is documented in `docs/forge.md` instead. Writing that section down turned up a
bug: `GraphicsContext::Destroy()` called `volkFinalize()` unconditionally, so an empty or moved-from context
going out of scope unloaded Vulkan process wide while another context was still using it. It now finalizes
only when it owned the instance.

### 2.6 Explicit descriptor binding indices — DONE

`DescriptorSetLayoutDesc::Binding` carries its own `binding` index, so a layout can skip a slot, list its
bindings in any order, and match a shader with non-contiguous bindings. The field is named and ordered the
way `VertexInputDesc::Binding` already names its own, and `AddBinding` takes the index first for the same
reason. Two bindings claiming the same index throw rather than silently shadowing one another.

`Binding::immutable_samplers` bakes samplers into the layout. It is only accepted on `DescriptorType::Sampler`
and `::CombinedImageSampler`, and its length has to equal `descriptor_count`; both are checked in `AddBinding`
rather than left to the validation layer. The samplers are collected into one array sized up front, so that
the `pImmutableSamplers` pointers stay valid until `vkCreateDescriptorSetLayout` has read them. Forge holds
`Opal::Ref<const Sampler>` and does not own them, so the caller has to keep them alive for as long as the
layout and its sets — documented on the field.

Verified by temporarily driving `modern-vulkan` with the two bindings added in reverse order, a third at index
7 that the shader never declares, and that third binding's sampler supplied as immutable: no validation
message, clean exit, probe removed.

Note for 3.9: the layout still passes an all-zero `binding_flags_array` and computes a `binding_flags` local
that nothing reads, so `use_update_after_bind` and `variable_descriptor_count` remain unreachable.

---

## Priority 3 — Missing capability

### 3.1 Compute dispatch

`Forge::Pipeline` builds compute pipelines and `QueueFamily::AsyncCompute` exists, but there is no
`CmdDispatch`, so compute is unreachable. Add `CmdDispatch`, `CmdDispatchIndirect`.

### 3.2 Buffer and memory barriers — DONE

`BufferBarrier` names a range of one buffer - offset and size, `k_whole_buffer` by default - and `MemoryBarrier`
names nothing at all, for work that touches more than is worth listing. Both carry the same four stage and
access fields as `ImageBarrier`, since buffers have no layout to transition.

`CmdBarriers` takes a `Barriers` group of all three kinds and issues one `vkCmdPipelineBarrier2`.
`CmdBufferBarrier`, `CmdBufferBarriers`, `CmdMemoryBarrier`, `CmdImageBarrier` and `CmdImageBarriers` are that
call with the other groups left empty, so batching across kinds costs one barrier where separate calls cost
several. `BufferBarrier::WriteThenRead` and `::ReadThenWrite` cover the two orderings a buffer needs.

Nothing in the repository issues one yet - the first real consumer is the compute dispatch of 3.1. Verified
instead by temporarily driving `modern-vulkan` with a buffer barrier and a memory barrier every frame, then
with all three kinds through one `CmdBarriers`: no validation message either way, clean exit, probe removed.

### 3.3 The rest of the draw calls

`CmdDraw` (non-indexed), `CmdDrawIndirect`, `CmdDrawIndexedIndirect`, and `CmdDrawMeshTasks` — the graphics
pipeline desc already accepts task and mesh shaders that nothing can draw with.

### 3.4 Copies, blits and readback

`CmdCopyBuffer`, `CmdCopyImage`, `CmdBlitImage`, `CmdCopyImageToBuffer`, plus a staging-buffer helper and
mip generation. Today a texture can only be uploaded from a `Bitmap` that already has its mips, and nothing
can be read back.

### 3.5 Richer submission

`Forge::DeviceQueue::Submit` takes exactly one command buffer, at most one wait semaphore with one stage,
one signal semaphore, and a mandatory fence. Add array-based submit, make the fence optional, and add
`WaitIdle()`. Timeline semaphores are the natural follow-up and would simplify the frames-in-flight
bookkeeping.

### 3.6 Device feature chaining

`Forge::DeviceDesc::features` is the Vulkan 1.0 `VkPhysicalDeviceFeatures`; the 1.2 and 1.3 features are
hardcoded in `src/forge/device.cpp:95-104`. Let the caller request 1.1/1.2/1.3 features and extension
feature structs, and fail with a clear message when the physical device does not support what was asked for.

### 3.7 Physical device selection

`EnumeratePhysicalDevices` returns the raw list and the sample takes element 0. Add a scoring or filtering
helper: prefer discrete, require a set of extensions and features, require present support for a given
surface.

### 3.8 Debug tooling

No `vkSetDebugUtilsObjectName`, no `vkCmdBeginDebugUtilsLabel`, no query pool or timestamps. Captures are
anonymous and there is no GPU timing. Add object naming on every resource — ideally a `debug_name` field on
each desc — command buffer labels, and a timestamp query pool.

### 3.9 Bindless plumbing

`Forge::DescriptorPoolDesc::use_update_after_bind` and the `variable_descriptor_count` argument on set
allocation exist, but the layout desc has no per-binding flags, so neither can actually be used. Add
per-binding `VkDescriptorBindingFlags` equivalents (partially bound, update after bind, variable count).

### 3.10 Pipeline gaps

Multisample state, per-attachment color write masks, pipeline cache (serialized to disk), specialization
constants, and dynamic state beyond viewport and scissor (depth bias, stencil reference, line width).

### 3.11 Tests

There are none for this layer; `test/` covers canvas only. Start with a headless smoke test: context →
device → buffer → compute dispatch → readback → verify. That single test would have caught 1.3, 1.4 and 1.5.

### 3.12 Complete the barrier vocabulary

`include/rndr/forge/types.hpp`, `include/rndr/forge/synchronization.hpp`, `src/forge/command-buffer.cpp`

`PipelineStageBits` is missing most of what a barrier needs to name. The task and mesh stages are absent
although the graphics pipeline desc already accepts task and mesh shaders; geometry and both tessellation
stages are missing; and `Transfer` is one bit where synchronization2 splits copy, blit, resolve and clear.

`PipelineStageAccessBits` is `Read` and `Write`, which map to `MEMORY_READ` and `MEMORY_WRITE`. That is legal
everywhere and easy to reason about, but it gives up the narrowing that synchronization2 exists for - a
barrier cannot say "color attachment write" rather than "any write". Decide whether the coarse model stays,
and write the decision down either way.

Also missing: ownership transfer between queue families, which 1.8 pinned to ignored, and `VkDependencyFlags`,
so a barrier cannot be by-region. Batching across barrier kinds is covered - 3.2 added `CmdBarriers`.

`CmdImageBarriers` translates into a `DynamicArray` through the default allocator on every call, so a
per-frame barrier batch heap-allocates. `Opal::GetScratchAllocator()` is not the fix on its own:
`modern-vulkan` pushes no scratch allocator, so asking for one asserts inside Opal. Either a small in-place
array with a heap fallback, or a scratch allocator that Forge pushes and resets itself.

---

## Priority 4 — Convenience

### 4.1 Frame context

The single biggest ergonomic gap. `modern-vulkan.cpp` hand-rolls frames-in-flight, the fence array, the
per-frame and per-image semaphores, and the acquire/submit/present ordering — the part that is hardest to get
right and near identical in every application. A `Forge::FrameContext` that owns the frame count, one command
buffer and one fence per frame, and exposes `BeginFrame()` / `EndFrame()` would roughly halve the sample.

### 4.2 Barrier presets — DONE

`ImageBarrier::ToColorAttachment`, `::ToDepthStencilAttachment`, `::ToShaderRead`, `::ToTransferDestination`,
`::ToTransferSource` and `::ToPresent` name the standard transitions. Each takes the texture and the layout
it is coming from, and derives the stages and the access from both ends: the destination from what the
texture is about to be used for, the source from what the old layout says it was last used for. Where the old
layout has an obvious answer it is the default, and where getting it wrong would silently discard the
contents of the texture - `ToShaderRead`, `ToTransferSource` - the caller has to say. The twenty lines per
frame in the sample are now two, and the two hand-written barriers inside `Texture` are one line each.

The subresource range now comes from the texture too. `ImageSubresourceRange` defaulted to the color aspect
and one mip level, so a barrier on a depth texture that forgot `aspect_mask` named the wrong aspect, and a
barrier on a mipped texture that forgot `mip_level_count` covered mip zero only - `Texture` worked around the
second one by hand. The default is now the whole texture: `k_all_mip_levels` and `k_all_array_layers`, which
mirror `VK_REMAINING_MIP_LEVELS` and `VK_REMAINING_ARRAY_LAYERS`, and an empty `aspect_mask` that
`ResolveAspectMask(format)` turns into depth, stencil, both or color. Barriers and the image view that
`Texture` creates both go through it, so the swap chain no longer spells out the depth aspect by hand.

### 4.3 Track the current layout on the texture

Once the texture knows its own layout, `old_layout` stops being something the caller can get wrong, and
`texture.TransitionTo(layout)` replaces most hand-written barriers. Needs care around swap chain images,
which the presentation engine transitions behind our back.

### 4.4 Shorter descriptor set updates

Binding two textures currently takes thirteen lines (`modern-vulkan.cpp:139-152`). Add overloads:
`set.Update(binding, texture, sampler, layout)` and `set.Update(binding, buffer, offset, size)`.

### 4.5 Optional depth attachment

`Forge::RenderingDesc::depth_attachment` is a value, so "no depth" is expressed as a null image view by
convention. Make it explicitly optional.

### 4.6 Swap chain remembers the acquired image

`AcquireImage` returns an index that the caller then threads through `GetColorImage`, `GetColorImageView`
and `Present`. Store it and add `GetCurrentColorImage()` / `GetCurrentColorImageView()`. Also make the
index parameters consistently `u32` — the sample casts to `i32` at `modern-vulkan.cpp:239`.

### 4.7 Write `docs/forge.md`

`docs/vulkan.md` is a set of notes on Vulkan concepts, not a guide to this API. A short document covering
object lifetimes, the frame loop, and who owns what would carry more weight than any single feature here.

The file now exists, created by task 2.2, but it only covers error handling. Object lifetimes and the frame
loop are still to be written.
