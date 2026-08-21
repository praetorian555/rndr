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

### 3.1 Compute dispatch — DONE

`CmdDispatch` takes the three group counts, `y` and `z` defaulting to one, since a one dimensional dispatch
is the common case. `CmdDispatchIndirect` reads them out of a buffer instead, and `DispatchIndirectCommand`
gives the three counts a name so that the caller fills the buffer through a Forge struct rather than
knowing the layout. It checks what it can before the call: the buffer needs
`BufferUsageBits::IndirectBuffer`, the offset has to be a multiple of four, and the command has to fit,
checked so a large offset cannot overflow the sum and pass. Group counts are not checked against the device
limits - that needs limits plumbing that does not exist yet, and the validation layer already covers it.

`Buffer::GetDesc()` was added along the way. Every other Forge type had one after 2.5 and `Buffer` did not,
and the usage check needs it.

Verified end to end rather than by the absence of validation messages, which proved nothing for 1.9: a probe
compute shader wrote `index + 1000` into a 256 element storage buffer, dispatched over four groups of 64.
The buffer was wiped and read back before each pass, so a pass could not be shown green by what the previous
one left behind - zero before, 1000 through 1255 after, no mismatches, for the direct and the indirect path
both. All three guards threw. Probe removed.

Left for later: `PipelineStageBits` has no `Host`, so the probe drained with `AllCommands` where a
compute-to-host barrier is what it wanted. That belongs to 3.12 along with the other missing stages.

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

### 3.3 The rest of the draw calls — DONE

`CmdDraw`, `CmdDrawIndirect`, `CmdDrawIndexedIndirect` and `CmdDrawMeshTasks`. `DrawIndirectCommand` and
`DrawIndexedIndirectCommand` name the buffer contents the way `DispatchIndirectCommand` does, so a caller
fills an indirect buffer through a Forge struct instead of knowing the Vulkan layout.

The checks the indirect commands share moved into one `ValidateIndirectRange`, which `CmdDispatchIndirect`
now goes through as well - buffer usage, offset alignment, and that every command the device will read fits,
written so neither the offset nor the span of the commands can overflow the sum and pass. Above one command
it also checks the stride, since that is what turns a small buffer into an out of bounds read.

`CmdDrawMeshTasks` is the one that cannot run here. Nothing enables `VK_EXT_mesh_shader` yet, and the
important part is that a null check does not catch that: the loader hands out a callable trampoline for an
extension command whether or not the device enabled it, so the first version of this crashed with an access
violation instead of failing. It asks the device now, through the new `Device::IsExtensionEnabled`, which
answers from the list that actually went to `vkCreateDevice` rather than from the desc, since the swap chain
extension is added after the desc is copied. Enabling the extension is 3.6.

Verified by counting what the GPU actually did rather than by the absence of validation messages. A probe
storage buffer bound to the vertex shader took a `1` at each vertex index it saw, so the count of ones after
a draw is exactly the set of vertices that draw processed. Wiped between passes, with the direct and the
indirect path deliberately given different counts so that one could not be read as the other:

- `CmdDrawIndexed` over the whole mesh: 2012 of 2012.
- `CmdDraw(300)`: 300.
- `CmdDrawIndirect`, 600 in the buffer: 600.
- `CmdDrawIndexedIndirect`, whole mesh in the buffer: 2012.

Each matches its own argument, and the two indirect ones match what was in their buffers rather than what the
call site said, which is the thing worth proving. All six guards threw, `CmdDrawMeshTasks` included. Probe
removed.

Worth knowing for later: an early version of the probe drew `mesh.vertex_count` vertices non-indexed and
reported 2010 of 2012, which reads like a bug and is not one - 2012 vertices as a triangle list is 670 whole
triangles, and the two left over never form a primitive, so they never reach the vertex shader.

### 3.4 Copies, blits and readback — DONE

`CmdCopyBuffer`, `CmdCopyImage`, `CmdCopyImageToBuffer` and a region-based `CmdCopyBufferToImage` that the
`Bitmap` overload is now written on top of, which is what removed its hardcoded color aspect and its single
array layer. Regions are `BufferCopyRegion`, `BufferImageCopyRegion` and `ImageCopyRegion`, all sharing
`ImageSubresourceLayers` - the single-level counterpart of `ImageSubresourceRange`, whose aspect resolution
both now go through. A zero extent means the rest of the mip level past the offset, so the common case names
nothing at all.

Readback needed more than a copy command. Every buffer was allocated
`VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT`, which is legal to read and slow enough to be a bug,
and nothing called `vmaInvalidateAllocation`, which non-coherent memory needs before a host read.
`BufferDesc::host_access` picks the memory type and `Buffer::Read` invalidates before copying out, throwing
when the buffer is not `HostAccess::Random` rather than being quietly slow.

`rndr/forge/transfer.hpp` holds `ImmediateSubmit` - record, submit, wait - and `UploadToBuffer`,
`ReadBackBuffer` and `ReadBackTexture` on top of it. `Texture`'s bitmap constructor was the one place that
spelled a one-shot submit out by hand and now goes through it. No barrier to the host is issued before the
readback reads: signalling the fence the submit waited on already makes every device write available to the
host domain, and `PipelineStageBits` has no `Host` to name anyway - that is 3.12.

`CmdBlitImage` names its boxes as an offset and an extent, the way the copy regions do, except that the
extent may be negative, since a backwards box is how a blit mirrors an axis and corners are what Vulkan
takes. Whether a format can be blitted is per device, per format and per side, so
`PhysicalDevice::SupportsBlit` and `::SupportsLinearFilter` answer it and the command throws instead of
leaving a driver-specific surprise to the validation layer.

`CmdGenerateMips` blits each level into the next. `ImageBarrier::To` was added for it and for
`ReadBackTexture`: both are handed the layout to leave a texture in rather than knowing it, and both were
about to spell the same switch over the presets out. `Texture`'s bitmap constructor takes a `generate_mips`
flag that creates the full mip chain of the extent and adds both transfer usages.

Verified by measurement rather than by the absence of validation messages, which 1.9 proved means nothing.
Every readback destination was wiped first, so nothing could be shown green by what the previous pass left:

- Buffer to buffer: 1 of 256 bytes matching before the copy, 256 of 256 after.
- Buffer to image and back, on mip 0 and on mip 1 - the levels the `Bitmap` path never exercised: 256 of 256
  and 64 of 64.
- Image to image: 256 of 256.
- Blit of four distinct texels from 2x2 into 4x4 with a nearest filter, so the expected result is exact:
  64 of 64, and 64 of 64 again with the source box run backwards on X, which mirrors it.
- Mip generation from a mip 0 whose every texel is the same value, since a box filter of a constant is that
  constant at every level whatever filtering the driver picked: 8x8, 4x4, 2x2 and 1x1 all exact.

All eleven guards threw. The sample was run with the validation layer and its albedo texture switched to the
`generate_mips` path: clean exit, no validation message. Probes removed.

Found along the way and fixed in its own commit: `Texture` created an image view unconditionally, which is
invalid for a texture whose usage is transfer only - a thing that could not exist before copies and blits
made it ordinary.

Left for later: compressed formats are not measured by `GetPixelSize`, so the buffer-size check of a
buffer-to-image copy skips them and the validation layer covers what the guard cannot. `vkCmdResolveImage`
belongs with the multisample state of 3.10, and `vkCmdClearColorImage` has no consumer yet.

### 3.5 Richer submission — DONE

`DeviceQueue::Submit(const SubmitDesc&)` takes any number of command buffers, any number of semaphores on
either side, and a fence that may be absent. `SemaphoreSubmit` pairs a semaphore with the stages it is tied
to - the stages that wait for it on one side, the stages that must finish before it is signalled on the
other - which is what lets the work either side does not depend on run ahead. `WaitIdle()` wraps
`vkQueueWaitIdle` for shutdown and setup, where a fence per submit buys nothing.

Written on `vkQueueSubmit2` rather than `vkQueueSubmit`. Synchronization2 is already enabled, the barriers
already speak it, and `VkSemaphoreSubmitInfo` carries the value field that timeline semaphores need, so the
follow-up is a field rather than a rewrite. It also removes a question the old code left open: it cast
`PipelineStageBits`, whose values mirror the 64 bit synchronization2 stages, to the 32 bit
`VkPipelineStageFlags` of the original submit. That happened to be correct, since the stages both versions
have kept their values, but it is not a cast anyone should have to check.

`Submit(command_buffer, fence)` remains, written on the desc based one; `ImmediateSubmit` in `transfer.hpp`
is what calls it. The overload that took a wait and a signal is gone. 4.1 was its last caller and moved off
it - two signal semaphores are more than it can express - and it was the only submit path that quietly
dropped an empty semaphore instead of throwing, so keeping it would have meant two rules for the same
mistake depending on which overload the caller reached for.

Verified by a test that copies one half of a buffer in one command buffer and the other half in another,
then reads the whole thing back, run three ways: both command buffers in one batch with a fence, the same
batch with no fence and `WaitIdle` instead, and one batch each with the second waiting on a semaphore the
first signals. All three produce the same bytes, and a batch given an empty command buffer or an empty
semaphore throws.

That test also turned up a flaw in the naming test of 3.8, which had been submitting a deliberately invalid
copy to provoke a message: work that breaks the specification is undefined behaviour, and the driver took
the next test down with it. The validation layer checks a copy while it is recorded, so the command buffer
is thrown away instead of submitted, which is both correct and faster.

`Semaphore` now carries a type. `SemaphoreDesc` picks between binary and timeline and, for a timeline, the
count it starts at; `SemaphoreSubmit` carries the value the submit waits for or signals. The host side is
`Wait`, `Signal`, `GetValue` and a batched `WaitForAll`, all four of which turn away a binary semaphore,
since only a device wait can consume one of those.

The mistakes the API can catch, it catches. A value on a binary semaphore throws: Vulkan ignores the field
there rather than complaining, which makes it exactly the kind of error that would otherwise surface as a
missing dependency several frames later. A timeline signal of zero throws too, since a signal has to leave
the count above where it was and nothing is above zero - a timeline wait for zero is legal and trivially
satisfied, so only the signal side is turned away. `Signal` from the host makes the same check against the
count the semaphore actually holds, which is what its doc comment had been promising and not doing; a device
signal landing between that read and the call can still slip past, but the mistake worth catching is the
host signalling backwards. Acquire and present reject a timeline outright; neither `vkAcquireNextImageKHR`
nor `VkPresentInfoKHR` can express one.

Both `WaitForAll` calls turn away a list that spans two devices. One wait is one call into one device, and
the call can only name the first entry's - the rest were being read off it and assumed to agree.

`timelineSemaphore` is not a `DeviceFeatures` field any more. It is required of every Vulkan 1.2
implementation and Forge asks for 1.3, so an opt-in flag was guarding against a device that cannot exist;
it is now enabled and required beside `synchronization2` and `dynamicRendering`.

Found along the way and fixed here: `Semaphore::operator=(Semaphore&&)` did not destroy the handle it held
before taking the other one, unlike `Fence::operator=` immediately above it, so assigning over a live
semaphore leaked it. 4.1's frame timeline is built by exactly that assignment.

A blocking wait and a wait with a deadline are two calls rather than one with a defaulted timeout, on the
fence and the semaphore alike: `Wait` blocks and `TryWait` gives up, answering false when the timeout ran
out. They were one call, and it did not work - `vkWaitForFences` and `vkWaitSemaphores` return `VK_TIMEOUT`
as a *success* code, which the single wait threw on, so the timeout could not be told from a lost device
without unpacking the exception. Nothing had ever passed a timeout, which is how it went unnoticed. `TryWait`
is `[[nodiscard]]`, since ignoring the answer is the whole mistake; `Wait` stays void and every existing
caller of it is untouched. `k_infinite_wait` moved to namespace scope with it, since it is no longer a
default argument on `Fence` and the semaphore was reaching across for it.

Verified by a test that reuses the split copy above: a fresh timeline reports its initial value and the host
raises it, a wait for a value already reached returns at once, two batches ordered by a timeline produce the
same bytes as one, the host waits on a value the device signals with no fence anywhere in the submit, and
`WaitForAll` covers two of them. A `TryWait` for a value nothing will ever signal answers false, on a
semaphore and on a fence, rather than throwing or hanging. The throwing cases are covered too, except the
two swap chain rejections - those need a surface and a window, which the headless tests do not have.

### 3.6 Device features — DONE

`DeviceDesc::features` is a `DeviceFeatures`, a flat struct of named booleans, rather than the Vulkan 1.0
feature structure it used to be. The fields say what they do - `fill_mode_non_solid`, `multi_draw_indirect`,
`buffer_device_address`, `mesh_shader` - and Forge maps each onto whichever structure Vulkan keeps it in,
chains those, and passes the chain. The chain lives in one local in `Device`'s constructor and is read
before that call returns, so the lifetime problem a caller-owned `pNext` chain would have created does not
exist. Nothing in the desc names a `Vk` type any more, which is what 2.4 asked for and this was the last
exception to.

Grouping by Vulkan version was deliberately dropped. A caller has no reason to know that buffer device
addresses arrived in 1.2 and dynamic rendering in 1.3, and that grouping ages badly as features are promoted
between releases.

What the caller cannot turn off is `synchronization2` and `dynamicRendering`, which Forge is written on -
every barrier and every `CmdBeginRendering` - so both are always enabled, and a device without them is
rejected by name at creation.

Support is checked against `vkGetPhysicalDeviceFeatures2` before anything is requested, and an unsupported
feature throws naming the field rather than coming back as `VK_ERROR_FEATURE_NOT_PRESENT` from
`vkCreateDevice`, which names nothing. Asking for `mesh_shader` or `task_shader` enables `VK_EXT_mesh_shader`
too, since a feature that lives in an extension is useless without it - that is what unblocks
`CmdDrawMeshTasks`, which has existed since 3.3 and had no way of ever being enabled.

`Device::GetFeatures()` reports what was asked for, and three guards now use it: a buffer wanting a device
address, an anisotropic sampler, and an indirect draw of more than one command all throw when the feature
behind them was not enabled. Writing the first of those turned up that the check has to come before
`vmaCreateBuffer` rather than after - the allocation asks for device address memory itself, so a guard that
threw afterwards had already produced a validation error.

Verified headless: the defaults come back out of `GetFeatures`, asking for mesh shaders succeeds exactly
when this machine has the extension, and each of the three guards throws on a device created without its
feature. The sample runs unchanged, since the defaults are what was hardcoded before.

### 3.7 Physical device selection — DONE

`FindPhysicalDevice` returns the index of the device best suited to a `DeviceDesc`, or nothing when none of
them can be created with it. `SelectPhysicalDevice` is the same thing moved out of the list, throwing when
nothing qualifies and naming what the last device was missing.

The requirements are the desc rather than a second structure beside it. The desc already says which surface
has to be presented to, which extensions and features are needed and which queues are wanted, so a separate
description of the same thing would only be one more pair to keep in step. The checks are shared with
`Device`'s constructor for the same reason - `CollectDeviceExtensions`, `FindUnsupportedFeature` and the
queue family masks are each written once - so a device that passes the choice cannot then fail the creation.

Ranking is the kind of device first, discrete over integrated over virtual over software, then device local
memory as the tiebreak, counted in gigabytes so that a slightly larger integrated device cannot outrank a
discrete one. `prefer_discrete` turns the first half off, and on a machine with one device none of it
matters.

Writing the test for it turned up that `PhysicalDevice::IsExtensionSupported` returned `true` for every
extension: it built an `Opal::Exception` where it meant to throw one and returned `true` regardless. Nothing
had ever rejected an unsupported extension, including the check in `Device`'s constructor. Fixed in its own
commit.

Verified headless: some device on this machine meets a headless desc and can then actually be created, a
desc naming an extension that does not exist is met by nothing, selecting when nothing qualifies throws, and
selecting moves exactly one device out of the list. The sample picks its device this way now instead of
taking element 0.

### 3.8 Debug tooling — DONE

`GraphicsContext` keeps what it is told instead of only logging it. `GetDebugMessages` hands back the
warnings and errors, `GetDebugMessageCount` counts them by severity and by type, and `ClearDebugMessages`
starts a fresh stretch. That last distinction is what makes the counts usable: this machine has broken layer
manifests left behind by two other applications, which the loader reports at error severity, so a test that
asked for "errors" would fail on somebody else's installer. Asking for `DebugMessageTypeBits::Validation`
asks about this code.

The log lives in a `SharedPtr` beside the context rather than inside it, because the callback is handed a
pointer to it when the messenger is created and there is no way to update that pointer afterwards - a moved
context would leave the callback writing into the object it was moved out of. Info messages are counted and
not stored, since the loader emits thousands and none of them says anything went wrong, and the stored
warnings and errors are capped by `GraphicsContextDesc::max_stored_debug_messages`.

The headless tests of 3.11 now end by asserting that the validation layer reported nothing, and print the
text of what it did report when they fail. Verified the other way round as well, which is the half that
matters: a probe copying between two formats of different texel size made the assertion fail with the
message quoted, so the tests can tell a clean run from a quiet one.

`SetDebugName` in `rndr/forge/debug.hpp` names every kind of object, and messages carry
`DebugMessage::objects`, the names of what the message is about - the validation layer hands those over
beside the text rather than inside it, so collecting them is what makes a message about "suzanne albedo"
read as one. There is no `debug_name` on the descs, which is what this task originally asked for: the types
with a desc are a subset of the types with a handle, so one shape that names all of them beat a field on
most of them. Naming is a no-op when the instance has no `VK_EXT_debug_utils`, which `Device` carries over
from the context, since the loader hands out a callable pointer for the command either way - the same trap
`CmdDrawMeshTasks` hit in 3.3.

Verified by naming two textures, breaking a rule about them on purpose, and requiring both names to come
back out of the reported message. The sample names its mesh, textures, samplers, layout, set, pipeline, swap
chain, per-frame buffers, command buffers and fences.

`CommandBuffer::CmdBeginDebugLabel`, `CmdEndDebugLabel` and `CmdInsertDebugLabel` mark a region of the
command stream, and `ScopedDebugLabel` in `rndr/forge/debug.hpp` is the pair as a scope, so an early return
or a throw cannot leave one open. They are `Cmd*` methods rather than free functions beside `SetDebugName`
because a label is recorded into a command buffer and a name is not. All three ask the same question before
calling through, which is what keeps a build without `VK_EXT_debug_utils` skipping both halves of a region
rather than one of them. The colour is a hint and nothing else - Vulkan gives it no meaning and the layer
never reads it - and it is kept because RenderDoc tints the row with it, which is how two passes are told
apart at a glance.

`TimestampQueryPool` in `rndr/forge/query.hpp` is the timing half: a pool of timestamps rather than one
object per timestamp, which is the shape Vulkan actually has and lets one pool cover a whole frame.
`CmdWriteTimestamp` writes a tick, `CmdResetQueryPool` and `TimestampQueryPool::Reset` put the pool back
into the state a write needs, and the elapsed helpers come in a blocking pair and a `TryGet*` pair. Ticks
are masked to `timestampValidBits` and scaled by `timestampPeriod` inside, and a family that writes no valid
bits throws at creation rather than handing back zeroes that read like a fast device.

Two things the shape had to get right. The elapsed helpers read the two queries one at a time rather than as
the range that spans them: `vkGetQueryPoolResults` reports a whole range unavailable when any query in it
is, so measuring one operation out of several - which leaves the queries between the pair unwritten - would
have reported "not ready" forever, and a measurement that never arrives is indistinguishable from a device
that is behind. And the stage a timestamp names is what decides what a pair of them means, which
`docs/forge.md` now spells out: `PipelineStart` then `PipelineEnd` is a span of the queue, `PipelineEnd` on
both sides is one operation with the pipeline drained around it, and bracketing a single draw the first way
does not give that draw's cost.

Verified by timing a dispatch against the ticks and the period the device reports, by a pair at the ends of
a four query pool with the middle unwritten, and by the misuse cases - a query past the end, a stage with
more than one bit, a pool of no queries, and `Reset` without `host_query_reset`. The negative half came for
free: the first version of the test read a pool that had never been reset, and the layer said so, which is
also the bug it then found in the sample.

The sample keeps one pool per frame in flight, reads the result of the frame from two frames ago right after
`BeginFrame` - where the fence of that slot has already been waited on, so nothing stalls - and shows CPU
and GPU milliseconds in the window title. Its forward pass is one labelled region. Run across four resizes
and a minimize with the layer on: the GPU figure tracks the window size and no validation message is
reported.

Left out on purpose: `vkQueueBeginDebugUtilsLabelEXT` for regions around a submit rather than inside one,
which has no consumer, and pipeline statistics and occlusion queries, which are a different query type and
belong to a different task.

### 3.9 Bindless plumbing - DONE

`DescriptorSetLayoutDesc::Binding::flags` is a `DescriptorBindingFlagBits` mask - `UpdateAfterBind`,
`UpdateUnusedWhilePending`, `PartiallyBound`, `VariableDescriptorCount` - whose values mirror the Vulkan
ones, so the per-binding array is filled by a cast. The array was already being built and passed; it was
being passed all zeroes, next to a local holding the three flags that nothing read. Both existed since the
layout was written, which is why `use_update_after_bind` and `variable_descriptor_count` had never done
anything.

`VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT` was also set on every layout ever created. It
is now set only when a binding asks for it, and `DescriptorSet` checks that such a layout is allocated from
a pool created with `use_update_after_bind`, which is the first place that can see both.

Each flag is checked against the device feature behind it, which 3.6 made requestable, so a binding asking
for something the device was not created with says so instead of reaching the validation layer. A variable
count is only allowed on the binding with the highest index - not the last one in the desc, since 2.6 let
them arrive in any order - and the count given at allocation has to fit inside the `descriptor_count` the
binding declared.

Writing the test turned up the other half of what bindless needs and what the task did not name:
`DescriptorSetUpdateBinding` had no array element, so `dstArrayElement` was always zero and Forge could only
ever write the first descriptor of an array binding. There is an `array_element` field now.

Verified end to end rather than by creation succeeding: a layout with all three flags on a four descriptor
storage buffer array, a set allocated with a variable count of two, only descriptor one ever written, and a
compute shader indexing `outputs[1]` writing `index + 2000` into it. All 256 values came back, which needs
the variable count, the partially bound flag and the array element all to have taken effect. Four guards
throw: a count above the binding's, a count without a binding that allows one, a variable count on a binding
that is not the highest, and an update after bind layout allocated from a pool that does not expect one.

Textures were plumbed the whole time and unproven the whole time. The feature mapping names the sampled
image bits specifically - `descriptorBindingSampledImageUpdateAfterBind` and
`shaderSampledImageArrayNonUniformIndexing`, not only the storage buffer ones - but every
`CombinedImageSampler` binding in the repo held a single descriptor, so nothing had ever written past
element zero of an image binding or indexed one from a shader. `"Forge bindless texture array"` closes that:
a four descriptor `Sampler2D textures[]` binding allocated with a variable count of three, elements one and
two written and element zero deliberately left alone, and a compute shader sampling
`textures[NonUniformResourceIndex(1 + (thread_id.x & 1))]` so the index differs between neighbouring
invocations. Two single texel textures with different red channels, 256 invocations, and the value each one
reports says which descriptor it reached.

It was checked against being vacuous rather than trusted for passing. Making the index the constant `1`
fails at invocation 1, the first one that should have read the other element; swapping which texture is
written to which element fails at invocation 0. So the per-invocation index and the array element are both
load bearing.

**What it does not prove is the non-uniform part**, which is worth writing down because the name says
otherwise. Removing `shaderSampledImageArrayNonUniformIndexing` from the feature mapping and rebuilding
leaves the case passing and the validation layer silent, so whatever
`textures[NonUniformResourceIndex(...)]` compiles to here does not demand that bit. The index differing
between invocations is real; the decoration Vulkan gates on that feature is not visible from out here.
Proving it needs the SPIR-V disassembled rather than another runtime case.

Two gaps it turned up, both fixed. `shaderUniformBufferArrayNonUniformIndexing` was the one indexing bit
`DeviceFeatures::non_uniform_descriptor_indexing` did not map to, while sampled image, storage buffer and
storage image all were. `FindUnsupportedFeature` now checks every bit each of the two descriptor indexing
flags turns on rather than only the first: `vkCreateDevice` fails on any one of them the device lacks, and
naming the descriptor kind keeps a device supporting most of a flag from reporting the whole flag missing.
`"Forge bindless constant buffer array"` covers the array and the per-element writes into it, and carries
the same caveat as above - it passes with the bit off.

`DescriptorSet::Update` also passed `array_element` through to `dstArrayElement` unchecked, and Vulkan
reports nothing for an element past the end of a binding. `BindingInfo` keeps the descriptor count now,
taking the smaller allocated number on the binding that was given a variable count, so an element inside the
declared array but outside the allocated one throws as well.

### 3.10 Pipeline gaps — DONE, minus a cache that was measured and dropped

Three of the five were a hardcoded value becoming a field.

`ColorBlendDesc::color_write_mask` is a `ColorWriteMaskBits`, every channel by default, and the mask is
applied after the blend rather than instead of it. `GraphicsPipelineDesc::sample_count` is a `SampleCount`,
checked against `framebufferColorSampleCounts & framebufferDepthSampleCounts` - both, since a pipeline
renders into both and a count one of them cannot carry is as unusable as one neither can - and throwing
rather than leaving it to the validation layer. `GraphicsPipelineDesc::dynamic_state` is a
`DynamicStateBits` mask adding depth bias, stencil reference and line width to the viewport and scissor
every pipeline already declares, with `CmdSetDepthBias`, `CmdSetStencilReference` and `CmdSetLineWidth` to
supply them. The last two guard `DeviceFeatures::depth_bias_clamp` and `::wide_lines` the way
`CmdDrawMeshTasks` guards its extension.

`StencilFaceBits` is new because `Rndr::Face` could not do the job: it is a cull mode, where `None` means
"cull nothing" and there is no way to name both faces at once, which is the case a stencil command wants
most.

This is what finally put a graphics pipeline in the headless tests. A colour write mask cannot be checked
by anything short of a real draw, so there is now a triangle covering the target, a vertex and fragment
shader, and a readback: unmasked writes every channel, green-only leaves the red and alpha the clear set,
and masking everything out leaves the attachment exactly as cleared. Verified the other way round as well -
with the mask hardcoded back to all four channels two of those three fail. The positions come from a vertex
buffer rather than `SV_VertexID`, which maps to `gl_VertexIndex` and pulls in a SPIR-V capability that
would have needed a device feature turned on to draw a triangle.

**The pipeline cache is deliberately not done.** Measured before writing any of it, on the sample, three
runs of a debug build: compiling its two Slang entry points takes 4.9 seconds, and building the graphics
pipeline from the SPIR-V that comes out takes 3 milliseconds. A `VkPipelineCache` would save 0.06% of
startup. Slang is a prebuilt dependency, so a release build moves the ratio very little.

A pipeline cache earns its keep against hundreds of pipelines or a shader variant explosion, and Forge has
one graphics pipeline and one compute pipeline. Reach for it when there is something to amortise; the five
seconds are in 3.13 instead. This paragraph is here so nobody adds it back thinking it was an oversight.

Specialization constants close it out. `GraphicsPipelineDesc::specialization` and the same field on
`ComputePipelineDesc` name constants and give them values; `pSpecializationInfo` is built per stage and
pointed at from each `VkPipelineShaderStageCreateInfo`, which both were leaving null.

Keyed by **name** rather than by the numeric id Vulkan wants, and that is the point of the task rather than a
convenience. The specification says a `constantID` matching no constant "does not affect the behavior of the
pipeline" - a mistyped number does nothing, silently, and the pipeline renders with the default while the
caller believes otherwise. Names come from reflection, so a name no stage declares throws. A type that does
not match the declared one throws too, with no coercion, since a value reinterpreted at the wrong width is
not something a caller could notice from the outside. One name given a value twice throws as well: it would
pack two map entries sharing a `constantID`, which no single `VkSpecializationInfo` may hold.

`Shader::GetSpecializationConstants` reports name, id, type, default and byte size, read in the window the
constructor already has a `SpvReflectShaderModule` open for - nothing is reflected twice and no reflection
state outlives the constructor, which a scope guard now sees to whichever way the constructor leaves, since
several of the steps reading from the module throw. The default comes back as raw bytes whose meaning depends
on the type, four for anything 32 bit or smaller and eight for the rest, low-order word first, so it is
copied rather than reinterpreted. `SpecializationValue` is a tag and a bit pattern rather than an
`Opal::Variant`, which is move-only and would have made a list of them awkward to write inline in a desc; the
raw bytes are what Vulkan wants anyway. Its pointer constructor is deleted, or `{"MAX_LIGHTS", "8"}` would
reach the `bool` one and mean true.

Byte size is reported apart from the type because the two part company below 32 bits.
`VkSpecializationMapEntry::size` has to be the byte size of the declared type - one byte for an `int8_t`
constant - while a bool is four whatever SPIR-V calls it. Narrow integers are still reported as `Int32` or
`UInt32` so a caller writes a plain integer for them, and the map entry carries the declared width instead;
a value too wide for that width throws rather than being truncated on the way into the blob.

The stages are gathered before anything points into a list. `pMapEntries` and `pData` have to stay put until
`vkCreateGraphicsPipelines` returns, and the first attempt reserved capacity and pushed - the same shape as
the bug 1.9 fixed in the descriptor writes. Both arrays are built at their final size and written by index
instead.

Slang was probed before any of this was designed around it: `[SpecializationConstant]` and
`[vk::constant_id(N)]` both compile and reflect identically, name, id, type and default all correct. The
first is what the tests use, since it lets the compiler assign ids.

Verified by building two pipelines from one pair of `Shader` objects with different values and reading back
two different colours, which is the only thing that shows one module producing two behaviours. Also the
default when nothing is supplied, the reflection report, a compute pipeline through its own path, an unknown
name, and a mismatched type. Checked the other way round as well: with `pSpecializationInfo` back to null
both pipelines return the shader default and the two-pipeline case fails with the two colours identical.

### 3.11 Tests — DONE

`test/forge/smoke-test.cpp`, tag `[forge]`, in `rndr-test` behind the `RNDR_FORGE` option. No window, no
surface, no swap chain: `Device` never needed one, so the whole file runs on a machine with nothing but a
Vulkan driver. A machine without one skips rather than fails, decided once by trying to build a context and
a device.

Seven cases, each ending in a readback compared against a value computed on the CPU, with every destination
wiped first:

- context, physical device enumeration, device and queue;
- buffer update at an offset and read back, including that the bytes before the offset are untouched, which
  is what makes it a test of the offset rather than of the write (1.3, and 1.5 with it);
- a buffer moved twice and then read, which fails if the mapped pointer did not come along (1.4);
- the compute dispatch the task asked for: a Slang shader compiled from a string in the test file writes
  `index + 1000` into a buffer named by its device address, over four groups of 64;
- buffer copy and readback through `CmdCopyBuffer` and `ReadBackBuffer`;
- a `HostAccess::None` buffer filled and read through the staging helpers;
- texture upload, mip generation and readback of every level.

It found three bugs on its first run, each fixed in its own commit:

- `Device`'s move carried neither the VMA allocator nor the enabled extension list, so a moved device
  allocated nothing and the moved-from one destroyed the allocator on the way out. `DeviceQueue`'s move
  carried neither its device nor its family index and did not destroy the pool it already held, so a moved
  queue released its command pool through a device it no longer had - `vkDestroyCommandPool: Invalid device`.
  And every queue holds a reference back to its device, which a move has to re-point.
- `Buffer` and `Texture` leaked everything they had already created when a later step of the constructor
  threw, since the destructor does not run for an object whose constructor did.
- `BufferDesc` asked VMA for `HOST_ACCESS_ALLOW_TRANSFER_INSTEAD` on every buffer while
  `keep_memory_mapped` defaulted to true, which are contradictory: the flag permits memory the host cannot
  map, and the default then failed to map it. A buffer created with nothing but `StorageBuffer` usage threw
  from its constructor. Host access now means the memory is mappable, and `HostAccess::None` is how a caller
  asks for device-local memory, with `Update` and `Read` throwing on one and pointing at the staging helpers.

Left for later: nothing asserts on validation messages. `GraphicsContextDesc::collect_debug_messages` only
logs them, so a test cannot fail on one; making it collectable is worth doing with 3.8, which adds the rest
of the debug tooling. The graphics pipeline, the swap chain and the barrier presets are still only covered
by running the sample.

### 3.12 Complete the barrier vocabulary - DONE

`PipelineStageBits` names every stage a barrier here can need: the tessellation pair, geometry, task and
mesh, `Host`, `AllGraphics`, `PreRasterizationShaders`, the halves of vertex input, and the four the
transfer stage splits into - `Copy`, `Blit`, `Resolve`, `Clear`. `Transfer` stays as the one that covers all
four, for a barrier that does not care which.

**The access model: both, with the coarse pair as the default and the narrow bits used by Forge itself.**
`Read` and `Write` stay exactly what they were, `MEMORY_READ` and `MEMORY_WRITE`, because they are correct
beside every stage and an access that does not match its stage is invalid - a caller writing a barrier by
hand should be able to reach for a pair that cannot be wrong. The specific bits are there beside them, and
every barrier preset now uses them, because a preset knows precisely what the texture is about to be used
for: `ToColorAttachment` says colour attachment write rather than any write. That is the narrowing
synchronization2 exists for, spent where Forge already has the information and not demanded of callers who
do not.

Ownership transfer is `source_queue_family` and `destination_queue_family` on both resource barriers, both
`k_ignored_queue_family` by default, which is what 1.8 pinned them to and is still what a resource used on
one queue wants. `Barriers::flags` carries `DependencyFlagBits`, so a dependency can be by-region.

The task and mesh stages belong to an extension, so `CmdBarriers` checks the device enabled it before
naming one - the same trap `CmdDrawMeshTasks` fell into in 3.3, where the loader hands out a callable
pointer either way.

The per-call allocation is gone: `BarrierBatch` keeps eight of each kind on the stack and falls back to the
heap above that. The scratch allocator was not the answer, for the reason this task already recorded and
`Fence::WaitForAll` then demonstrated - asking Opal for one asserts unless the application pushed it.

Verified headless on a real copy: the narrow stages and access ordering a transfer against a host read, a
batch of sixteen barriers past the in-place eight, a release-and-acquire pair naming a queue family on both
sides, and a barrier naming the mesh stage on a device without the extension throwing.

---

### 3.13 Cache the compiled SPIR-V — DONE

Measured before writing any of it, the way 3.10 did. Debug build, three runs of the sample, timing spans
inside `LoadModule` and `CompileEntryPoint`:

| Span | Per run |
|---|---|
| `slang::createGlobalSession` | 1.30 s (2 x 650 ms) |
| `createSession` + `loadModuleFromSourceString` | 1.40 s |
| entry point codegen | 1.03 s |

The measurement moved the task. **`createGlobalSession` was running once per compile** - it loads Slang's
core module, it is the same session whatever is being compiled, and the sample was paying for two of them.
There is now one per process behind a function-local static. That alone took the `[forge]` suite from
**172.7 s to 63.7 s**, because the suite compiles 78 times and every one of them was building a core module
first. Nothing about the compiler's output changes; it is the same Slang given the same input.

Then the cache. `ShaderCache` has two tiers: in memory for as long as the object lives, and on disk when it
was given a directory. The memory tier is not an optimisation of the disk one - Catch2 re-runs a `TEST_CASE`
body once per `SECTION`, so a case with nine sections was compiling its shaders nine times inside one
process, which no disk cache would have helped as much as it should.

Reached through `Forge::ShaderDesc::cache`, so no call site that does not want one changed. The cache
belongs to the application: it has to outlive the shaders that fill it, and a `Device` handing one out
would be Forge deciding where somebody else's files go.

| | `[forge]` suite | Sample startup |
|---|---|---|
| Before | 172.7 s | ~3.7 s of Slang |
| Shared global session | 63.7 s | ~2.3 s |
| Plus the cache, cold | 45.5 s | unchanged, it is a cold run |
| Plus the cache, warm | 39.0 s | **no Slang session created at all** |

Shader compilation has stopped being what the suite spends its time on; the 39 s left is device creation per
fixture. A warm run of the sample never loads Slang, which is worth spelling out: the build tag in the key
comes from `spGetBuildTagString`, a **free function**, so nothing about a hit needs a session. Had it only
been available on `IGlobalSession`, every hit would still have paid the 650 ms.

**Keying is the whole task and the failure is silent.** The key holds the source text *whole*, the entry
point, the output format and the build tag, and a lookup compares all four. The hash only names the file: a
collision costs a recompile rather than handing back a shader that is not the source being read. Everything
wrong with a blob - absent, unparseable, truncated, written by another Slang - is one path out, and the
caller compiles. `Find` never throws.

One premise of this task turned out to be stale. It warned that `File::ReadShader` expands includes, so
hashing the top file misses an edit to an included one. `File::ReadShader` has **no callers**;
`Shader::FromSource` reads the file with `ReadEntireTextFile`, `LoadModule` passes no `searchPaths`, and no
shader in the repo has an `import`. The string handed to Slang is the whole input, which is what makes the
key exact - and what would stop being true the moment search paths are configured. There is a comment at
the `SessionDesc` saying so.

Verified by the negative tests, since a test that only checks a hit is fast proves nothing: an edited source
gives different SPIR-V, two entry points of one source give different SPIR-V, a blob overwritten with
garbage reads as a miss and is replaced by a working one, a key carrying a different build tag misses a file
that is otherwise its own, a fresh cache over the same directory reads back what the last one wrote, and a
cache with no directory still answers within the process. The sample renders with the validation layer on
and exits clean twice in a row, the second run being the one that proves the disk tier.

Not to be confused with a pipeline cache, which 3.10 measured and dropped.

---

## Priority 4 — Convenience

### 4.1 Frame context — DONE

`Forge::FrameContext` owns the frames in flight: a command buffer and an image-ready semaphore for each, a
render-finished semaphore per swap chain image, one timeline semaphore counting the frames off, and the
order acquire, submit and present have to happen in. `BeginFrame` waits for the slot this frame reuses,
acquires an image and hands back a command buffer that is already recording; `EndFrame` transitions the
image to Present, closes the command buffer, submits it against the right semaphores and presents.

Both return `SwapChainStatus`, so a resized or minimized window stays what it already was in this API - an
outcome rather than a failure. `BeginFrame` returning `OutOfDate` means the swap chain was rebuilt, nothing
was recorded and the counter has not advanced, so the caller just continues its loop. The render-finished
semaphores are rebuilt with the swap chain, which was the part of the sample most easily left out.

`EndFrame` takes the layout the caller left the image in, defaulting to `ColorAttachment`, and makes the
transition to `Present` itself. Passing `Present` skips it, for a frame that already did it by hand.

The sample went from 339 lines to 292, and the part this replaced from 61 lines to 14. What is left in its
loop is what the application actually decides: the barriers into its attachments, the rendering desc, the
draw calls. `SetDebugName` has an overload for the whole context, which names the frame timeline and gives
every semaphore and command buffer the index of the frame or image it belongs to.

Verified by driving the sample through four resizes, a minimize and a restore with the validation layer on:
six swap chain rebuilds, no validation message, clean exit. The headless tests cannot reach this - a frame
context needs a swap chain and a swap chain needs a window - so the sample is still what covers it.

The fence per frame in flight is gone; one timeline semaphore replaced the whole array. It starts at
`frames_in_flight`, frame *k* signals `k + 1 + frames_in_flight` on submit and `BeginFrame` for frame *k*
waits for `k + 1` - which is what the frame whose slot it is about to reuse signalled. The first
`frames_in_flight` frames wait for a value at or below the initial one, so they never block, and no branch
or saturating subtraction is needed to say so. `GetFrameIndex` still means what it meant, so nothing outside
moved: it is the counter modulo the frames in flight rather than a field of its own.

Two things fell out of it beyond one fewer object per slot. There is no reset, so the ordering subtlety that
`fence.Reset()` had to happen after the out-of-date early return rather than before it does not exist any
more. And the fence version could deadlock: an `EndFrame` that threw between `BeginFrame`'s reset and its
submit - the "gave up its acquired image" check, or `command_buffer.End()` - left that slot's fence
unsignalled with nothing left to signal it, and the next `BeginFrame` on that slot waited forever. A frame
that dies between begin and submit now costs a frame rather than the process.

Verified the same way as the rest of this task: four resizes, a minimize and a restore with the validation
layer on, six swap chain rebuilds, no validation message, clean exit. That protocol is what catches both
ways the counter can be wrong - an off-by-one in the wait value lets the host run further ahead than
`frames_in_flight` and the layer reports a command buffer rewritten while still in flight, and a skipped
frame that advanced the counter would hang on a wait that never returns.

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

### 4.3 Track the current layout on the texture — DONE

`Texture` keeps one `ImageLayout` per (mip level, array layer) and `CommandBuffer::CmdBarriers` is the only
thing that writes it, over the range each image barrier covers. Every `ImageBarrier` preset gained a form
that reads it, `CmdTransition(texture, layout)` is the transition in one line, and the layout arguments of
`CmdCopyImage`, `CmdBlitImage`, `CmdCopyBufferToImage`, `CmdCopyImageToBuffer`, `CmdGenerateMips`,
`ReadBackTexture` and `FrameContext::EndFrame` are gone.

Per subresource rather than per texture because the scope forced it: `CmdGenerateMips` blits level *n - 1*
into level *n* of the same texture, so one call has that texture in `TransferSource` and
`TransferDestination` at once, and a single layout cannot express it. `GetCurrentLayout()` throws while the
levels disagree rather than picking one.

The `Undefined` defaults on `ToColorAttachment`, `ToDepthStencilAttachment` and `ToTransferDestination` went
with it. They were legal Vulkan that discarded the contents of the texture and said nothing, and the
validation layer has no way to notice; a discard is now something a call site spells out. Holding the layout
also made room for a check the old API could not make - a transfer whose source is not `TransferSource` or
`General`, or whose destination is not `TransferDestination` or `General`, throws instead of being undefined
behaviour the layer sometimes catches.

Tracking is record-time, which `docs/forge.md` now says out loud: it is correct while one thread records in
execution order, it does not survive interleaved recording of two command buffers over one texture, and
`CommandBuffer::Reset()` does not roll it back. Swap chain images are the case the tracker cannot observe,
so `AcquireImage` resets the image it hands out to `Undefined`.

`ImageBarrier::To` is the one preset with no short form, and deliberately. Its long form is
`To(texture, old_layout, new_layout)`, so a short `To(texture, new_layout)` would put the destination where
the source sits in the other, and dropping an argument would compile into the opposite of what was meant.
`CmdTransition` is that short form instead - the same preset over the tracked layout, recorded.

Two things fell out of holding the layout that were not what the task was for. `CmdGenerateMips` blitted
array layer zero only: `ImageBlitRegion` names one layer unless told otherwise, while the barriers around
the loop covered every layer, so an array texture came back with layers past the first still holding what
they were created with and a tracked layout claiming otherwise. The regions now carry
`desc.array_layer_count`, and `"Forge mip generation covers every array layer"` fails on the first byte of
layer one without it. And the sample's depth barrier used to come out of `Undefined`, whose source scope is
`{PipelineStart, None}` - no execution dependency at all between one frame's depth writes and the next, on
one depth image shared across two frames in flight. It now comes out of `DepthStencilAttachment` and waits
on the fragment test stages, which is a hazard that was there before this task and that nothing would have
reported.

Untested, and unfixably so until a headless surface exists: the acquire-time reset and the non-const swap
chain getters have no `[forge]` coverage, since nothing in the test binary creates a swap chain. The sample
is what exercised them - several resizes plus a minimize and restore, with no validation message.

### 4.4 Shorter descriptor set updates — DONE

`DescriptorSet::Update` has three forms: the array of `DescriptorSetUpdateBinding` that used to be called
`UpdateDescriptorSets`, `Update(binding, texture, sampler)` and `Update(binding, buffer, offset, size)`.
The two textures the sample binds went from thirteen lines to two, and the batch form is still there for a
set with several bindings to fill, since each single-resource call is one `vkUpdateDescriptorSets`.

The overloads do not take a `DescriptorType`, which is what makes them short. The set copies the binding
index and descriptor type of every binding out of the layout when it is allocated, and `Update` looks the
type up from there - so a call site cannot disagree with the shader about what a binding holds, and a
binding the layout never declared throws instead of writing a descriptor nothing reads.
`GetBindingDescriptorType` exposes the same lookup.

Copied rather than referenced on purpose. Vulkan lets a descriptor set layout be destroyed while sets
allocated from it live on, and holding an `Opal::Ref` to the layout would have quietly taken that away -
a lifetime edge the rest of `docs/forge.md` would then have had to document.

`DescriptorSetUpdateBinding::BufferInfo::buffer` and `ImageInfo::sampler` / `::image` became
`Opal::Ref<const T>`, since writing a descriptor only reads the resource and every accessor it uses is
const. That is what lets the new overloads take `const Texture&` and `const Buffer&` the way the parameter
convention of 2.3 asks for.

The copy is state, so it moves with the set. Reviewing this change caught the move assignment leaving it
behind, which is the same bug 1.4 fixed for a mapped pointer: the set would have rejected the bindings its
own layout declared, and only after a move - the pattern every per-frame resource in the sample uses.
`Destroy` clears it too, so an emptied set reports no bindings rather than the ones it used to have.

Verified by a compute dispatch reading a storage buffer bound through `Update(binding, buffer)` and
checking every value it wrote, plus the throwing cases: a range past the end of the buffer, an empty range,
a binding the layout does not have, and a gap between two declared binding indices. The move is covered
both ways round - the test fails with the assignment left as it was. The sample renders and reports no
validation message.

### 4.5 Optional depth attachment — DONE

`RenderingDesc::depth_attachment` is an `Opal::Optional<RenderingAttachmentDesc>`, empty by default, so a
pass that renders without depth says nothing instead of filling in a desc whose null image view meant
"ignore this". `CmdBeginRendering` builds the `VkRenderingAttachmentInfo` only when the optional holds one
and points `pDepthAttachment` at it only then.

The convention it replaced is now an error rather than a second way of saying the same thing: an attachment
that is present and names no image view throws, since that is a desc somebody started filling in and did
not finish. Nothing else could tell those two apart before.

Removing the convention from one side of a boundary and leaving it on the other is what a review of this
caught. `SwapChain::GetDepthImageView()` still hands back a null view for a swap chain built with
`use_depth` off, which used to be exactly how a caller said "no depth" and now throws instead. `HasDepth()`
is the guard that was missing - `IsValid()` on the depth texture, under a name a caller would look for -
and the thrown message names it, since the answer being available is no use if nothing points at it.

Verified headless, which this turned out to be reachable by after all - dynamic rendering needs an image,
not a swap chain. A pass with one colour attachment, no depth and no pipeline at all is cleared by its load
operation, and reading the texture back shows the clear colour in every texel. Only zero and one are used
there, since those are the only channel values a UNORM format converts exactly. The throwing case is
covered too, and the sample still renders with depth and reports no validation message.

### 4.6 Swap chain remembers the acquired image — DONE

`AcquireImage` records the index it hands out, `GetCurrentColorImage()` and `GetCurrentColorImageView()`
hand back that image, and `HasAcquiredImage()` says whether there is one. Asking outside an acquire and
present pair throws rather than returning a stale image.

`Present` lost its index parameter rather than gaining an overload beside it. Presenting an image other
than the one just acquired is not a thing anyone does, so two ways of spelling it would only have been two
ways to get it wrong. It clears the index before calling `vkQueuePresentKHR`, not after: a present that
comes back out of date recreates the swap chain inside that call, and an index cleared afterwards would
have spent that moment pointing into images that no longer exist.

`FrameContext` kept its own `m_image_index`, which was the same fact stored twice. It now reads the swap
chain's, and `GetImageIndex()` forwards. That deleted the field, its two move paths and its reset.

The index is carried across both `SwapChain` moves. Reviewing 4.4 had just turned up a member wired into
the move constructor and not the assignment, so this one was written with that in mind rather than found
the same way twice.

The `i32` cast this task pointed at in the sample was already gone by the time it was reached.

Verified by driving the sample through four resizes plus a minimize and restore with the layer on: six
swap chain rebuilds, no validation message, clean exit. That is the path where the bookkeeping could go
wrong, since an out of date acquire and an out of date present each leave the swap chain holding nothing.

### 4.7 Write `docs/forge.md` — DONE

Four sections beyond what 2.2 and the tasks after it left there:

- **Object lifetimes.** Nothing is reference counted and nothing is deferred, and an object holds a
  reference to what it was created from without keeping it alive, so declaration order is the whole rule.
  The references that are easy to miss are listed - a descriptor set holds its pool, a command buffer holds
  its queue, immutable samplers have to outlive the layout - along with the other half of a lifetime, which
  is waiting for the device before releasing anything it might still be reading.
- **The frame loop.** What `FrameContext` owns and what the application still decides, with the loop written
  out, plus why a resize is a return value rather than an exception and what to do when the window has no
  client area at all.
- **Getting data in and out.** `HostAccess` decides where the memory lives and therefore which of `Update`,
  `Read` and the staging helpers works. Also the part that is easy to get wrong by hand and is handled
  underneath: flushing, invalidating, and the fence that makes a submit's writes visible to the host.
- **Debugging.** Reading the messages back off the context, why the type filter matters more than the
  severity, naming objects, and that a command the layer rejects has to be thrown away rather than
  submitted, since submitting it is undefined behaviour whatever the layer said.

`docs/vulkan.md` is left as what it is, notes on Vulkan itself rather than on this API.

---

### 4.8 Use the rest of the reflection — DONE

`Shader` was opening a `SpvReflectShaderModule` and throwing away everything but the stage and, since 3.10,
the specialization constants. It now keeps the vertex attributes, the descriptor bindings and the push
constant blocks as well, read in the same window, through `spvReflectEnumerateEntryPoint*` rather than the
module wide calls: a Slang file holds several entry points and the module wide view reports their union, so
the vertex shader of the sample would have come back claiming the fragment shader's two textures.

`VertexInputDesc::FromShader` and `PushConstantRangesFromShaders` build what the shader already fixed.
`DescriptorSetLayoutDesc::shaders` deliberately does not build a layout - immutable samplers, bindless flags
and an array sized past what the shader indexes are all things only the caller knows - it checks the
hand-written one and gives each binding the name the shader uses, which is what `DescriptorSet::Update`
takes. The sample lost three `AddAttribute` calls, a hand-counted `sizeof(VkDeviceAddress)` and two binding
indices.

Reflection has the location and format of an attribute but never its offset or stride: those describe the
vertex struct on the CPU side and are nowhere in the SPIR-V. `FromShader` packs tightly in location order
and says so; the sample asserts the stride it computes equals `mesh.vertex_size` rather than trusting it.

`GetFormatNumericClass` went into `pixel-format.hpp` for the format check, since UNORM, SNORM, USCALED,
SSCALED, SRGB and both float kinds all arrive in a shader as floats and only UINT and SINT do not. The
switch was generated from the enum rather than typed, so all 185 formats are covered.

**Two checks were designed in and then removed, which is the finding worth keeping.** An attribute at a
location the shader declares nothing at, and a layout binding no shader reads, both look like exactly the
mistake this task is for - Vulkan accepts them and silently ignores them. Neither can be detected. A shader
input or a descriptor that nothing reads is optimised out of the SPIR-V entirely: a probe compiled a struct
with two members and one use, and reflection reported one input. So a stale attribute and a live one feeding
a member this entry point ignores are the same thing from out here, and the check refuses correct code. The
sample proved it before the test did - its fragment shader declares `metal_roughness_texture` and never
samples it, and the first version of the layout check refused to build. Those bindings keep an empty name
and drop out of the by-name lookup, which is the honest cost.

Verified by a reflection test case and a descriptor test case: what each stage reports, the offsets and
stride `FromShader` computes, ranges merged across stages, and the throwing cases - a location nothing
feeds, a float attribute against a uint input, a range four bytes short, no range at all, a binding of the
wrong kind, a binding whose stages omit the one reading it, a binding the shader reads that the layout
omits, a name no binding carries, and a name asked of a layout that has none. A UNORM attribute feeding a
float input is checked to be accepted, since that is the case a naive format comparison would refuse. The
sample renders for twelve seconds with the validation layer on and exits with no message.

---
