# Forge (Vulkan) API

Forge is the lower level of the two rendering APIs in this repository. It is a thin, owning wrapper over
Vulkan: every type holds one Vulkan handle, is move-only, and releases the handle in its destructor.

This document covers the conventions that hold across the whole API: how long objects live and what they
hold on to, what a frame looks like, how data gets to the device and back, and how failures are reported.

---

## The empty state

Every type has a default constructor that produces an *empty* object: it holds no Vulkan handle, owns
nothing, and its destructor does nothing. This exists so the types can live in containers and as members
that are filled in later - `Opal::DynamicArray<Fence>`, the depth texture of a swap chain - not as a
two-phase initialization step. There is no `Create()` to call afterwards; assign a constructed object over
it instead.

`IsValid()` reports whether an object holds a handle. Every type has it, and it always answers the same
question: false for a default-constructed object, false after `Destroy()`, false for the source of a move,
true otherwise.

Calling anything else on an empty object is undefined - the accessors return null handles and the methods
dereference references that are not there. `IsValid()` is the guard, not a Vulkan handle comparison at the
call site.

`Destroy()` is idempotent and is what the destructor calls, so releasing early is always safe.

---

## Object lifetimes

Every object owns its handle and releases it in its destructor. Nothing is reference counted, and there is
no deferred deletion queue: when a `Buffer` goes out of scope, its memory is freed right there.

An object does *not* own what it was created from. It keeps a reference - the device, the pool a descriptor
set came from, the queue a command buffer was allocated on - and that reference does not keep the other
object alive. Everything therefore has to outlive what was created from it. Declaring objects in the order
they are created gives that for free, since C++ destroys them in reverse:

```cpp
GraphicsContext context{...};       // destroyed last
Device device{...};
Buffer buffer{device, ...};         // destroyed first
```

The references that are easy to miss:

- Every resource holds the `Device`, and the device holds the `GraphicsContext` that made its instance.
- `DescriptorSet` holds its `DescriptorPool`, and a set is invalid once the pool is reset or destroyed.
- `CommandBuffer` holds the `DeviceQueue` whose command pool it came from.
- `SwapChain` holds the `Surface`, which holds the window.
- `FrameContext` holds the swap chain and both queues.
- The samplers baked into a `DescriptorSetLayoutDesc::Binding` are held by reference and have to outlive the
  layout and every set allocated from it.

### Waiting before destroying

The other half of a lifetime is the device. Releasing an object the device is still reading is a use after
free that no validation layer catches reliably, so work has to be finished first:

- Per frame, that is what the fence of the frame is for; `FrameContext` already waits on it before it reuses
  a slot.
- At shutdown, `Device::WaitForAll()` waits for everything, and `DeviceQueue::WaitIdle()` for one queue.
  `FrameContext::Destroy()` calls the first one itself, so an application that uses one only has to make
  sure its own resources outlive the frame context.

### Moves

Every type is move-only. A move transfers the handle and everything that goes with it, and leaves the source
empty - `IsValid()` false, destructor does nothing. Moving is cheap and always safe; the source is simply
not usable afterwards.

Two cases are worth knowing because they are not obvious. Moving a `Device` re-points the queues it owns
back at the new object, since each queue holds a reference to its device. Moving a `Buffer` carries its
mapped pointer, so the mapping follows the object rather than staying with the corpse.

### What the swap chain owns

`SwapChain` owns its images, and `Recreate()` replaces all of them. Anything cached about it - an image
view, the image count, the extent - is stale afterwards. That is why acquire and present report
`SwapChainStatus::OutOfDate` rather than throwing: it is a signal to drop what was cached, not a failure.
Applications using `FrameContext` do not see this at all, since it re-reads what it needs each frame.

A depth image is optional: `SwapChainDesc::use_depth` is on by default, and a swap chain made without one
has an empty depth texture, so `GetDepthImageView()` is null. Ask `HasDepth()` rather than comparing that
view against a handle, and leave `RenderingDesc::depth_attachment` absent when it answers false - an
attachment that is present and names no image view throws, since a null view no longer means "no depth".

---

## The frame loop

`FrameContext` owns the parts of a frame that are the same in every application: a fence, a command buffer
and an image-ready semaphore per frame in flight, a render-finished semaphore per swap chain image, and the
order in which acquire, submit and present have to happen.

```cpp
FrameContext frame_context(device, swap_chain, graphics_queue, present_queue, {.frames_in_flight = 2});

while (!window->IsClosed())
{
    application->ProcessSystemEvents();

    if (frame_context.BeginFrame() == SwapChainStatus::OutOfDate)
    {
        continue;  // the swap chain was rebuilt, nothing was recorded
    }

    CommandBuffer& command_buffer = frame_context.GetCommandBuffer();
    command_buffer.CmdImageBarrier(ImageBarrier::ToColorAttachment(frame_context.GetColorImage()));
    command_buffer.CmdBeginRendering({.render_area_extent = frame_context.GetRenderSize(),
                                      .color_attachments = {{.image_view = frame_context.GetColorImageView(), ...}}});
    ...
    command_buffer.CmdEndRendering();

    frame_context.EndFrame();
}
```

`BeginFrame` waits for the slot this frame is about to reuse, acquires an image, and begins the command
buffer, so what comes back is already recording. `EndFrame` transitions the image to `Present`, ends the
command buffer, submits it against the right semaphores, and presents.

Resizing and minimizing are ordinary outcomes rather than errors. `BeginFrame` returning `OutOfDate` means
the swap chain has already been rebuilt, nothing was recorded, the fence of that slot was not reset and the
frame index did not advance - there is nothing to undo, so the loop just continues. A window with no client
area, as while minimized, has no swap chain at all; `swap_chain.IsValid()` says so, and an application that
does not want to spin should idle for a frame.

Anything the application keeps one of per frame in flight - a uniform buffer, a descriptor set - is indexed
by `frame_context.GetFrameIndex()`. `EndFrame` takes the layout the image was left in, defaulting to
`ImageLayout::ColorAttachment`, and passing `ImageLayout::Present` skips the transition for a frame that
already made it.

Work that happens once rather than per frame - uploading a mesh, generating mips - does not belong in this
loop. `ImmediateSubmit` from `rndr/forge/transfer.hpp` records, submits and waits in one call.

---

## Getting data in and out

Where a buffer's memory lives is decided by `BufferDesc::host_access`, and it decides which way of filling
the buffer works:

- `HostAccess::SequentialWrite`, the default, is memory the host writes and never reads. `Buffer::Update`
  writes into it directly. Reading it back is possible in principle and slow enough to be a bug, so
  `Buffer::Read` throws on such a buffer.
- `HostAccess::Random` is cached memory. Both `Update` and `Read` work, which is what a readback wants.
- `HostAccess::None` may land in memory the host cannot map at all, which is the fastest for the device.
  `Update` and `Read` both throw, and the buffer is filled and read through the staging helpers instead.

`UploadToBuffer`, `ReadBackBuffer` and `ReadBackTexture` in `rndr/forge/transfer.hpp` do the staging buffer,
the copy, the submit and the wait. They block, which is what setup code wants and what a frame does not.

Non-coherent memory is handled underneath: `Update` flushes and `Read` invalidates, so a write is visible to
the device when it returns and a read sees what the device wrote. Fences carry the rest - waiting on the
fence of a submit makes everything that submit wrote available to the host, so no extra barrier is needed
before reading a buffer the device just filled.

A texture is filled from a `Bitmap` by its constructor, which uploads every mip level the bitmap carries, or
generates the whole chain from level zero when asked. `CommandBuffer::CmdGenerateMips` does the same for a
texture that was filled some other way.

---

## Parameters and references

References at the API surface are plain C++ references. `Opal::Ref` appears only where a reference has to be
*stored*: as a member, as a field of a desc, or as the element type of a container, none of which can hold a
plain reference.

- An argument the call only reads is `const T&`. An argument the call mutates, or submits work to, is `T&` -
  `SwapChain::Present` takes `DeviceQueue&` because `DeviceQueue::Submit` is non-const.
- A constructor that keeps a reference to its argument still takes `const T&`; that it stores an
  `Opal::Ref<const T>` internally is an implementation detail, and every Forge constructor already takes its
  device this way.
- A range of objects is `Opal::ArrayView<const T>`, by value. `ArrayView` is already a view, so a reference to
  one buys nothing, and `const` on the element type says the call does not write through it. When the range is
  of objects rather than values, the element type is `Opal::Ref<const T>`, since an array of references is not
  a thing - `CommandBuffer::CmdBindDescriptorSets` and `Pipeline::CreatePipelineLayout` both look like this.
- A getter that hands back an object the callee owns returns `T&`, not `Opal::Ref<T>`. `Device::GetQueue`
  throws when the device has no such queue, so there is no absent case for the return type to carry.

`Opal::Ref` is move-only, so taking one by value forces every call site to write `.Clone()` for a reference
that is only read during the call. That is what this convention removes.

---

## How much Vulkan is visible

Forge is a wrapper, not a passthrough. Everything a desc asks for is named in Forge's own vocabulary -
`BufferUsageBits`, `TextureUsageBits`, `SampleCount`, `PresentMode`, `ColorSpace`, `TextureDimension`,
`TextureViewType` - so filling one in never requires reaching for a `Vk` enum.

That vocabulary is split across two headers by what the type describes, not by who uses it:

- `rndr/forge/types.hpp`, namespace `Rndr::Forge`, holds the types that are Vulkan concepts: `ImageLayout`,
  `ImageAspectBits`, `ImageSubresourceRange`, `PipelineStageBits`, `PipelineStageAccessBits` and the usage
  masks above. An OpenGL renderer has no use for them.
- `rndr/graphics-types.hpp`, namespace `Rndr`, keeps what any graphics API would recognize - `PixelFormat`,
  `ShaderTypeBits`, `IndexSize`, `Comparator`, `BlendFactor`, `SamplerDesc` - and is shared with Canvas.

The values of the flag enums mirror the Vulkan values they map to, so translating a mask is a cast. The
plain enums are translated by a `ToVk*` switch in the source file that needs them, which is why a value with
no Vulkan counterpart cannot be cast into one by accident.

Vulkan is still visible in two deliberate places. `GetNative*()` on every type hands out the raw handle,
which is the escape hatch for anything Forge does not wrap yet. `Surface::GetSwapChainSupportDetails` and
the queue family queries on `PhysicalDevice` return what Vulkan reported, since they exist to inspect the
driver rather than to describe an object.

`DeviceFeatures` is the same idea applied to device creation. Its fields are named for what they do rather
than for the Vulkan version that introduced them, and Forge maps each one onto whichever feature structure
Vulkan keeps it in and chains those itself. A caller never sees `VkPhysicalDeviceVulkan12Features`, never
keeps a `pNext` chain alive, and never has to know that buffer device addresses arrived in 1.2 while
descriptor indexing arrived in the same release by a different name.

---

## Error handling

Three rules, in the order they are applied.

### Failures throw

Anything that went wrong throws. A `VkResult` that means the call failed becomes a `VulkanException`, which
carries both the result and the name of the Vulkan function it came from. Misuse of the API - an unhandled
enum value, an update that would run past the end of a buffer - becomes an `Opal::Exception`.

This holds for ordinary methods, not only for constructors. Constructors have no alternative, because Forge
has no two-phase initialization and no half-built objects, but `Buffer::Update`, `CommandBuffer::Begin` and
`DescriptorPool::Reset` throw for the same reason: there is no useful value to return when the call did not
do what it says.

Exceptions cost nothing until one is thrown, and the checks that precede them are needed either way, so
there is no reason to spell these as return values on the per-frame path.

### Expected outcomes are return values

Not everything Vulkan reports as an error is one. A swap chain that no longer matches its surface is the
normal consequence of the user resizing the window, so `SwapChain::AcquireImage` and `SwapChain::Present`
return a `SwapChainStatus` and the caller skips a frame. `VK_ERROR_OUT_OF_DATE_KHR` never leaves the swap
chain as an exception.

The same applies to queries whose answer may legitimately be "there is none". A device with no queue family
matching a set of flags is not a broken device, so `PhysicalDevice::GetQueueFamilyIndex` returns an
`Opal::Optional<u32>`. Inventing a `VkResult` to describe the absence would add no information the caller
did not already have.

Use `Opal::Optional` when the outcome is present-or-absent, and a dedicated status enum when there are more
than two outcomes or when the caller has to react differently to each.

### Never log and return a default

A function that logs an error and returns an empty container hands back a value the caller cannot tell from
a legitimately empty result - `EnumeratePhysicalDevices` returning nothing has to mean "this machine has no
Vulkan device", never "the enumeration failed". Failures throw, so the two stay distinguishable.

`RNDR_RETURN_ON_FAIL` is therefore not used in Forge.

### What this means for callers

An application that sets up a device and its resources should wrap that setup in one `try` block rather than
checking each call. The frame loop below it needs no error handling beyond the `SwapChainStatus` it already
has to react to.

---

## Barriers

A barrier says which work has to finish before which other work may start, and for a texture it also says
what the image is about to be used for. Forge spells both sides the same way: the stages that must finish
and the access they made, then the stages that must not start and the access they will make.

`ImageBarrier` has a preset for each of the standard transitions - `ToColorAttachment`,
`ToDepthStencilAttachment`, `ToShaderRead`, `ToTransferSource`, `ToTransferDestination`, `ToPresent`, and
`To` for a layout that is not known while writing the call. Each picks the stages and the access from what
the texture is about to be used for, and derives the source side from the layout it is coming from, so the
only thing left to the caller is where the texture is now. `BufferBarrier::WriteThenRead` and
`::ReadThenWrite` cover the two orderings a buffer needs.

`PipelineStageAccessBits` offers two ways to name an access, and the difference matters:

- `Read` and `Write` are any read and any write. They are correct beside every stage, which is what makes
  them the right thing to reach for in a barrier written by hand - an access that does not match its stage
  is invalid, and this pair never can be.
- The named bits - `ColorAttachmentWrite`, `TransferRead`, `ShaderSampledRead` and the rest - say which read
  or which write, which lets a driver leave everything else alone. Forge's own presets use these, because a
  preset knows exactly what the texture is for. Reach for them by hand once the coarse pair shows up in a
  profile, not before.

`CmdBarriers` takes barriers of all three kinds and issues them as one dependency, which is cheaper than one
call per kind. Every other `Cmd*Barrier` is that call with the other groups left empty.

Two things a barrier can carry that most do not need. `source_queue_family` and `destination_queue_family`
transfer ownership between queue families, as two barriers naming the same pair: a release on the source
queue and an acquire on the destination one. Both default to `k_ignored_queue_family`, which means no
transfer. `Barriers::flags` can mark a dependency by-region, which is only valid inside a render pass.

---

## Debugging

Build with `-DRNDR_FORGE_VALIDATION=ON` and the validation layer is enabled whenever it is installed. Its
messages are logged, and the context also keeps them:

```cpp
if (context.GetDebugMessageCount(DebugMessageSeverity::Error, DebugMessageTypeBits::Validation) != 0)
{
    for (const DebugMessage& message : context.GetDebugMessages()) { ... }
}
```

Ask for `DebugMessageTypeBits::Validation` rather than for every type. The loader reports its own problems
through the same callback at error severity - a layer manifest that some other application left behind on
the machine is a common one - and none of that says anything about this code. `ClearDebugMessages()` starts
a fresh stretch, for measuring one piece of work rather than the whole run.

Info messages are counted but not stored, since the loader emits thousands of them, and the stored warnings
and errors are capped by `GraphicsContextDesc::max_stored_debug_messages`.

`SetDebugName` from `rndr/forge/debug.hpp` names any object, and both the messages and a capture use the
name in place of the handle. `DebugMessage::objects` carries the names of what a message is about, which the
layer hands over beside the text rather than inside it. Naming does nothing in a build without the debug
utils extension, so call sites never have to ask first.

One thing the layer cannot save anyone from: work that breaks the specification is undefined behaviour once
it is submitted, whatever the layer said about it. A command that the layer rejects at record time should be
thrown away rather than submitted - the driver may take the process down some time later, far from the
cause.

### Labelled regions

A name says what a resource is; a label says what a stretch of work is. `CommandBuffer::CmdBeginDebugLabel`
and `CmdEndDebugLabel` bracket a region of the command stream, which a capture shows as one collapsible
entry in place of the loose commands inside it, and `CmdInsertDebugLabel` marks a single point.
`ScopedDebugLabel` from `rndr/forge/debug.hpp` is the pair as a scope, so an early return or a thrown
exception cannot leave a region open:

```cpp
{
    ScopedDebugLabel forward_pass(command_buffer, "forward pass", {0.2f, 0.6f, 1.0f, 1.0f});
    command_buffer.CmdBeginRendering(...);
    ...
    command_buffer.CmdEndRendering();
}
```

Regions nest, and every one has to be closed. The colour is a hint and nothing more: Vulkan gives it no
meaning, the validation layer never reads it, and a tool is free to ignore it - RenderDoc tints the row with
it, which is enough to tell two passes apart at a glance.

Like naming, labelling is a no-op in a build without the debug utils extension. All three commands ask the
same question, so a build without it skips both halves of a region rather than one.

---

## GPU timing

`TimestampQueryPool` in `rndr/forge/query.hpp` owns a pool of timestamps, and
`CommandBuffer::CmdWriteTimestamp` writes the device's tick counter into one of them. The difference between
two ticks is what the device spent between those points, which is what the CPU side of a frame cannot tell
you - `Opal::GetSeconds()` around a frame measures how long submitting took, not how long the work did.

```cpp
TimestampQueryPool timer(device, {.query_count = 2});

command_buffer.CmdResetQueryPool(timer);
command_buffer.CmdWriteTimestamp(timer, 0, PipelineStageBits::PipelineStart);
...
command_buffer.CmdWriteTimestamp(timer, 1, PipelineStageBits::PipelineEnd);

// Once the fence of that submit has been waited on.
f64 gpu_milliseconds = 0.0;
if (timer.TryGetElapsedMilliseconds(0, 1, gpu_milliseconds)) { ... }
```

A pool holds **undefined values until it is reset**, and reading one that never was is undefined rather than
empty - the validation layer says so. `CmdResetQueryPool` works on every device;
`TimestampQueryPool::Reset` does it from the host and needs `DeviceFeatures::host_query_reset`.

`timestampValidBits` and `timestampPeriod` are handled inside: ticks come back masked to the bits the queue
family actually writes, and the elapsed helpers apply the device's nanoseconds per tick. The period is also
the resolution floor, so on a device reporting tens of nanoseconds per tick a sub-microsecond measurement is
noise. Ticks belong to the timeline of one queue family, so a tick from a graphics queue and one from an
async compute queue are not comparable without `VK_EXT_calibrated_timestamps`.

### What a pair of timestamps measures

`CmdWriteTimestamp` is a marker in the command stream, not a timer around a scope. The tick is written once
every *previously submitted* command has reached the stage the call names, and which stage that is decides
what the pair means:

- **A span** - `PipelineStart` then `PipelineEnd`. The first waits for nothing and fires as the device
  reaches it; the second waits for everything before it to finish. The difference is wall time on the queue
  across that stretch, which is what a frame or a pass wants. Bracketing a *single draw* this way does not
  give that draw's cost: the GPU overlaps work, and the span swallows whatever neighbouring work was still
  in flight.
- **One operation on its own** - `PipelineEnd` on both sides. The write in front drains everything before
  it, so the difference covers the operation between them and nothing else. The drain is the price: an
  operation measured with no overlap is not what it costs inside a real frame, where it would run alongside
  its neighbours. Both numbers are true, and they answer different questions.

### Reading without stalling

A result is not there when the command buffer is submitted, only when the device has run that far. Blocking
on the frame that recorded it throws away the frames in flight, so keep one pool per frame in flight, write
into the pool of the current frame, and read the pool whose fence has already been waited on:

```cpp
frame_context.BeginFrame();                       // waited on the fence of this slot
TimestampQueryPool& timer = timers[frame_context.GetFrameIndex()];
timer.TryGetElapsedMilliseconds(0, 1, gpu_ms);    // the frame from frames_in_flight ago
command_buffer.CmdResetQueryPool(timer);          // only now is it safe to throw those away
```

The measurement lags by the number of frames in flight and costs nothing. `GetResults` and
`GetElapsedMilliseconds` are the blocking pair, for setup and for tests; `TryGet*` returning false means the
device has not reached those queries yet, which is what the first frames of a per-frame pool look like.
