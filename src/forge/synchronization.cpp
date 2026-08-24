#include "rndr/forge/synchronization.hpp"

#include <mutex>

#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/log.hpp"

Opal::Expected<Rndr::Forge::Fence, Rndr::ErrorCode> Rndr::Forge::Fence::Create(const Device& device, bool create_signaled)
{
    using Result = Opal::Expected<Fence, ErrorCode>;

    const VkFenceCreateInfo fence_create_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                                 .flags = create_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u};

    Fence fence;
    fence.m_device = device;
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateFence(device.GetNativeDevice(), &fence_create_info, nullptr, &fence.m_fence), "vkCreateFence",
                                 Result);
    return Result(std::move(fence));
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

Rndr::ErrorCode Rndr::Forge::Fence::Wait() const
{
    // An infinite timeout cannot expire, so the only thing this can report is a failed wait.
    const Opal::Expected<bool, ErrorCode> result = TryWait(k_infinite_wait);
    return result.HasValue() ? ErrorCode::Success : result.GetError();
}

Opal::Expected<bool, Rndr::ErrorCode> Rndr::Forge::Fence::TryWait(u64 timeout) const
{
    using Result = Opal::Expected<bool, ErrorCode>;

    const VkResult result = vkWaitForFences(m_device->GetNativeDevice(), 1, &m_fence, VK_TRUE, timeout);
    // VK_TIMEOUT is a success code: the wait did what it was told, the fence was simply not signalled in
    // time. Reporting it as a failure would leave the timeout parameter unusable for the one thing it exists for.
    if (result == VK_TIMEOUT)
    {
        return Result(false);
    }
    if (result != VK_SUCCESS)
    {
        RNDR_LOG_ERROR("Forge: {} failed: {}", "vkWaitForFences", VkResultToString(result));
        return Result(VkResultToErrorCode(result));
    }
    return Result(true);
}

Rndr::ErrorCode Rndr::Forge::Fence::Reset() const
{
    RNDR_FORGE_VK_CHECK(vkResetFences(m_device->GetNativeDevice(), 1, &m_fence), "vkResetFences");
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::Fence::WaitForAll(Opal::ArrayView<const Fence> fences)
{
    const Opal::Expected<bool, ErrorCode> result = TryWaitForAll(fences, k_infinite_wait);
    return result.HasValue() ? ErrorCode::Success : result.GetError();
}

Opal::Expected<bool, Rndr::ErrorCode> Rndr::Forge::Fence::TryWaitForAll(Opal::ArrayView<const Fence> fences, u64 timeout)
{
    using Result = Opal::Expected<bool, ErrorCode>;

    if (fences.empty())
    {
        return Result(true);
    }
    // The default allocator rather than the scratch one: asking Opal for a scratch allocator asserts unless
    // the application pushed one, and nothing in Forge does, so this asserted on the first call it ever got.
    Opal::DynamicArray<VkFence> native_fences(fences.GetSize());
    for (i32 i = 0; i < fences.GetSize(); ++i)
    {
        const Fence& fence = fences[i];
        if (!fence.IsValid())
        {
            RNDR_LOG_ERROR("Forge: waiting on an empty fence");
            return Result(ErrorCode::InvalidArgument);
        }
        // One wait is one call into one device, and the call below can only name the first fence's. Reading
        // that one is safe from the second entry on, since the first has been through the check above.
        if (i > 0 && fence.m_device->GetNativeDevice() != fences[0].m_device->GetNativeDevice())
        {
            RNDR_LOG_ERROR("Forge: waiting on fences from more than one device at once");
            return Result(ErrorCode::InvalidArgument);
        }
        native_fences[i] = fence.GetNativeFence();
    }
    const VkResult wait_result = vkWaitForFences(fences[0].m_device->GetNativeDevice(), static_cast<u32>(native_fences.GetSize()),
                                                 native_fences.GetData(), VK_TRUE, timeout);
    if (wait_result == VK_TIMEOUT)
    {
        return Result(false);
    }
    if (wait_result != VK_SUCCESS)
    {
        RNDR_LOG_ERROR("Forge: {} failed: {}", "vkWaitForFences", VkResultToString(wait_result));
        return Result(VkResultToErrorCode(wait_result));
    }
    return Result(true);
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

Rndr::Forge::TextureBarrier Rndr::Forge::TextureBarrier::ToColorAttachment(Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::ColorAttachmentOutput,
            .before_stages_start_access = PipelineStageAccessBits::ColorAttachmentRead | PipelineStageAccessBits::ColorAttachmentWrite,
            .old_layout = old_layout,
            .new_layout = ImageLayout::ColorAttachment,
            .texture = texture};
}

Rndr::Forge::TextureBarrier Rndr::Forge::TextureBarrier::ToDepthStencilAttachment(Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::EarlyFragmentTests | PipelineStageBits::LateFragmentTests,
            .before_stages_start_access =
                PipelineStageAccessBits::DepthStencilAttachmentRead | PipelineStageAccessBits::DepthStencilAttachmentWrite,
            .old_layout = old_layout,
            .new_layout = ImageLayout::DepthStencilAttachment,
            .texture = texture};
}

Rndr::Forge::TextureBarrier Rndr::Forge::TextureBarrier::ToShaderRead(Texture& texture, ImageLayout old_layout, PipelineStageBits reader)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = reader,
            .before_stages_start_access = PipelineStageAccessBits::ShaderRead,
            .old_layout = old_layout,
            .new_layout = ImageLayout::ShaderReadOnly,
            .texture = texture};
}

Rndr::Forge::TextureBarrier Rndr::Forge::TextureBarrier::ToGeneral(Texture& texture, ImageLayout old_layout, PipelineStageBits accessor)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    // Both sides of the access, because General is the layout for a texture a stage reads and writes at once,
    // which is the whole reason to be in it rather than in one of the narrow ones.
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = accessor,
            .before_stages_start_access = PipelineStageAccessBits::ShaderRead | PipelineStageAccessBits::ShaderWrite,
            .old_layout = old_layout,
            .new_layout = ImageLayout::General,
            .texture = texture};
}

/** The layout the one-argument presets below leave, or what reading it off the texture reported. */
static Opal::Expected<Rndr::Forge::TextureBarrier, Rndr::ErrorCode> FromCurrentLayout(
    Rndr::Forge::Texture& texture, Rndr::Forge::TextureBarrier (*make)(Rndr::Forge::Texture&, Rndr::Forge::ImageLayout))
{
    using Result = Opal::Expected<Rndr::Forge::TextureBarrier, Rndr::ErrorCode>;

    const Opal::Expected<Rndr::Forge::ImageLayout, Rndr::ErrorCode> old_layout = texture.GetCurrentLayout();
    if (!old_layout.HasValue())
    {
        return Result(old_layout.GetError());
    }
    return Result(make(texture, old_layout.GetValue()));
}

Opal::Expected<Rndr::Forge::TextureBarrier, Rndr::ErrorCode> Rndr::Forge::TextureBarrier::ToGeneral(Texture& texture,
                                                                                                    PipelineStageBits accessor)
{
    using Result = Opal::Expected<TextureBarrier, ErrorCode>;

    const Opal::Expected<ImageLayout, ErrorCode> old_layout = texture.GetCurrentLayout();
    if (!old_layout.HasValue())
    {
        return Result(old_layout.GetError());
    }
    return Result(ToGeneral(texture, old_layout.GetValue(), accessor));
}

Rndr::Forge::TextureBarrier Rndr::Forge::TextureBarrier::ToTransferDestination(Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::Transfer,
            .before_stages_start_access = PipelineStageAccessBits::TransferWrite,
            .old_layout = old_layout,
            .new_layout = ImageLayout::TransferDestination,
            .texture = texture};
}

Rndr::Forge::TextureBarrier Rndr::Forge::TextureBarrier::ToTransferSource(Texture& texture, ImageLayout old_layout)
{
    const SourceScope source = ScopeOfLayout(old_layout);
    return {.stages_must_finish = source.stages,
            .stages_must_finish_access = source.access,
            .before_stages_start = PipelineStageBits::Transfer,
            .before_stages_start_access = PipelineStageAccessBits::TransferRead,
            .old_layout = old_layout,
            .new_layout = ImageLayout::TransferSource,
            .texture = texture};
}

Rndr::Forge::TextureBarrier Rndr::Forge::TextureBarrier::ToPresent(Texture& texture, ImageLayout old_layout)
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
            .texture = texture};
}

Opal::Expected<Rndr::Forge::TextureBarrier, Rndr::ErrorCode> Rndr::Forge::TextureBarrier::ToColorAttachment(Texture& texture)
{
    return FromCurrentLayout(texture, [](Texture& t, ImageLayout layout) { return ToColorAttachment(t, layout); });
}

Opal::Expected<Rndr::Forge::TextureBarrier, Rndr::ErrorCode> Rndr::Forge::TextureBarrier::ToDepthStencilAttachment(Texture& texture)
{
    return FromCurrentLayout(texture, [](Texture& t, ImageLayout layout) { return ToDepthStencilAttachment(t, layout); });
}

Opal::Expected<Rndr::Forge::TextureBarrier, Rndr::ErrorCode> Rndr::Forge::TextureBarrier::ToShaderRead(Texture& texture,
                                                                                                       PipelineStageBits reader)
{
    using Result = Opal::Expected<TextureBarrier, ErrorCode>;

    const Opal::Expected<ImageLayout, ErrorCode> old_layout = texture.GetCurrentLayout();
    if (!old_layout.HasValue())
    {
        return Result(old_layout.GetError());
    }
    return Result(ToShaderRead(texture, old_layout.GetValue(), reader));
}

Opal::Expected<Rndr::Forge::TextureBarrier, Rndr::ErrorCode> Rndr::Forge::TextureBarrier::ToTransferDestination(Texture& texture)
{
    return FromCurrentLayout(texture, [](Texture& t, ImageLayout layout) { return ToTransferDestination(t, layout); });
}

Opal::Expected<Rndr::Forge::TextureBarrier, Rndr::ErrorCode> Rndr::Forge::TextureBarrier::ToTransferSource(Texture& texture)
{
    return FromCurrentLayout(texture, [](Texture& t, ImageLayout layout) { return ToTransferSource(t, layout); });
}

Opal::Expected<Rndr::Forge::TextureBarrier, Rndr::ErrorCode> Rndr::Forge::TextureBarrier::ToPresent(Texture& texture)
{
    return FromCurrentLayout(texture, [](Texture& t, ImageLayout layout) { return ToPresent(t, layout); });
}

Opal::Expected<Rndr::Forge::TextureBarrier, Rndr::ErrorCode> Rndr::Forge::TextureBarrier::To(Texture& texture, ImageLayout old_layout,
                                                                                             ImageLayout new_layout)
{
    using Result = Opal::Expected<TextureBarrier, ErrorCode>;

    switch (new_layout)
    {
        case ImageLayout::ShaderReadOnly:
            return Result(ToShaderRead(texture, old_layout));
        case ImageLayout::ColorAttachment:
            return Result(ToColorAttachment(texture, old_layout));
        case ImageLayout::DepthStencilAttachment:
            return Result(ToDepthStencilAttachment(texture, old_layout));
        case ImageLayout::TransferSource:
            return Result(ToTransferSource(texture, old_layout));
        case ImageLayout::TransferDestination:
            return Result(ToTransferDestination(texture, old_layout));
        case ImageLayout::General:
            return Result(ToGeneral(texture, old_layout));
        case ImageLayout::Present:
            return Result(ToPresent(texture, old_layout));
        default:
            RNDR_LOG_ERROR("Forge: no barrier preset transitions a texture into {}", ImageLayoutToString(new_layout));
            return Result(ErrorCode::InvalidArgument);
    }
}

Opal::Expected<Rndr::Forge::Semaphore, Rndr::ErrorCode> Rndr::Forge::Semaphore::Create(const Device& device, const SemaphoreDesc& desc)
{
    using Result = Opal::Expected<Semaphore, ErrorCode>;

    // The type lives in a chained structure rather than in the create info, so a binary semaphore is the one
    // that chains nothing - which is also why it is what a defaulted desc asks for.
    const VkSemaphoreTypeCreateInfo type_create_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                                        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                                                        .initialValue = desc.initial_value};
    const VkSemaphoreCreateInfo semaphore_create_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                                         .pNext = desc.type == SemaphoreType::Timeline ? &type_create_info : nullptr};
    Semaphore semaphore;
    semaphore.m_device = device;
    semaphore.m_type = desc.type;
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateSemaphore(device.GetNativeDevice(), &semaphore_create_info, nullptr, &semaphore.m_semaphore),
                                 "vkCreateSemaphore", Result);
    return Result(std::move(semaphore));
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

/** The host side of a timeline does nothing on a binary semaphore, so every one of them is turned away. */
static Rndr::ErrorCode RequireTimeline(const Rndr::Forge::Semaphore& semaphore, const char* what)
{
    if (!semaphore.IsValid())
    {
        RNDR_LOG_ERROR("Forge: {} an empty semaphore", what);
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (!semaphore.IsTimeline())
    {
        RNDR_LOG_ERROR("Forge: {} a binary semaphore, which only a device wait can consume", what);
        return Rndr::ErrorCode::InvalidArgument;
    }
    return Rndr::ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::Semaphore::Wait(u64 value) const
{
    // An infinite timeout cannot expire, so the only thing this can report is a failed wait.
    const Opal::Expected<bool, ErrorCode> result = TryWait(value, k_infinite_wait);
    return result.HasValue() ? ErrorCode::Success : result.GetError();
}

Opal::Expected<bool, Rndr::ErrorCode> Rndr::Forge::Semaphore::TryWait(u64 value, u64 timeout) const
{
    using Result = Opal::Expected<bool, ErrorCode>;

    const ErrorCode timeline_status = RequireTimeline(*this, "waiting on");
    if (timeline_status != ErrorCode::Success)
    {
        return Result(timeline_status);
    }
    const VkSemaphoreWaitInfo wait_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, .semaphoreCount = 1, .pSemaphores = &m_semaphore, .pValues = &value};
    const VkResult result = vkWaitSemaphores(m_device->GetNativeDevice(), &wait_info, timeout);
    // VK_TIMEOUT is a success code here as well, for the reason Fence::TryWait gives above.
    if (result == VK_TIMEOUT)
    {
        return Result(false);
    }
    if (result != VK_SUCCESS)
    {
        RNDR_LOG_ERROR("Forge: {} failed: {}", "vkWaitSemaphores", VkResultToString(result));
        return Result(VkResultToErrorCode(result));
    }
    return Result(true);
}

Rndr::ErrorCode Rndr::Forge::Semaphore::Signal(u64 value) const
{
    const ErrorCode timeline_status = RequireTimeline(*this, "signalling");
    if (timeline_status != ErrorCode::Success)
    {
        return timeline_status;
    }
    // A signal has to leave the count above where it was. A device signal landing between this read and the
    // call below would slip past, which no check on this side can close, but the mistake worth catching is
    // the host signalling backwards - and Vulkan reports that one late, if at all.
    const Opal::Expected<u64, ErrorCode> current = GetValue();
    if (!current.HasValue())
    {
        return current.GetError();
    }
    if (value <= current.GetValue())
    {
        RNDR_LOG_ERROR("Forge: a timeline holding {} can only be signalled above that, not with {}", current.GetValue(), value);
        return ErrorCode::InvalidArgument;
    }
    const VkSemaphoreSignalInfo signal_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, .semaphore = m_semaphore, .value = value};
    RNDR_FORGE_VK_CHECK(vkSignalSemaphore(m_device->GetNativeDevice(), &signal_info), "vkSignalSemaphore");
    return ErrorCode::Success;
}

Opal::Expected<Rndr::u64, Rndr::ErrorCode> Rndr::Forge::Semaphore::GetValue() const
{
    using Result = Opal::Expected<u64, ErrorCode>;

    const ErrorCode timeline_status = RequireTimeline(*this, "reading the count of");
    if (timeline_status != ErrorCode::Success)
    {
        return Result(timeline_status);
    }
    u64 value = 0;
    RNDR_FORGE_VK_CHECK_EXPECTED(vkGetSemaphoreCounterValue(m_device->GetNativeDevice(), m_semaphore, &value), "vkGetSemaphoreCounterValue",
                                 Result);
    return Result(value);
}

Rndr::ErrorCode Rndr::Forge::Semaphore::WaitForAll(Opal::ArrayView<const SemaphoreWait> waits)
{
    const Opal::Expected<bool, ErrorCode> result = TryWaitForAll(waits, k_infinite_wait);
    return result.HasValue() ? ErrorCode::Success : result.GetError();
}

Opal::Expected<bool, Rndr::ErrorCode> Rndr::Forge::Semaphore::TryWaitForAll(Opal::ArrayView<const SemaphoreWait> waits, u64 timeout)
{
    using Result = Opal::Expected<bool, ErrorCode>;

    if (waits.empty())
    {
        return Result(true);
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
            RNDR_LOG_ERROR("Forge: waiting on an empty semaphore");
            return Result(ErrorCode::InvalidArgument);
        }
        if (!wait.semaphore->IsTimeline())
        {
            RNDR_LOG_ERROR("Forge: waiting on a binary semaphore, which only a device wait can consume");
            return Result(ErrorCode::InvalidArgument);
        }
        // One wait is one call into one device, and the call below can only name the first semaphore's.
        // Reading that one is safe from the second entry on, since the first has been through the checks above.
        if (i > 0 && wait.semaphore->m_device->GetNativeDevice() != waits[0].semaphore->m_device->GetNativeDevice())
        {
            RNDR_LOG_ERROR("Forge: waiting on semaphores from more than one device at once");
            return Result(ErrorCode::InvalidArgument);
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
        return Result(false);
    }
    if (result != VK_SUCCESS)
    {
        RNDR_LOG_ERROR("Forge: {} failed: {}", "vkWaitSemaphores", VkResultToString(result));
        return Result(VkResultToErrorCode(result));
    }
    return Result(true);
}
