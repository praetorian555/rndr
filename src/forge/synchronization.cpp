#include "rndr/forge/synchronization.hpp"

#include <mutex>

#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-exception.hpp"

Rndr::Forge::Fence::Fence(const Device& device, bool create_signaled) : m_device(device)
{
    const VkFenceCreateInfo fence_create_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                                 .flags = create_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u};

    const VkResult result = vkCreateFence(device.GetNativeDevice(), &fence_create_info, nullptr, &m_fence);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateFence");
    }
}

Rndr::Forge::Fence::~Fence()
{
    Destroy();
}

void Rndr::Forge::Fence::Destroy()
{
    if (m_fence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_device->GetNativeDevice(), m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }
}

Rndr::Forge::Fence::Fence(Fence&& other) noexcept : m_fence(other.m_fence), m_device(std::move(other.m_device))
{
    other.m_fence = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::Forge::Fence& Rndr::Forge::Fence::operator=(Fence&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_fence = other.m_fence;
        other.m_fence = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::Forge::Fence::Wait() const
{
    // An infinite timeout cannot expire, so the answer is always true and there is nothing to report.
    (void)TryWait(k_infinite_wait);
}

bool Rndr::Forge::Fence::TryWait(u64 timeout) const
{
    const VkResult result = vkWaitForFences(m_device->GetNativeDevice(), 1, &m_fence, VK_TRUE, timeout);
    // VK_TIMEOUT is a success code: the wait did what it was told, the fence was simply not signalled in
    // time. Throwing on it would leave the timeout parameter unusable for the one thing it exists for.
    if (result == VK_TIMEOUT)
    {
        return false;
    }
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkWaitForFences");
    }
    return true;
}

void Rndr::Forge::Fence::Reset() const
{
    const VkResult reset_result = vkResetFences(m_device->GetNativeDevice(), 1, &m_fence);
    if (reset_result != VK_SUCCESS)
    {
        throw VulkanException(reset_result, "vkResetFences");
    }
}

void Rndr::Forge::Fence::WaitForAll(Opal::ArrayView<const Fence> fences)
{
    (void)TryWaitForAll(fences, k_infinite_wait);
}

bool Rndr::Forge::Fence::TryWaitForAll(Opal::ArrayView<const Fence> fences, u64 timeout)
{
    if (fences.empty())
    {
        return true;
    }
    // The default allocator rather than the scratch one: asking Opal for a scratch allocator asserts unless
    // the application pushed one, and nothing in Forge does, so this asserted on the first call it ever got.
    Opal::DynamicArray<VkFence> native_fences(fences.GetSize());
    for (i32 i = 0; i < fences.GetSize(); ++i)
    {
        const Fence& fence = fences[i];
        if (!fence.IsValid())
        {
            throw Opal::Exception("Waiting on an empty fence!");
        }
        // One wait is one call into one device, and the call below can only name the first fence's. Reading
        // that one is safe from the second entry on, since the first has been through the check above.
        if (i > 0 && fence.m_device->GetNativeDevice() != fences[0].m_device->GetNativeDevice())
        {
            throw Opal::Exception("Waiting on fences from more than one device at once!");
        }
        native_fences[i] = fence.GetNativeFence();
    }
    const VkResult wait_result = vkWaitForFences(fences[0].m_device->GetNativeDevice(), static_cast<u32>(native_fences.GetSize()),
                                                native_fences.GetData(), VK_TRUE, timeout);
    if (wait_result == VK_TIMEOUT)
    {
        return false;
    }
    if (wait_result != VK_SUCCESS)
    {
        throw VulkanException(wait_result, "vkWaitForFences");
    }
    return true;
}

namespace
{
/**
 * What has to finish before a texture can leave the given layout, as a stage and the access it made. The
 * layout says what the texture was last used for, so it says which work the transition has to wait on.
 */
struct SourceScope
{
    Rndr::Forge::PipelineStageBits stages = Rndr::Forge::PipelineStageBits::None;
    Rndr::Forge::PipelineStageAccessBits access = Rndr::Forge::PipelineStageAccessBits::None;
};

SourceScope ScopeOfLayout(Rndr::Forge::ImageLayout layout)
{
    using namespace Rndr::Forge;
    switch (layout)
    {
        case ImageLayout::Undefined:
            // Nothing is being preserved, so there is nothing to wait for.
            return {PipelineStageBits::PipelineStart, PipelineStageAccessBits::None};
        case ImageLayout::ColorAttachment:
            return {PipelineStageBits::ColorAttachmentOutput, PipelineStageAccessBits::ColorAttachmentWrite};
        case ImageLayout::DepthStencilAttachment:
            return {PipelineStageBits::EarlyFragmentTests | PipelineStageBits::LateFragmentTests,
                    PipelineStageAccessBits::DepthStencilAttachmentWrite};
        case ImageLayout::DepthStencilReadOnly:
        case ImageLayout::ShaderReadOnly:
            return {PipelineStageBits::FragmentShader, PipelineStageAccessBits::ShaderRead};
        case ImageLayout::TransferSource:
            return {PipelineStageBits::Transfer, PipelineStageAccessBits::TransferRead};
        case ImageLayout::TransferDestination:
            return {PipelineStageBits::Transfer, PipelineStageAccessBits::TransferWrite};
        default:
            // General and Present, plus anything added later, can have been touched by anything.
            return {PipelineStageBits::AllCommands, PipelineStageAccessBits::Read | PipelineStageAccessBits::Write};
    }
}
}  // namespace

Rndr::Forge::BufferBarrier Rndr::Forge::BufferBarrier::WriteThenRead(const Buffer& buffer, PipelineStageBits writer,
                                                                     PipelineStageBits reader)
{
    return {.stages_must_finish = writer,
            .stages_must_finish_access = PipelineStageAccessBits::Write,
            .before_stages_start = reader,
            .before_stages_start_access = PipelineStageAccessBits::Read,
            .buffer = buffer};
}

Rndr::Forge::BufferBarrier Rndr::Forge::BufferBarrier::ReadThenWrite(const Buffer& buffer, PipelineStageBits reader,
                                                                     PipelineStageBits writer)
{
    return {.stages_must_finish = reader,
            .stages_must_finish_access = PipelineStageAccessBits::Read,
            .before_stages_start = writer,
            .before_stages_start_access = PipelineStageAccessBits::Write,
            .buffer = buffer};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToColorAttachment(Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::ColorAttachmentOutput,
            .before_stages_start_access = PipelineStageAccessBits::ColorAttachmentRead | PipelineStageAccessBits::ColorAttachmentWrite,
            .old_layout = old_layout,
            .new_layout = ImageLayout::ColorAttachment,
            .image = texture};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToDepthStencilAttachment(Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::EarlyFragmentTests | PipelineStageBits::LateFragmentTests,
            .before_stages_start_access =
                PipelineStageAccessBits::DepthStencilAttachmentRead | PipelineStageAccessBits::DepthStencilAttachmentWrite,
            .old_layout = old_layout,
            .new_layout = ImageLayout::DepthStencilAttachment,
            .image = texture};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToShaderRead(Texture& texture, ImageLayout old_layout,
                                                                  PipelineStageBits reader)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = reader,
            .before_stages_start_access = PipelineStageAccessBits::ShaderRead,
            .old_layout = old_layout,
            .new_layout = ImageLayout::ShaderReadOnly,
            .image = texture};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToTransferDestination(Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::Transfer,
            .before_stages_start_access = PipelineStageAccessBits::TransferWrite,
            .old_layout = old_layout,
            .new_layout = ImageLayout::TransferDestination,
            .image = texture};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToTransferSource(Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::Transfer,
            .before_stages_start_access = PipelineStageAccessBits::TransferRead,
            .old_layout = old_layout,
            .new_layout = ImageLayout::TransferSource,
            .image = texture};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToPresent(Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    // The presentation engine synchronizes against the semaphore the present waits on, not against a stage,
    // so nothing on this side has to be blocked.
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::PipelineEnd,
            .before_stages_start_access = PipelineStageAccessBits::None,
            .old_layout = old_layout,
            .new_layout = ImageLayout::Present,
            .image = texture};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToColorAttachment(Texture& texture)
{
    return ToColorAttachment(texture, texture.GetCurrentLayout());
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToDepthStencilAttachment(Texture& texture)
{
    return ToDepthStencilAttachment(texture, texture.GetCurrentLayout());
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToShaderRead(Texture& texture, PipelineStageBits reader)
{
    return ToShaderRead(texture, texture.GetCurrentLayout(), reader);
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToTransferDestination(Texture& texture)
{
    return ToTransferDestination(texture, texture.GetCurrentLayout());
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToTransferSource(Texture& texture)
{
    return ToTransferSource(texture, texture.GetCurrentLayout());
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToPresent(Texture& texture)
{
    return ToPresent(texture, texture.GetCurrentLayout());
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::To(Texture& texture, ImageLayout old_layout, ImageLayout new_layout)
{
    switch (new_layout)
    {
        case ImageLayout::ShaderReadOnly:
            return ToShaderRead(texture, old_layout);
        case ImageLayout::ColorAttachment:
            return ToColorAttachment(texture, old_layout);
        case ImageLayout::DepthStencilAttachment:
            return ToDepthStencilAttachment(texture, old_layout);
        case ImageLayout::TransferSource:
            return ToTransferSource(texture, old_layout);
        case ImageLayout::TransferDestination:
            return ToTransferDestination(texture, old_layout);
        case ImageLayout::Present:
            return ToPresent(texture, old_layout);
        default:
            throw Opal::Exception("No barrier preset transitions a texture into that layout!");
    }
}

Rndr::Forge::Semaphore::Semaphore(const Device& device, const SemaphoreDesc& desc) : m_device(device), m_type(desc.type)
{
    // The type lives in a chained structure rather than in the create info, so a binary semaphore is the one
    // that chains nothing - which is also why it is what a defaulted desc asks for.
    const VkSemaphoreTypeCreateInfo type_create_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                                        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                                                        .initialValue = desc.initial_value};
    const VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = desc.type == SemaphoreType::Timeline ? &type_create_info : nullptr};
    const VkResult result = vkCreateSemaphore(device.GetNativeDevice(), &semaphore_create_info, nullptr, &m_semaphore);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateSemaphore");
    }
}

Rndr::Forge::Semaphore::~Semaphore()
{
    Destroy();
}

void Rndr::Forge::Semaphore::Destroy()
{
    if (m_semaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device->GetNativeDevice(), m_semaphore, nullptr);
        m_semaphore = VK_NULL_HANDLE;
    }
}

Rndr::Forge::Semaphore::Semaphore(Semaphore&& other) noexcept
    : m_semaphore(other.m_semaphore), m_device(std::move(other.m_device)), m_type(other.m_type)
{
    other.m_device = nullptr;
    other.m_semaphore = VK_NULL_HANDLE;
    other.m_type = SemaphoreType::Binary;
}

Rndr::Forge::Semaphore& Rndr::Forge::Semaphore::operator=(Semaphore&& other) noexcept
{
    if (this != &other)
    {
        // Destroy first, as Fence::operator= above already does: without it, assigning over a live semaphore
        // drops the handle it holds and leaks it.
        Destroy();
        m_device = std::move(other.m_device);
        m_semaphore = other.m_semaphore;
        m_type = other.m_type;
        other.m_semaphore = VK_NULL_HANDLE;
        other.m_device = nullptr;
        other.m_type = SemaphoreType::Binary;
    }
    return *this;
}

/** The host side of a timeline is undefined on a binary semaphore, so every one of them is turned away. */
static void ThrowIfNotTimeline(const Rndr::Forge::Semaphore& semaphore, const char* what)
{
    if (!semaphore.IsValid())
    {
        throw Opal::Exception(Opal::StringEx(what) + " an empty semaphore!");
    }
    if (!semaphore.IsTimeline())
    {
        throw Opal::Exception(Opal::StringEx(what) + " a binary semaphore, which only a device wait can consume!");
    }
}

void Rndr::Forge::Semaphore::Wait(u64 value) const
{
    // An infinite timeout cannot expire, so the answer is always true and there is nothing to report.
    (void)TryWait(value, k_infinite_wait);
}

bool Rndr::Forge::Semaphore::TryWait(u64 value, u64 timeout) const
{
    ThrowIfNotTimeline(*this, "Waiting on");
    const VkSemaphoreWaitInfo wait_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, .semaphoreCount = 1, .pSemaphores = &m_semaphore, .pValues = &value};
    const VkResult result = vkWaitSemaphores(m_device->GetNativeDevice(), &wait_info, timeout);
    // VK_TIMEOUT is a success code here as well, for the reason Fence::TryWait gives above.
    if (result == VK_TIMEOUT)
    {
        return false;
    }
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkWaitSemaphores");
    }
    return true;
}

void Rndr::Forge::Semaphore::Signal(u64 value) const
{
    ThrowIfNotTimeline(*this, "Signalling");
    // A signal has to leave the count above where it was. A device signal landing between this read and the
    // call below would slip past, which no check on this side can close, but the mistake worth catching is
    // the host signalling backwards - and Vulkan reports that one late, if at all.
    if (value <= GetValue())
    {
        throw Opal::Exception("A timeline can only be signalled above the count it already holds!");
    }
    const VkSemaphoreSignalInfo signal_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, .semaphore = m_semaphore, .value = value};
    const VkResult result = vkSignalSemaphore(m_device->GetNativeDevice(), &signal_info);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkSignalSemaphore");
    }
}

Rndr::u64 Rndr::Forge::Semaphore::GetValue() const
{
    ThrowIfNotTimeline(*this, "Reading the count of");
    u64 value = 0;
    const VkResult result = vkGetSemaphoreCounterValue(m_device->GetNativeDevice(), m_semaphore, &value);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkGetSemaphoreCounterValue");
    }
    return value;
}

void Rndr::Forge::Semaphore::WaitForAll(Opal::ArrayView<const SemaphoreWait> waits)
{
    (void)TryWaitForAll(waits, k_infinite_wait);
}

bool Rndr::Forge::Semaphore::TryWaitForAll(Opal::ArrayView<const SemaphoreWait> waits, u64 timeout)
{
    if (waits.empty())
    {
        return true;
    }
    // The default allocator rather than the scratch one, for the reason Fence::WaitForAll gives above.
    // vkWaitSemaphores wants the semaphores and their values as two parallel arrays.
    Opal::DynamicArray<VkSemaphore> native_semaphores(waits.GetSize());
    Opal::DynamicArray<u64> values(waits.GetSize());
    for (i32 i = 0; i < waits.GetSize(); ++i)
    {
        const SemaphoreWait& wait = waits[i];
        // Two checks rather than the one a fence needs: the entry holds a reference, so the reference can be
        // empty and so can the semaphore behind it.
        if (!wait.semaphore.IsValid() || !wait.semaphore->IsValid())
        {
            throw Opal::Exception("Waiting on an empty semaphore!");
        }
        if (!wait.semaphore->IsTimeline())
        {
            throw Opal::Exception("Waiting on a binary semaphore, which only a device wait can consume!");
        }
        // One wait is one call into one device, and the call below can only name the first semaphore's.
        // Reading that one is safe from the second entry on, since the first has been through the checks above.
        if (i > 0 && wait.semaphore->m_device->GetNativeDevice() != waits[0].semaphore->m_device->GetNativeDevice())
        {
            throw Opal::Exception("Waiting on semaphores from more than one device at once!");
        }
        native_semaphores[i] = wait.semaphore->GetNativeSemaphore();
        values[i] = wait.value;
    }
    const VkSemaphoreWaitInfo wait_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                                           .semaphoreCount = static_cast<u32>(native_semaphores.GetSize()),
                                           .pSemaphores = native_semaphores.GetData(),
                                           .pValues = values.GetData()};
    const VkResult result = vkWaitSemaphores(waits[0].semaphore->m_device->GetNativeDevice(), &wait_info, timeout);
    if (result == VK_TIMEOUT)
    {
        return false;
    }
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkWaitSemaphores");
    }
    return true;
}
