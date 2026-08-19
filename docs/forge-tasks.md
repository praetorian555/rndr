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

### 3.5 Richer submission — DONE apart from timeline semaphores

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

The two old overloads remain and are written on the new one, so nothing calling them changed. The one that
takes a wait and a signal still treats an empty semaphore on either side as "not synchronized there" and
leaves it out of the batch, rather than tripping the checks the desc based submit makes.

Verified by a test that copies one half of a buffer in one command buffer and the other half in another,
then reads the whole thing back, run three ways: both command buffers in one batch with a fence, the same
batch with no fence and `WaitIdle` instead, and one batch each with the second waiting on a semaphore the
first signals. All three produce the same bytes, and a batch given an empty command buffer or an empty
semaphore throws.

That test also turned up a flaw in the naming test of 3.8, which had been submitting a deliberately invalid
copy to provoke a message: work that breaks the specification is undefined behaviour, and the driver took
the next test down with it. The validation layer checks a copy while it is recorded, so the command buffer
is thrown away instead of submitted, which is both correct and faster.

Left: timeline semaphores. `Semaphore` is binary only, and a timeline one needs a type and an initial value
at creation, a value on each wait and signal, and `Wait`, `Signal` and `GetValue` on the host side. Worth
doing with 4.1, which is where the frames-in-flight bookkeeping they simplify actually lives.

### 3.6 Device feature chaining

`Forge::DeviceDesc::features` is the Vulkan 1.0 `VkPhysicalDeviceFeatures`; the 1.2 and 1.3 features are
hardcoded in `src/forge/device.cpp:95-104`. Let the caller request 1.1/1.2/1.3 features and extension
feature structs, and fail with a clear message when the physical device does not support what was asked for.

### 3.7 Physical device selection

`EnumeratePhysicalDevices` returns the raw list and the sample takes element 0. Add a scoring or filtering
helper: prefer discrete, require a set of extensions and features, require present support for a given
surface.

### 3.8 Debug tooling — messages and names DONE, labels and timestamps left

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

Left: `vkCmdBeginDebugUtilsLabelEXT` for regions inside a command buffer, and a timestamp query pool for GPU
timing. Neither has a consumer yet - the sample has no pass structure worth labelling and no timing display
- and `Canvas::TimestampQuery` is worth reading before writing the Forge one.

### 3.9 Bindless plumbing

`Forge::DescriptorPoolDesc::use_update_after_bind` and the `variable_descriptor_count` argument on set
allocation exist, but the layout desc has no per-binding flags, so neither can actually be used. Add
per-binding `VkDescriptorBindingFlags` equivalents (partially bound, update after bind, variable count).

### 3.10 Pipeline gaps

Multisample state, per-attachment color write masks, pipeline cache (serialized to disk), specialization
constants, and dynamic state beyond viewport and scissor (depth bias, stencil reference, line width).

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

### 4.1 Frame context — DONE

`Forge::FrameContext` owns the frames in flight: a fence, a command buffer and an image-ready semaphore for
each, a render-finished semaphore per swap chain image, and the order acquire, submit and present have to
happen in. `BeginFrame` waits for the slot this frame reuses, acquires an image and hands back a command
buffer that is already recording; `EndFrame` transitions the image to Present, closes the command buffer,
submits it against the right semaphores and presents.

Both return `SwapChainStatus`, so a resized or minimized window stays what it already was in this API - an
outcome rather than a failure. `BeginFrame` returning `OutOfDate` means the swap chain was rebuilt, nothing
was recorded, the fence of that slot is untouched and the frame index has not advanced, so the caller just
continues its loop. The render-finished semaphores are rebuilt with the swap chain, which was the part of
the sample most easily left out.

`EndFrame` takes the layout the caller left the image in, defaulting to `ColorAttachment`, and makes the
transition to `Present` itself. Passing `Present` skips it, for a frame that already did it by hand.

The sample went from 339 lines to 292, and the part this replaced from 61 lines to 14. What is left in its
loop is what the application actually decides: the barriers into its attachments, the rendering desc, the
draw calls. `SetDebugName` has an overload for the whole context, which names every fence, semaphore and
command buffer with the index of the frame or image it belongs to.

Verified by driving the sample through four resizes, a minimize and a restore with the validation layer on:
six swap chain rebuilds, no validation message, clean exit. The headless tests cannot reach this - a frame
context needs a swap chain and a swap chain needs a window - so the sample is still what covers it.

Timeline semaphores, which 3.5 left open, are now an internal matter: nothing outside `FrameContext` names
the semaphores it waits on, so replacing the fence array with a timeline is a change to one file rather than
to every application.

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
