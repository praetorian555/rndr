#include "rndr/forge/frame-context.hpp"

#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-exception.hpp"

Rndr::Forge::FrameContext::FrameContext(const Device& device, SwapChain& swap_chain, DeviceQueue& graphics_queue,
                                        DeviceQueue& present_queue, const FrameContextDesc& desc)
    : m_desc(desc), m_device(device), m_swap_chain(swap_chain), m_graphics_queue(graphics_queue), m_present_queue(present_queue)
{
    if (m_desc.frames_in_flight == 0)
    {
        throw Opal::Exception("A frame context needs at least one frame in flight!");
    }
    for (i32 frame = 0; frame < m_desc.frames_in_flight; ++frame)
    {
        // Signaled, so that the first BeginFrame does not wait for work that was never submitted.
        constexpr bool k_start_signaled = true;
        m_fences.EmplaceBack(device, k_start_signaled);
        m_image_ready_semaphores.EmplaceBack(device);
        m_command_buffers.EmplaceBack(device, graphics_queue);
    }
    MatchRenderSemaphoresToSwapChain();
}

Rndr::Forge::FrameContext::~FrameContext()
{
    Destroy();
}

Rndr::Forge::FrameContext::FrameContext(FrameContext&& other) noexcept
    : m_desc(other.m_desc),
      m_device(std::move(other.m_device)),
      m_swap_chain(std::move(other.m_swap_chain)),
      m_graphics_queue(std::move(other.m_graphics_queue)),
      m_present_queue(std::move(other.m_present_queue)),
      m_fences(std::move(other.m_fences)),
      m_image_ready_semaphores(std::move(other.m_image_ready_semaphores)),
      m_command_buffers(std::move(other.m_command_buffers)),
      m_render_finished_semaphores(std::move(other.m_render_finished_semaphores)),
      m_frame_index(other.m_frame_index),
      m_is_frame_recording(other.m_is_frame_recording)
{
    other.m_device = nullptr;
    other.m_swap_chain = nullptr;
    other.m_graphics_queue = nullptr;
    other.m_present_queue = nullptr;
    other.m_fences.Clear();
    other.m_image_ready_semaphores.Clear();
    other.m_command_buffers.Clear();
    other.m_render_finished_semaphores.Clear();
    other.m_frame_index = 0;
    other.m_is_frame_recording = false;
}

Rndr::Forge::FrameContext& Rndr::Forge::FrameContext::operator=(FrameContext&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_device = std::move(other.m_device);
        m_swap_chain = std::move(other.m_swap_chain);
        m_graphics_queue = std::move(other.m_graphics_queue);
        m_present_queue = std::move(other.m_present_queue);
        m_fences = std::move(other.m_fences);
        m_image_ready_semaphores = std::move(other.m_image_ready_semaphores);
        m_command_buffers = std::move(other.m_command_buffers);
        m_render_finished_semaphores = std::move(other.m_render_finished_semaphores);
        m_frame_index = other.m_frame_index;
        m_is_frame_recording = other.m_is_frame_recording;
        other.m_device = nullptr;
        other.m_swap_chain = nullptr;
        other.m_graphics_queue = nullptr;
        other.m_present_queue = nullptr;
        other.m_fences.Clear();
        other.m_image_ready_semaphores.Clear();
        other.m_command_buffers.Clear();
        other.m_render_finished_semaphores.Clear();
        other.m_frame_index = 0;
        other.m_is_frame_recording = false;
    }
    return *this;
}

void Rndr::Forge::FrameContext::Destroy()
{
    if (m_device.IsValid() && !m_fences.IsEmpty())
    {
        // Frames may still be in flight, and every object below is one the device could still be reading.
        m_device->WaitForAll();
    }
    m_command_buffers.Clear();
    m_render_finished_semaphores.Clear();
    m_image_ready_semaphores.Clear();
    m_fences.Clear();
    m_device = nullptr;
    m_swap_chain = nullptr;
    m_graphics_queue = nullptr;
    m_present_queue = nullptr;
    m_frame_index = 0;
    m_is_frame_recording = false;
}

void Rndr::Forge::FrameContext::MatchRenderSemaphoresToSwapChain()
{
    const u32 image_count = m_swap_chain->GetColorImageCount();
    if (static_cast<u32>(m_render_finished_semaphores.GetSize()) == image_count)
    {
        return;
    }
    m_render_finished_semaphores.Clear();
    for (u32 image = 0; image < image_count; ++image)
    {
        m_render_finished_semaphores.EmplaceBack(*m_device);
    }
}

Rndr::Forge::SwapChainStatus Rndr::Forge::FrameContext::BeginFrame()
{
    if (m_is_frame_recording)
    {
        throw Opal::Exception("BeginFrame was called twice without an EndFrame in between!");
    }
    const Fence& fence = m_fences[m_frame_index];
    fence.Wait();

    const AcquiredImage acquired_image = m_swap_chain->AcquireImage(m_image_ready_semaphores[m_frame_index]);
    if (acquired_image.status == SwapChainStatus::OutOfDate)
    {
        // The swap chain was rebuilt, or the window has no client area and there is none. Nothing was submitted
        // for this frame, so the fence stays signaled and the frame index does not advance.
        MatchRenderSemaphoresToSwapChain();
        return SwapChainStatus::OutOfDate;
    }
    fence.Reset();

    const CommandBuffer& command_buffer = m_command_buffers[m_frame_index];
    command_buffer.Reset();
    command_buffer.Begin();
    m_is_frame_recording = true;
    return SwapChainStatus::Success;
}

Rndr::Forge::SwapChainStatus Rndr::Forge::FrameContext::EndFrame(ImageLayout color_image_layout)
{
    if (!m_is_frame_recording)
    {
        throw Opal::Exception("EndFrame was called without a BeginFrame that succeeded!");
    }
    CommandBuffer& command_buffer = m_command_buffers[m_frame_index];
    if (color_image_layout != ImageLayout::Present)
    {
        command_buffer.CmdImageBarrier(ImageBarrier::ToPresent(GetColorImage(), color_image_layout));
    }
    command_buffer.End();
    m_is_frame_recording = false;

    // The recording flag and the acquired image are set together in BeginFrame, so this only fires when
    // something outside the frame context released the swap chain mid-frame. Cheaper to say so than to
    // index the semaphore array with k_invalid_image_index.
    if (!m_swap_chain->HasAcquiredImage())
    {
        throw Opal::Exception("The swap chain gave up its acquired image in the middle of a frame!");
    }
    const Semaphore& image_ready = m_image_ready_semaphores[m_frame_index];
    const Semaphore& render_finished = m_render_finished_semaphores[static_cast<i32>(m_swap_chain->GetCurrentImageIndex())];
    m_graphics_queue->Submit(command_buffer, image_ready, PipelineStageBits::ColorAttachmentOutput, render_finished,
                             m_fences[m_frame_index]);

    // Advanced before the present, so that a present that comes back out of date still leaves the next frame on
    // the following slot - the work of this one was submitted either way.
    m_frame_index = (m_frame_index + 1) % m_desc.frames_in_flight;

    const SwapChainStatus status = m_swap_chain->Present(*m_present_queue, render_finished);
    if (status == SwapChainStatus::OutOfDate)
    {
        MatchRenderSemaphoresToSwapChain();
    }
    return status;
}

Rndr::Forge::CommandBuffer& Rndr::Forge::FrameContext::GetCommandBuffer()
{
    if (!m_is_frame_recording)
    {
        throw Opal::Exception("There is no command buffer outside of a frame - call BeginFrame first!");
    }
    return m_command_buffers[m_frame_index];
}

const Rndr::Forge::Texture& Rndr::Forge::FrameContext::GetColorImage() const
{
    if (!m_swap_chain->HasAcquiredImage())
    {
        throw Opal::Exception("There is no acquired image outside of a frame - call BeginFrame first!");
    }
    return m_swap_chain->GetCurrentColorImage();
}

VkImageView Rndr::Forge::FrameContext::GetColorImageView() const
{
    return GetColorImage().GetNativeImageView();
}

Rndr::Vector2i Rndr::Forge::FrameContext::GetRenderSize() const
{
    const VkExtent2D extent = m_swap_chain->GetExtent();
    return {static_cast<i32>(extent.width), static_cast<i32>(extent.height)};
}
