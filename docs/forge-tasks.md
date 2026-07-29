# Forge (Vulkan) API — Task List

Tasks for the `forge` branch, ordered by priority. Each one lists the files it touches and what "done"
means. Priority 1 is broken behaviour, priority 2 is the naming and shape of the API while it still has a
single consumer, priority 3 is missing capability, priority 4 is convenience on top of a correct API.

The only consumer today is `samples/modern-vulkan/modern-vulkan.cpp`. Every task that changes a public
signature has to update that sample in the same commit.

---

## Priority 1 — Broken

### 1.1 Fix swap chain recreation

`src/forge/swap-chain.cpp`

`Destroy()` nulls `m_device`, `m_surface` and resets `m_desc` (`:161-164`), and `AcquireImage`/`Present`
call `Destroy(); Recreate();` (`:177`, `:203`). `Recreate()` immediately dereferences `m_surface` and
`m_device` (`:215`), so any window resize is a null dereference.

- Split teardown: a `DestroyResources()` that releases the images, views and the `VkSwapchainKHR` but keeps
  the device, surface and desc, and a full `Destroy()` that additionally drops them.
- Wait for the device to go idle before releasing images that may still be in flight.
- Pass the old swap chain to `oldSwapchain` and destroy it after the new one is created. Right now `:262`
  sets it and `:295` overwrites it with `VK_NULL_HANDLE`.
- Recreate on `VK_SUBOPTIMAL_KHR` from present as well, not only on `VK_ERROR_OUT_OF_DATE_KHR`.
- Decide and document what `AcquireImage` returns when the swap chain was recreated, so the caller knows its
  cached image views are stale. Returning an explicit "out of date, retry next frame" result beats looping
  inside the acquire call.

**Done when:** resizing the window in `modern-vulkan` keeps rendering, with validation layers clean.

### 1.2 Assign `m_extent` in `Recreate`

`src/forge/swap-chain.cpp:244-252`

The extent is computed into a local and never stored, so `GetExtent()` returns `{0, 0}` for the whole
lifetime of the swap chain. Store it and make the sample use it instead of `window->GetSize()`.

### 1.3 Honour the offset in `Forge::Buffer::Update`

`src/forge/buffer.cpp:113`

The parameter is unnamed and ignored, so every update writes at offset 0. Apply the offset in both the
mapped and the map-on-demand path, and bounds check `offset + data.GetSize()` against `m_desc.size`.

### 1.4 Carry `m_mapped_memory` across moves

`src/forge/buffer.cpp:66-95`

The move constructor and move assignment do not transfer `m_mapped_memory`, and `Destroy()` does not reset
it. A moved buffer with `keep_memory_mapped = true` silently falls back to map/unmap on every update — which
is what the sample does for its per-frame uniform buffers. Transfer the pointer, null it in the source, and
clear it in `Destroy()`.

### 1.5 Flush non-coherent writes

`src/forge/buffer.cpp`

VMA with `VMA_MEMORY_USAGE_AUTO` and `HOST_ACCESS_SEQUENTIAL_WRITE` may pick non-coherent memory. Call
`vmaFlushAllocation` after the initial upload and after every `Update`, or query the memory properties once
at creation and only flush when the allocation is not coherent.

### 1.6 Default-initialize every field of every desc

`include/rndr/forge/*.hpp`

The API is designated-init based, so any field left out of the initializer keeps whatever was on the stack.
Offenders: `Forge::BufferDesc::size` and `::usage`, all of `Forge::RenderingAttachmentDesc`,
`Forge::SwapChainDesc::depth_pixel_format`, `Forge::VertexInputDesc::Attribute` and `::Binding`,
`Forge::DescriptorSetLayoutDesc::Binding`, `Forge::PushConstantRange::shader_stages`,
`Forge::ImageBarrier` sync fields.

### 1.7 Free descriptor sets

`src/forge/descriptor-set.cpp:298`

`Forge::DescriptorSet::Destroy()` only clears the handle; the set is never returned to the pool. Give the
set a reference to its pool, call `vkFreeDescriptorSets` when the pool was created with the free-descriptor-set
flag, and add `Forge::DescriptorPool::Reset()` for the recycle-per-frame pattern.

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
