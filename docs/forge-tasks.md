# Forge (Vulkan) API — Task List

Tasks for the `forge` branch, ordered by priority. Each one lists the files it touches and what "done"
means. Priority 1 is broken behaviour, priority 2 is the naming and shape of the API while it still has a
single consumer, priority 3 is missing capability, priority 4 is convenience on top of a correct API.

The only consumer today is `samples/modern-vulkan/modern-vulkan.cpp`. Every task that changes a public
signature has to update that sample in the same commit.

---

## Priority 1 — Broken — ALL DONE

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
- `ImageLayout`, `PipelineStageBits`, `ImageSubresourceRange` and friends are Vulkan shaped but live in the
  shared `rndr/graphics-types.hpp`. Decide whether they belong in `Forge` as part of task 2.4.

### 2.2 Pick one error strategy

Constructors throw `VulkanException`, `Forge::PhysicalDevice::GetQueueFamilyIndex` returns
`Opal::Expected<u32, VkResult>`. Choose one and apply it everywhere. Throwing from constructors and
returning `Expected` from queries is defensible, but it has to be stated in the docs, not inferred.

### 2.3 Pick one parameter convention

The API mixes `const T&`, `Opal::Ref<T>` by value and `Opal::Ref<const T>` for the same kind of argument,
which is why the sample has to write `present_queue.Clone()` at `modern-vulkan.cpp:289`. Rule of thumb:
`const T&` for arguments that are only read during the call, `Opal::Ref` only where the object stores the
reference beyond the call.

### 2.4 Decide how much Vulkan leaks into the descs

`PixelFormat`, `ImageLayout`, `ShaderTypeBits` and `PipelineStageBits` are wrapped, but `VkBufferUsageFlags`,
`VkImageType`, `VkImageUsageFlags`, `VkSampleCountFlagBits`, `VkImageViewType`, `VkPresentModeKHR`,
`VkColorSpaceKHR` and `VkPhysicalDeviceFeatures` sit raw in the same structures. The user currently has to
know both vocabularies. Either wrap the remaining ones — `BufferUsageBits`, `TextureUsageBits`,
`SampleCount`, `PresentMode` — or drop the wrappers and be an explicitly thin Vulkan layer. The former fits
the rest of the codebase.

### 2.5 Consistent validity and accessors

- Add `IsValid()` to `Forge::Buffer`, `Forge::Pipeline`, `Forge::CommandBuffer`, `Forge::DescriptorSet`,
  `Forge::Fence`, `Forge::Semaphore`. Present on the other types already.
- `Forge::Texture::GetDesc()` returns by value; make it a const reference like every other `GetDesc`.
- Document the default-constructed empty state, or remove the default constructors from the types that only
  have one so they can live in containers.

### 2.6 Explicit descriptor binding indices

`include/rndr/forge/descriptor-set.hpp:48`

`Forge::DescriptorSetLayoutDesc::Binding` has no binding index — bindings are numbered by insertion order,
so a layout cannot skip a slot, reorder, or match a shader with non-contiguous bindings. Add the index, and
add immutable samplers while touching the struct.

---

## Priority 3 — Missing capability

### 3.1 Compute dispatch

`Forge::Pipeline` builds compute pipelines and `QueueFamily::AsyncCompute` exists, but there is no
`CmdDispatch`, so compute is unreachable. Add `CmdDispatch`, `CmdDispatchIndirect`.

### 3.2 Buffer and memory barriers

Only `CmdImageBarrier`/`CmdImageBarriers` exist. Without a buffer barrier there is no way to synchronize a
compute write with a graphics read. Add `Forge::BufferBarrier` and a global memory barrier, sharing the
`PipelineStageBits`/`PipelineStageAccessBits` vocabulary already in use.

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

---

## Priority 4 — Convenience

### 4.1 Frame context

The single biggest ergonomic gap. `modern-vulkan.cpp` hand-rolls frames-in-flight, the fence array, the
per-frame and per-image semaphores, and the acquire/submit/present ordering — the part that is hardest to get
right and near identical in every application. A `Forge::FrameContext` that owns the frame count, one command
buffer and one fence per frame, and exposes `BeginFrame()` / `EndFrame()` would roughly halve the sample.

### 4.2 Barrier presets

The sample spends about twenty lines per frame on two standard transitions. Add named constructors:
`ImageBarrier::ToColorAttachment(texture)`, `::ToPresent(texture)`, `::ToShaderRead(texture)`,
`::ToTransferDestination(texture)`.

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
