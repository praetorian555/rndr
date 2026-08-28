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
two-phase initialization step. There is no method to call on an empty object to fill it in; assign a created
one over it instead.

Everything else is built by a static `Create` that hands back `Opal::Expected<T, ErrorCode>`, so an object
that exists is one that was created successfully. A constructor cannot report anything without throwing, and
Forge does not throw - see the error handling section below.

`IsValid()` reports whether an object holds a handle. Every type has it, and it always answers the same
question: false for a default-constructed object, false after `Destroy()`, false for the source of a move,
true otherwise.

A method that reports answers `ErrorCode::InvalidArgument` on an empty object. The accessors do not: they
return null handles and dereference references that are not there, so `IsValid()` is still the guard rather
than a Vulkan handle comparison at the call site.

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

### A device before any window

Everything a swap chain needs is decided at device creation - the present queue's family and the
swap chain extension - and the usual way to decide both is the surface in `DeviceDesc::surface`.
That couples the device to a window: surface needs window, window needs the platform layer, so an
application that wants its device first - an engine whose UI library will create the windows -
cannot name one.

`DeviceDesc::enable_presentation` breaks the coupling. It asks for the present queue and the
extension without a surface: the family comes from the platform's surface-free query
(`vkGetPhysicalDeviceWin32PresentationSupportKHR` on Windows; on Linux the XCB query needs a
window's connection, so the graphics family stands in), and whether that family can present to a
*particular* surface is verified when a swap chain is created over one - `SwapChain` checks
`vkGetPhysicalDeviceSurfaceSupportKHR` against the device's present family either way, which also
covers a second window whose surface the device never saw. A surface in the desc answers the same
question more precisely and wins when both are given.

`Device::CanPresentTo` is that check on its own, for a surface that arrives after the device. It is
the `vkGetPhysicalDeviceSurfaceSupportKHR` the swap chain runs anyway, so nothing has to call it -
what it buys is the failure landing where the window is created rather than at the first swap chain,
which is the difference between "this window cannot be presented to" and a swap chain that will not
build. A device created without presentation answers false rather than failing.

The device presents to no surface of its own, and there is nothing to switch between windows: a
surface belongs to the `SwapChain` built over it, and one device drives as many swap chains as there
are windows - a `Surface`, a `SwapChain` and a `FrameContext` each, all on the same queues.

### More than one context

`GraphicsContext` is the one object that is not only its own. Forge dispatches through volk, which keeps a
single set of function pointers for the whole process: `volkInitialize` loads them and `volkFinalize` nulls
them and unloads the library. Neither counts its callers, so Forge counts the contexts and only the last one
out tears volk down. Destroying a second context no longer takes the first one down with it.

What is left is `volkLoadInstance`, which points the global instance *and* device tables at whichever
instance was created most recently. Two live contexts therefore share one table, and the older one
dispatches through the newer one's instance. It works, because the loader's entry points are trampolines
that dispatch on the handle they are given, but it is not something to rely on. Creating a second context
while one is live logs a warning saying so. Prefer one context per process; the reason to want a second is
usually a second *device*, which one context makes as many of as you like.

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

`SwapChain` owns its textures, and `Recreate()` replaces all of them. Anything cached about it - a view,
the texture count, the extent - is stale afterwards. That is why acquire and present report
`SwapChainStatus::OutOfDate` on the value side rather than as an error: it is a signal to drop what was
cached, not a failure.
Applications using `FrameContext` do not see this at all, since it re-reads what it needs each frame.

The swap chain also remembers which texture it handed out. `AcquireTexture` records it,
`GetCurrentColorTexture()` hands it back, and `Present` takes no index because the swap chain already knows
which one it is - so an index never has to be threaded through a frame. `HasAcquiredTexture()` is true only
between the two, and asking for the texture outside that pair reports rather than returning a stale one.

A depth texture is optional: `SwapChainDesc::use_depth` is on by default, and a swap chain made without one
has an empty depth texture. Ask `HasDepth()`, and leave `RenderingDesc::depth_attachment` absent when it
answers false - an attachment that is present and names no texture is refused, since absent is what "no
depth" is spelled as.

What was presented can be read back, but only when the swap chain was asked for it.
`SwapChainDesc::allow_readback` adds `TransferSource` to the color textures, and it is off by default -
a frame that only presents has no use for the usage, and it can cost the driver a compression path. A
surface that does not offer it is refused rather than handing back textures whose desc claims a usage their
images do not have. The windowed tests are what it exists for; copy the swap chain texture inside the
frame that drew it rather than reading it after the present, which races the presentation engine.

---

## The frame loop

`FrameContext` owns the parts of a frame that are the same in every application: a fence, a command buffer
and a texture-ready semaphore per frame in flight, a render-finished semaphore per swap chain texture, and the
order in which acquire, submit and present have to happen.

```cpp
auto frame_context_result = FrameContext::Create(device, swap_chain, graphics_queue, present_queue,
                                                {.frames_in_flight = 2});
...
FrameContext frame_context = std::move(frame_context_result.GetValue());

while (!window->IsClosed())
{
    application->ProcessSystemEvents();

    auto begin_status = frame_context.BeginFrame();
    if (!begin_status.HasValue() || begin_status.GetValue() == SwapChainStatus::OutOfDate)
    {
        continue;  // the swap chain was rebuilt, nothing was recorded
    }

    CommandBuffer& command_buffer = frame_context.GetCommandBuffer().GetValue();
    Texture& color_texture = frame_context.GetColorTexture().GetValue();
    // The preset reads the layout off the texture, so what that reported rides through the command.
    (void)command_buffer.CmdTextureBarrier(TextureBarrier::ToColorAttachment(color_texture));
    (void)command_buffer.CmdBeginRendering({.render_area_extent = frame_context.GetRenderSize(),
                                           .color_attachments = {{.texture = color_texture, ...}}});
    ...
    (void)command_buffer.CmdEndRendering();

    (void)frame_context.EndFrame();
}
```

`BeginFrame` waits for the slot this frame is about to reuse, acquires a texture, and begins the command
buffer, so what comes back is already recording. `EndFrame` transitions the texture to `Present`, ends the
command buffer, submits it against the right semaphores, and presents.

The `(void)` casts above are the frame loop being explicit: every recorded command reports, and a loop that
means to render regardless says so rather than dropping the codes by accident. A loop that acts on them
keeps the first one instead.

Resizing and minimizing are ordinary outcomes rather than errors. `BeginFrame` answering `OutOfDate` means
the swap chain has already been rebuilt, nothing was recorded, the fence of that slot was not reset and the
frame index did not advance - there is nothing to undo, so the loop just continues. A window with no client
area, as while minimized, has no swap chain at all; `swap_chain.IsValid()` says so, and an application that
does not want to spin should idle for a frame.

Anything the application keeps one of per frame in flight - a uniform buffer, a descriptor set - is indexed
by `frame_context.GetFrameIndex()`. `EndFrame` takes the layout the texture was left in, defaulting to
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
  `Buffer::Read` refuses such a buffer with `ErrorCode::InvalidArgument`.
- `HostAccess::Random` is cached memory. Both `Update` and `Read` work, which is what a readback wants.
- `HostAccess::None` may land in memory the host cannot map at all, which is the fastest for the device.
  `Update` and `Read` both refuse it, and the buffer is filled and read through the staging helpers instead.

`UploadToBuffer`, `ReadBackBuffer` and `ReadBackTexture` in `rndr/forge/transfer.hpp` do the staging buffer,
the copy, the submit and the wait. They block, which is what setup code wants and what a frame does not.

Non-coherent memory is handled underneath: `Update` flushes and `Read` invalidates, so a write is visible to
the device when it returns and a read sees what the device wrote. Fences carry the rest - waiting on the
fence of a submit makes everything that submit wrote available to the host, so no extra barrier is needed
before reading a buffer the device just filled.

A texture is filled from a `Bitmap` by the `Texture::Create` overload that takes one, which uploads every mip
level the bitmap carries, or generates the whole chain from level zero when asked. `CommandBuffer::CmdGenerateMips` does the same for a
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
- A getter that hands back an object the callee owns returns `T&`, not `Opal::Ref<T>`. Where the object may
  be absent - `Device::GetQueue` on a device created without that family - it is
  `Opal::Expected<T&, ErrorCode>`, which is Opal's own shape for a reference that may not be there.

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

### Image or texture

Vulkan says image where Forge says texture, and one word for one thing is what keeps the API searchable:
finding everything that touches a texture is one grep, not two.

**Anything that takes, holds or hands out a `Forge::Texture` says texture.** `TextureBarrier` and its
presets, `CmdTextureBarrier`, `CmdCopyBufferToTexture`, `CmdCopyTextureToBuffer`, `CmdCopyTexture`,
`CmdBlitTexture` and their region types, `DescriptorSetUpdateBinding::TextureInfo`, and the whole swap chain
surface - `AcquireTexture` returning an `AcquiredTexture`, `GetCurrentColorTexture`, `GetColorTexture`,
`GetDepthTexture`, `HasAcquiredTexture`, `GetCurrentTextureIndex`.

**A name says image only where it mirrors something Vulkan named.** That is `ImageLayout`,
`ImageAspectBits`, `ImageSubresourceRange` and `ImageSubresourceLayers`, which mirror the `Vk` types of
those names; the `DescriptorType` values `SampledImage`, `CombinedImageSampler` and `StorageImage`, which
mirror `VkDescriptorType`; and `Texture::GetNativeImage()` and `GetNativeImageView()`, which are named after
the `VkImage` and `VkImageView` they hand out. `Rndr::ImageFilter` and `Rndr::ImageAddressMode` are not
Forge types at all - they live in `rndr/graphics-types.hpp` and are shared with Canvas.

So a call site that mentions an image is either reaching for a layout, a descriptor type or a raw handle.
Anything else is a texture.

---

## Error handling

Nothing in Forge throws. Three rules, in the order they are applied.

### Failures are return values

Anything that went wrong comes back as a `Rndr::ErrorCode`, and the detail goes to the log at error level.
A call that produces something hands back `Opal::Expected<T, ErrorCode>`; a call that produces nothing hands
back the code itself, `ErrorCode::Success` when it worked. Both are `[[nodiscard]]`, so a code cannot be
dropped by accident - `(void)` is how a caller says it means to.

A failing `VkResult` becomes the code `VkResultToErrorCode` maps it to, and the log line carries the result
and the name of the Vulkan function it came from - so the code stays small enough to switch on while nothing
about the failure is lost. Misuse of the API is reported the same way: `ErrorCode::InvalidArgument` for a
value that names nothing or a desc that asks for what this device cannot do, `ErrorCode::OutOfBounds` for a
range that does not fit.

This is the same convention `src/audio/` and Canvas use, and the one Opal's own containers use -
`DynamicArray::Create`, `TryAt`, `Opal::FileSystem` - which is where it came from.

Creation goes through a static `Create` rather than a constructor, since a constructor has no way to report
without throwing. `Create` builds into a local object and hands it back, so a step that gives up part way
leaves through that object's destructor with everything already released. The rule is worth knowing when
adding a step to one of these: a new check placed after a `vkCreate*` call needs nothing around it, as long
as what the call produced went into the object being built.

Two things do not report. `SetDebugName` is best effort - a name that could not be set changes nothing a
caller would act on - and the accessors on an empty object are undefined rather than checked, which is what
`IsValid()` is the guard for.

`ShaderCompiler` used to be the exception to this, and is no longer: it reports an `ErrorCode` like
everything else, and `Shader::FromSourceInMemory` passes on what it reported instead of catching. Its
`LoadModule` answers a code, and `DiscoverEntryPoints`, `CompileEntryPoint`, `FindSingleEntryPoint` and
`MergeParameters` answer an `Expected`.

### Nothing throws, and the build says so

"Nothing throws" is a claim about the whole program rather than about Forge's own code, because Forge calls
Opal underneath and Opal's own rule lands on the same wall this one does: anything that can report does, and a
constructor, which cannot, has only a throw left. A sized `Opal::DynamicArray` or a `StringUtf8` that outgrows
its inline buffer therefore had one failure - running out of memory - that no `ErrorCode` described.

Opal is configured here with `OPAL_EXCEPTIONS=OFF` (in `cmake/dependencies.cmake`), which replaces that throw
with its contract violation handler: the failure prints and ends the program rather than unwinding. So there
is no exception to catch anywhere in Rndr, `try` and `catch` do not appear in the library, and on MSVC the
build carries `/EHs-c-` and `_HAS_EXCEPTIONS=0` through Opal's `PUBLIC` interface. An application that wants
to unwind out of memory exhaustion instead wants that option back on.

What this costs is that out of memory is fatal rather than reportable. It was never reportable here - the
throw had no catch in Rndr either - so what changed is where it stops, not whether it does.

### Expected outcomes are values, not errors

Not everything Vulkan reports as an error is one. A swap chain that no longer matches its surface is the
normal consequence of the user resizing the window, so `SwapChain::AcquireTexture` and `SwapChain::Present`
carry `SwapChainStatus::OutOfDate` on the *value* side of their `Expected` and the caller skips a frame.
`VK_ERROR_OUT_OF_DATE_KHR` never leaves the swap chain as an error code.

A timeout is the same shape. `Fence::TryWait` answers `Expected<bool, ErrorCode>`: false means the fence was
not signalled in time, which is the one thing the timeout parameter exists for, and only a failed wait is an
error.

The same applies to queries whose answer may legitimately be "there is none". A device with no queue family
matching a set of flags is not a broken device, so `PhysicalDevice::GetQueueFamilyIndex` returns an
`Opal::Optional<u32>`. Inventing a code to describe the absence would add no information the caller did not
already have.

Use `Opal::Optional` when the outcome is present-or-absent, a dedicated status enum when there are more than
two outcomes or when the caller has to react differently to each, and an `ErrorCode` only for what actually
went wrong.

### Never log and return a default

A function that logs an error and returns an empty container hands back a value the caller cannot tell from
a legitimately empty result. An `Expected` keeps the two apart by construction, which is what makes
`RNDR_RETURN_ON_FAIL` unnecessary here rather than forbidden: `RNDR_FORGE_CHECK` and
`RNDR_FORGE_VK_CHECK` carry a code out of the call that reported it, and neither can be mistaken for a
value.

`EnumeratePhysicalDevices` is worth following through, because it lands on the other side of the previous
rule. An empty list there would be a legitimate answer in the sense that nothing went wrong - the machine
simply has no Vulkan capable device - but it is not an *outcome the caller can act on*. Every caller either
indexes the first element or hands the list to `SelectPhysicalDevice`, so an empty one is a check every one
of them has to remember and none of them did. It reports `ErrorCode::NoGraphicsDevice` instead, separate
from the code that means the enumeration itself failed.

The test that distinguishes the two rules is not "did something go wrong" but "is there more than one thing
the caller might reasonably do next". A device with no queue family matching some flags leaves the caller a
real choice - fall back to another family, or give up - so `GetQueueFamilyIndex` returns an `Opal::Optional`.
A machine with no device at all leaves no choice, so it is an error code.

### What this means for callers

Setup is a chain of `Create` calls, each of which can report. An application that has nothing to fall back on
writes one helper that unwraps or stops, and uses it throughout - `modern-vulkan` calls its two `Require` and
`RequireOk`, and the tests have `ForgeTest::Unwrap`, which fails the case with the code. One helper beats a
check per line, and the log already says which call failed and why.

The frame loop needs no error handling beyond the `SwapChainStatus` it already has to react to. Every
recorded command reports, and a loop that means to render regardless says so with `(void)`.

---

## Barriers

A barrier says which work has to finish before which other work may start, and for a texture it also says
what the texture is about to be used for. Forge spells both sides the same way: the stages that must finish
and the access they made, then the stages that must not start and the access they will make.

`TextureBarrier` has a preset for each of the standard transitions - `ToColorAttachment`,
`ToDepthStencilAttachment`, `ToShaderRead`, `ToTransferSource`, `ToTransferDestination`, `ToPresent`, and
`To` for a layout that is not known while writing the call. Each picks the stages and the access from what
the texture is about to be used for, and derives the source side from the layout it is coming from.
`BufferBarrier::WriteThenRead` and `::ReadThenWrite` cover the two orderings a buffer needs.

### The texture remembers its layout

Vulkan makes the current layout of an image the caller's bookkeeping: neither the driver nor the validation
layer catches a plausible-but-wrong `oldLayout`, and `VK_IMAGE_LAYOUT_UNDEFINED` is always accepted because
it means "throw the contents away". Forge does that bookkeeping instead. `Texture` keeps one layout per
(mip level, array layer), starting at `Undefined` the way a freshly created image does, and `CmdBarriers`
writes the new layout back over the range every texture barrier covered.

So the short form of each named preset takes the source layout off the texture:

```cpp
command_buffer.CmdTransition(texture, ImageLayout::ShaderReadOnly);      // the whole transition
command_buffer.CmdTextureBarrier(TextureBarrier::ToShaderRead(texture)); // the same, with the stages spelled out
```

`GetCurrentLayout()` answers for the whole texture and reports when the levels disagree, which is what mip
generation leaves behind halfway through; `GetCurrentLayout(mip_level, array_layer)` answers for one of
them. The commands that need a layout without changing it - `CmdCopyTexture`, `CmdBlitTexture`,
`CmdCopyBufferToTexture`, `CmdCopyTextureToBuffer`, `CmdGenerateMips`, `ReadBackTexture` - read it the same
way, for exactly the levels their regions name, and refuse it when it is not one the role allows. That last
check is the one the old API could not make.

A preset that has to read the layout can therefore fail, and hands back `Expected<TextureBarrier, ErrorCode>`
where the long form given the old layout hands back the barrier itself. `CmdTextureBarrier` takes either, so
both spellings read the same at a call site and what the read reported comes out of the command.

`CmdBeginRendering` is the same thing at the other end of the frame. A `RenderingAttachmentDesc` names the
texture it draws into rather than an image view, so the layout it is rendered in is read off that texture
instead of being written out beside it - and the role decides which layouts are allowed: `ColorAttachment`
or `General` for a colour attachment, `DepthStencilAttachment`, `DepthStencilReadOnly` or `General` for the
depth and stencil ones. Anything else is refused, `Undefined` included, which is a texture no barrier has
moved into place yet. The layer rejects an undefined attachment layout too; what it cannot reject is a layout that
is legal and not the one the barriers actually left the texture in, and that is the case this removes.

The clear value is the same idea one field over. `clear_value` is an
`Opal::Variant<Vector4f, DepthStencilClearValue>` rather than the union `VkClearValue` is, so it remembers
which kind was written and a colour clear on a depth attachment is refused at `CmdBeginRendering` instead of
clearing to whatever the first two floats of that vector mean as a depth and a stencil - the one attachment
misuse the layer cannot catch either, since it is handed the same union. Only a `Clear` load operation reads
it, so an attachment that loads or discards leaves the default alone whichever role it plays.

The long form of each preset, the one that is told the old layout, stays for the two cases the tracker
cannot answer: a barrier over part of a texture whose range is narrowed after the preset built it, and a
deliberate discard, which is `ImageLayout::Undefined` and is now something a call site has to say out loud.

`To` is the exception with no short form. It is spelled `To(texture, old_layout, new_layout)`, so a short
`To(texture, new_layout)` would put the destination in the slot the source occupies in the long one, and
dropping an argument would compile into the opposite of what was meant. `CmdTransition` is its short form.

**This is record-time bookkeeping, not execution-time.** It is correct exactly while one thread records
command buffers in the order they will execute. It lies when two command buffers that touch the same texture
are recorded interleaved, and `CommandBuffer::Reset()` does not roll it back - the recorded barriers go, the
layouts they set stay. Swap chain textures are the one case Forge cannot observe, so `AcquireTexture` resets
the texture it hands out to `Undefined`, which is what the specification says about a re-acquired image and
what a frame that clears it wants anyway.

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

## Specialization constants

A specialization constant is a value the shader declares and the pipeline decides, folded in when the
pipeline is built. One SPIR-V module becomes several pipelines that differ in a light count, a tile size or
a feature toggle, without recompiling the shader or reaching for a preprocessor.

```slang
[SpecializationConstant]
const int RED_LEVEL = 64;
```

```cpp
GraphicsPipelineDesc desc;
desc.vertex_shader = vertex_shader;
desc.fragment_shader = fragment_shader;
desc.specialization.PushBack({.name = "RED_LEVEL", .value = 200});
```

Values are keyed by **name**, not by the numeric id Vulkan uses. That is the whole reason the names are
resolved through reflection: the specification says a `constantID` matching no constant in the shader *"does
not affect the behavior of the pipeline"* - so a mistyped number does nothing, in silence, and the pipeline
renders with the default while the caller believes it does not. A name no stage of the pipeline declares is
refused instead.

A constant left unnamed keeps the default the shader gave it. A value whose type does not match the declared
one is refused rather than coerced, an integer into a float included. So is one name given a value twice,
which would become two map entries sharing a `constantID` - not allowed within one `VkSpecializationInfo`.

`Shader::GetSpecializationConstants()` says what a module declares - name, id, type, default and the bytes it
occupies - so what can be specialized is answerable without reading the shader source. A constant narrower
than 32 bits is reported as `Int32` or `UInt32`, so a caller writes a plain integer for it; `byte_size` keeps
the declared width, which is what the map entry has to carry, and a value too wide for it is refused rather
than truncated.

These are applied at pipeline creation and do not change the SPIR-V, so nothing about them affects how a
shader is compiled or cached.

Slang spells the declaration `[SpecializationConstant]`, which lets it assign the ids, or
`[vk::constant_id(N)]` to pin one. Both reflect identically here.

---

## Shader reflection

A `Shader` reads its SPIR-V once when it is created and keeps what reflection found: the specialization
constants above, and what the entry point reads from a vertex buffer, through a descriptor, and from a push
constant block.

```cpp
shader.GetInputs();                   // location, format, name
shader.GetBindings();                 // set, binding, kind, count, name
shader.GetPushConstants();            // offset, size
shader.GetSpecializationConstants();  // id, type, default, byte size
```

All of it is scoped to the entry point, not to the module. A Slang file holding a vertex and a fragment
entry point reports each of them what it alone reads - the vertex stage of the sample declares no
descriptors at all, and the module-wide view would have handed it the fragment stage's two.

### What it is used for

`VertexInputDesc::FromShader(vertex_shader)` builds one binding holding every attribute the shader reads,
in location order. Reflection has the location and the format of each; it does not have the offset or the
stride, which describe the vertex struct on the CPU side and appear nowhere in the SPIR-V, so this packs
the attributes tightly and takes the total as the stride. **That is an assumption**, true of a plain struct
whose members are declared in the order the shader reads them. A vertex buffer laid out any other way needs
the desc written by hand.

`PushConstantRangesFromShaders({shaders, count})` returns the ranges the shaders read, with a block declared
by two stages merged into one range naming both.

`DescriptorSetLayoutDesc::shaders` does not build a layout - bindings are still written by hand, since only
the caller knows about immutable samplers, bindless flags and arrays sized past what the shader indexes.
What naming the shaders does is check the layout against them and hand each binding the name the shader
uses:

```cpp
layout_desc.shaders = {vertex_shader, fragment_shader};
layout_desc.AddBinding(0, DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
...
descriptor_set.Update("albedo_texture", albedo_texture, albedo_sampler);
```

### What is checked, and what cannot be

Building a graphics pipeline always checks the vertex input and the push constant ranges against the
shaders, however the desc was written. A layout checks its bindings only when it was given `shaders`.
These are refused:

- a location the vertex shader reads that no attribute feeds;
- an attribute whose numeric class is not what the shader reads it as, an integer against a float. Component
  counts are left alone on purpose - Vulkan pads a shorter attribute and drops the tail of a longer one;
- a push constant block no supplied range covers, including the case of no range at all;
- a binding declared as a different kind than the shader reads, sized for fewer descriptors than the shader
  indexes, or whose stages leave out a stage that reads it;
- a binding the shaders read that the layout never declared.

One thing is deliberately **not** refused: a vertex attribute at a location the shader declares nothing at,
or a layout binding no shader reads. Both are tempting and both are impossible to get right. A shader input
or a descriptor that nothing reads is optimised out of the SPIR-V, so reflection cannot tell a stale entry
apart from one feeding a member this entry point happens to ignore - the sample binds a metallic roughness
texture its fragment shader does not sample yet. Such a binding keeps an empty name and stays out of the
by-name lookup, which is all it costs.

`Update` by name is only as good as the names, so a layout built without `shaders` carries none, and asking
it for one says so rather than guessing.

---

## Shader compilation and the cache

Slang is the whole of Forge's startup cost. Compiling the sample's two entry points takes seconds;
everything built out of the result - the shader module, the pipeline, the reflection - takes milliseconds.

Two things make that cheaper and neither is on by accident.

**The Slang global session is shared.** `slang::createGlobalSession` loads Slang's core module and costs
about 650 ms in a debug build. It used to run once per compile. There is now one per process, created on
first use, and nothing about the compiler's behaviour changes with it. Not synchronized: nothing in Rndr
compiles shaders off the main thread.

**`ShaderCache` keeps the compiled code**, in memory for as long as the object lives and, when it was given
a directory, on disk for as long as the files do:

```cpp
Rndr::ShaderCache shader_cache{Opal::StringUtf8(RNDR_CORE_ASSETS_DIR "/../build/shader-cache")};
const auto shader = Forge::Shader::FromSource(device, path, {.entry_point = "main_vertex", .cache = shader_cache});
```

The cache belongs to the application, not to Forge - a `ShaderDesc` without one compiles every time, exactly
as before. It has to outlive the shaders that fill it to be worth anything, which is why it is not something
`Device` hands out.

A hit never creates a Slang session at all. Even the compiler version in the key comes from
`spGetBuildTagString`, a free function, so a warm run of the sample does not load Slang.

### What a hit means

A wrong hit is the only failure worth worrying about: it hands back a shader that is not the source being
read, which is a debugging session nobody enjoys. So the key holds **the source text whole**, the entry
point, the output format and the Slang build tag, and a lookup compares all four byte for byte. The hash
only names the file and narrows the search - a collision costs a recompile, never a wrong answer.

Everything that can go wrong with a blob means the same thing and takes the same path: no entry, compile it.
A missing file, one that does not parse, one truncated by a half-finished write, one written by a different
Slang. `Find` reports nothing at all.

One thing to know before changing the compiler: the cache is exact because `LoadModule` passes no
`searchPaths` to Slang, so the source string it is handed is the whole input. Adding search paths would let
a module pull in a file the key never sees, and the key would go on claiming hits for a source that changed
underneath it. There is a comment saying so where the paths would go.

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

The log has the same problem and its own answer: `GraphicsContextDesc::logged_message_types` says which
types are written out, and it is every type by default because a loader that cannot load a driver says so as
a `General` message and there is nowhere else to hear it. A program reading its own log for its own mistakes
- the test suite is one, building a context per case - narrows it to `Validation`, and the manifests of
other applications stop arriving. Only the log is filtered; `GetDebugMessages` and `GetDebugMessageCount`
answer for every type either way.

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
`ScopedDebugLabel` from `rndr/forge/debug.hpp` is the pair as a scope, so an early return cannot leave a
region open:

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

Every one of these takes the name as a `const char*` or as an `Opal::StringUtf8`, and a label written into the
source wants the first: Vulkan takes a `const char*` here, so it converts nothing, where a `StringUtf8` built
from a literal at the call site is a copy and, past the 23 characters that fit inline, an allocation - both
paid before the extension is even known to be there.

#### The GPU event macros

`RNDR_GPU_EVENT_SCOPED` and the `RNDR_GPU_EVENT_BEGIN` / `RNDR_GPU_EVENT_END` pair from `rndr/trace.hpp` are
the same regions spelled so that Canvas and Forge code can both reach for them. They are the backend-agnostic
name for the thing; `ScopedDebugLabel` is the Forge one, and the reason to keep using it directly is the
colour, which OpenGL has no counterpart for and the macros therefore do not carry.

Which backend a call means is decided by its arguments, since OpenGL pushes onto the context current on the
thread while Vulkan has to be told which command buffer to record into:

```cpp
RNDR_GPU_EVENT_SCOPED("shadow pass");                  // Canvas
RNDR_GPU_EVENT_SCOPED(command_buffer, "shadow pass");  // Forge
```

A build with both APIs has both forms, and nothing has to say which it means - the call site already knows,
because the command buffer is either in scope or the concept does not exist there.

These annotate; they do not measure. What the device spent is `TimestampQueryPool` below.

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
