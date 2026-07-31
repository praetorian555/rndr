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

Rndr::Forge::Fence::Fence(Fence&& other) noexcept : m_device(std::move(other.m_device)), m_fence(other.m_fence)
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
    }
    return *this;
}

void Rndr::Forge::Fence::Wait(u64 timeout) const
{
    const VkResult result = vkWaitForFences(m_device->GetNativeDevice(), 1, &m_fence, VK_TRUE, timeout);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkWaitForFences");
    }
}

void Rndr::Forge::Fence::Reset() const
{
    const VkResult reset_result = vkResetFences(m_device->GetNativeDevice(), 1, &m_fence);
    if (reset_result != VK_SUCCESS)
    {
        throw VulkanException(reset_result, "vkResetFences");
    }
}

void Rndr::Forge::Fence::WaitForAll(Opal::ArrayView<const Fence> fences, u64 timeout)
{
    if (fences.empty())
    {
        return;
    }
    Opal::DynamicArray<VkFence> native_fences(fences.GetSize(), Opal::GetScratchAllocator());
    for (i32 i = 0; i < fences.GetSize(); ++i)
    {
        native_fences[i] = fences[i].GetNativeFence();
    }
    const VkResult wait_result =
        vkWaitForFences(fences[0].m_device->GetNativeDevice(), static_cast<u32>(native_fences.GetSize()), native_fences.GetData(), VK_TRUE, timeout);
    if (wait_result != VK_SUCCESS)
    {
        throw VulkanException(wait_result, "vkWaitForFences");
    }
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
            return {PipelineStageBits::ColorAttachmentOutput, PipelineStageAccessBits::Write};
        case ImageLayout::DepthStencilAttachment:
            return {PipelineStageBits::EarlyFragmentTests | PipelineStageBits::LateFragmentTests, PipelineStageAccessBits::Write};
        case ImageLayout::DepthStencilReadOnly:
        case ImageLayout::ShaderReadOnly:
            return {PipelineStageBits::FragmentShader, PipelineStageAccessBits::Read};
        case ImageLayout::TransferSource:
            return {PipelineStageBits::Transfer, PipelineStageAccessBits::Read};
        case ImageLayout::TransferDestination:
            return {PipelineStageBits::Transfer, PipelineStageAccessBits::Write};
        default:
            // General and Present, plus anything added later, can have been touched by anything.
            return {PipelineStageBits::AllCommands, PipelineStageAccessBits::Read | PipelineStageAccessBits::Write};
    }
}
}  // namespace

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToColorAttachment(const Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::ColorAttachmentOutput,
            .before_stages_start_access = PipelineStageAccessBits::Read | PipelineStageAccessBits::Write,
            .old_layout = old_layout,
            .new_layout = ImageLayout::ColorAttachment,
            .image = texture};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToDepthStencilAttachment(const Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::EarlyFragmentTests | PipelineStageBits::LateFragmentTests,
            .before_stages_start_access = PipelineStageAccessBits::Read | PipelineStageAccessBits::Write,
            .old_layout = old_layout,
            .new_layout = ImageLayout::DepthStencilAttachment,
            .image = texture};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToShaderRead(const Texture& texture, ImageLayout old_layout,
                                                                  PipelineStageBits reader)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = reader,
            .before_stages_start_access = PipelineStageAccessBits::Read,
            .old_layout = old_layout,
            .new_layout = ImageLayout::ShaderReadOnly,
            .image = texture};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToTransferDestination(const Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::Transfer,
            .before_stages_start_access = PipelineStageAccessBits::Write,
            .old_layout = old_layout,
            .new_layout = ImageLayout::TransferDestination,
            .image = texture};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToTransferSource(const Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::Transfer,
            .before_stages_start_access = PipelineStageAccessBits::Read,
            .old_layout = old_layout,
            .new_layout = ImageLayout::TransferSource,
            .image = texture};
}

Rndr::Forge::ImageBarrier Rndr::Forge::ImageBarrier::ToPresent(const Texture& texture, ImageLayout old_layout)
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

Rndr::Forge::Semaphore::Semaphore(const Device& device) : m_device(device)
{
    const VkSemaphoreCreateInfo semaphore_create_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
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
    : m_device(std::move(other.m_device)), m_semaphore(other.m_semaphore)
{
    other.m_device = nullptr;
    other.m_semaphore = VK_NULL_HANDLE;
}

Rndr::Forge::Semaphore& Rndr::Forge::Semaphore::operator=(Semaphore&& other) noexcept
{
    if (this != &other)
    {
        m_device = std::move(other.m_device);
        m_semaphore = other.m_semaphore;
        other.m_semaphore = VK_NULL_HANDLE;
    }
    return *this;
}
