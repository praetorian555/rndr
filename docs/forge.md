# Forge (Vulkan) API

Forge is the lower level of the two rendering APIs in this repository. It is a thin, owning wrapper over
Vulkan: every type holds one Vulkan handle, is move-only, and releases the handle in its destructor.

This document covers the conventions that hold across the whole API. Task 4.7 in `docs/forge-tasks.md`
tracks filling in object lifetimes and the frame loop.

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
