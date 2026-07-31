# Forge (Vulkan) API

Forge is the lower level of the two rendering APIs in this repository. It is a thin, owning wrapper over
Vulkan: every type holds one Vulkan handle, is move-only, and releases the handle in its destructor.

This document covers the conventions that hold across the whole API. Task 4.7 in `docs/forge-tasks.md`
tracks filling in object lifetimes and the frame loop.

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

Vulkan is still visible in three deliberate places. `GetNative*()` on every type hands out the raw handle,
which is the escape hatch for anything Forge does not wrap yet. `Surface::GetSwapChainSupportDetails` and
the queue family queries on `PhysicalDevice` return what Vulkan reported, since they exist to inspect the
driver rather than to describe an object. And `DeviceDesc::features` is still `VkPhysicalDeviceFeatures`,
which task 3.6 in `docs/forge-tasks.md` covers.

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
