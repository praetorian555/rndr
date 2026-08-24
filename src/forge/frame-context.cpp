#include "rndr/forge/frame-context.hpp"

#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-exception.hpp"

Rndr::Forge::FrameContext::FrameContext(const Device& device, SwapChain& swap_chain, DeviceQueue& graphics_queue,
                                        DeviceQueue& present_queue, const FrameContextDesc& desc)
    : m_desc(desc), m_device(device), m_swap_chain(swap_chain), m_graphics_queue(graphics_queue), m_present_queue(present_queue)
{
    // Below one rather than zero: a negative count used to be survivable by accident, since the loop below
    // never ran and left every array empty, but the timeline is not built in that loop.
    if (m_desc.frames_in_flight < 1)
    {
        throw Opal::Exception("A frame context needs at least one frame in flight!");
    }
    // Starting at the number of frames in flight is what replaces a fence per slot created signaled: the
    // first frames_in_flight frames wait for a value at or below it, so none of them waits for work that was
    // never submitted.
    Opal::Expected<Semaphore, ErrorCode> timeline =
        Semaphore::Create(device, {.type = SemaphoreType::Timeline, .initial_value = static_cast<u64>(m_desc.frames_in_flight)});
    if (!timeline.HasValue())
    {
        throw Opal::Exception("The frame timeline could not be created!");
    }
    m_frame_timeline = std::move(timeline.GetValue());
    for (i32 frame = 0; frame < m_desc.frames_in_flight; ++frame)
    {
        Opal::Expected<Semaphore, ErrorCode> texture_ready = Semaphore::Create(device);
        Opal::Expected<CommandBuffer, ErrorCode> command_buffer = CommandBuffer::Create(device, graphics_queue);
        if (!texture_ready.HasValue() || !command_buffer.HasValue())
        {
            throw Opal::Exception("A frame slot could not be created!");
        }
        m_texture_ready_semaphores.EmplaceBack(std::move(texture_ready.GetValue()));
        m_command_buffers.EmplaceBack(std::move(command_buffer.GetValue()));
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
      m_frame_timeline(std::move(other.m_frame_timeline)),
      m_texture_ready_semaphores(std::move(other.m_texture_ready_semaphores)),
      m_command_buffers(std::move(other.m_command_buffers)),
      m_render_finished_semaphores(std::move(other.m_render_finished_semaphores)),
      m_frames_submitted(other.m_frames_submitted),
      m_is_frame_recording(other.m_is_frame_recording)
{
    other.m_device = nullptr;
    other.m_swap_chain = nullptr;
    other.m_graphics_queue = nullptr;
    other.m_present_queue = nullptr;
    // A moved-from Semaphore is already empty, so m_frame_timeline needs nothing here.
    other.m_texture_ready_semaphores.Clear();
    other.m_command_buffers.Clear();
    other.m_render_finished_semaphores.Clear();
    other.m_frames_submitted = 0;
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
        m_frame_timeline = std::move(other.m_frame_timeline);
        m_texture_ready_semaphores = std::move(other.m_texture_ready_semaphores);
        m_command_buffers = std::move(other.m_command_buffers);
        m_render_finished_semaphores = std::move(other.m_render_finished_semaphores);
        m_frames_submitted = other.m_frames_submitted;
        m_is_frame_recording = other.m_is_frame_recording;
        other.m_device = nullptr;
        other.m_swap_chain = nullptr;
        other.m_graphics_queue = nullptr;
        other.m_present_queue = nullptr;
        other.m_texture_ready_semaphores.Clear();
        other.m_command_buffers.Clear();
        other.m_render_finished_semaphores.Clear();
        other.m_frames_submitted = 0;
        other.m_is_frame_recording = false;
    }
    return *this;
}

void Rndr::Forge::FrameContext::Destroy()
{
    if (m_device.IsValid() && m_frame_timeline.IsValid())
    {
        // Frames may still be in flight, and every object below is one the device could still be reading.
        // A wait that fails has already logged why, and there is nothing else teardown can do about it.
        (void)m_device->WaitForAll();
    }
    m_command_buffers.Clear();
    m_render_finished_semaphores.Clear();
    m_texture_ready_semaphores.Clear();
    m_frame_timeline.Destroy();
    m_device = nullptr;
    m_swap_chain = nullptr;
    m_graphics_queue = nullptr;
    m_present_queue = nullptr;
    m_frames_submitted = 0;
    m_is_frame_recording = false;
}

void Rndr::Forge::FrameContext::MatchRenderSemaphoresToSwapChain()
{
    const u32 texture_count = m_swap_chain->GetColorTextureCount();
    if (static_cast<u32>(m_render_finished_semaphores.GetSize()) == texture_count)
    {
        return;
    }
    m_render_finished_semaphores.Clear();
    for (u32 texture = 0; texture < texture_count; ++texture)
    {
        Opal::Expected<Semaphore, ErrorCode> render_finished = Semaphore::Create(*m_device);
        if (!render_finished.HasValue())
        {
            throw Opal::Exception("A render finished semaphore could not be created!");
        }
        m_render_finished_semaphores.EmplaceBack(std::move(render_finished.GetValue()));
    }
}

Rndr::Forge::SwapChainStatus Rndr::Forge::FrameContext::BeginFrame()
{
    if (m_is_frame_recording)
    {
        throw Opal::Exception("BeginFrame was called twice without an EndFrame in between!");
    }
    // Frame k waits for k + 1, which is what frame k - frames_in_flight signalled - the frame whose slot,
    // command buffer and texture-ready semaphore this one is about to reuse. Nothing to reset afterwards, so
    // there is no ordering question about where the reset goes either.
    if (m_frame_timeline.Wait(m_frames_submitted + 1) != ErrorCode::Success)
    {
        throw Opal::Exception("Waiting for the frame timeline failed!");
    }

    const u32 frame_index = GetFrameIndex();
    const AcquiredTexture acquired_texture = m_swap_chain->AcquireTexture(m_texture_ready_semaphores[frame_index]);
    if (acquired_texture.status == SwapChainStatus::OutOfDate)
    {
        // The swap chain was rebuilt, or the window has no client area and there is none. Nothing was submitted
        // for this frame, so the counter does not advance.
        MatchRenderSemaphoresToSwapChain();
        return SwapChainStatus::OutOfDate;
    }

    const CommandBuffer& command_buffer = m_command_buffers[frame_index];
    if (command_buffer.Reset() != ErrorCode::Success || command_buffer.Begin() != ErrorCode::Success)
    {
        throw Opal::Exception("The frame command buffer could not be reset and begun!");
    }
    m_is_frame_recording = true;
    return SwapChainStatus::Success;
}

Rndr::Forge::SwapChainStatus Rndr::Forge::FrameContext::EndFrame()
{
    if (!m_is_frame_recording)
    {
        throw Opal::Exception("EndFrame was called without a BeginFrame that succeeded!");
    }
    // The recording flag and the acquired texture are set together in BeginFrame, so this only fires when
    // something outside the frame context released the swap chain mid-frame. Above the texture is reached for
    // rather than below, since every use of it from here on reports that as a missing BeginFrame instead.
    if (!m_swap_chain->HasAcquiredTexture())
    {
        throw Opal::Exception("The swap chain gave up its acquired texture in the middle of a frame!");
    }
    const u32 frame_index = GetFrameIndex();
    CommandBuffer& command_buffer = m_command_buffers[frame_index];
    Texture& color_texture = GetColorTexture();
    const Opal::Expected<ImageLayout, ErrorCode> color_layout = color_texture.GetCurrentLayout();
    if (!color_layout.HasValue())
    {
        throw Opal::Exception("The swap chain texture is not all in one layout at the end of a frame!");
    }
    if (color_layout.GetValue() != ImageLayout::Present)
    {
        if (command_buffer.CmdTextureBarrier(TextureBarrier::ToPresent(color_texture)) != ErrorCode::Success)
        {
            throw Opal::Exception("The barrier into the present layout could not be recorded!");
        }
    }
    if (command_buffer.End() != ErrorCode::Success)
    {
        throw Opal::Exception("The frame command buffer could not be ended!");
    }
    m_is_frame_recording = false;

    const Semaphore& image_ready = m_texture_ready_semaphores[frame_index];
    const Semaphore& render_finished = m_render_finished_semaphores[static_cast<i32>(m_swap_chain->GetCurrentTextureIndex())];

    // Two signals, which the convenience overload cannot express: the binary one the present waits on, and the
    // timeline the frame reusing this slot waits on. Both at AllCommands - the host wait on the timeline is the
    // whole proof that this command buffer has stopped executing, so an earlier stage would hand the buffer back
    // while work behind that stage is still reading it.
    const Opal::Ref<const CommandBuffer> command_buffer_ref(command_buffer);
    const SemaphoreSubmit wait{.semaphore = image_ready, .stages = PipelineStageBits::ColorAttachmentOutput};
    const SemaphoreSubmit signals[2] = {
        {.semaphore = render_finished},
        {.semaphore = m_frame_timeline, .value = m_frames_submitted + 1 + static_cast<u64>(m_desc.frames_in_flight)}};
    const ErrorCode submit_status = m_graphics_queue->Submit(
        {.command_buffers = {&command_buffer_ref, 1}, .wait_semaphores = {&wait, 1}, .signal_semaphores = {signals, 2}});
    if (submit_status != ErrorCode::Success)
    {
        throw Opal::Exception("Submitting the frame failed!");
    }

    // Advanced before the present, so that a present that comes back out of date still leaves the next frame on
    // the following slot - the work of this one was submitted either way.
    ++m_frames_submitted;

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
    return m_command_buffers[GetFrameIndex()];
}

const Rndr::Forge::Texture& Rndr::Forge::FrameContext::GetColorTexture() const
{
    if (!m_swap_chain->HasAcquiredTexture())
    {
        throw Opal::Exception("There is no acquired texture outside of a frame - call BeginFrame first!");
    }
    return m_swap_chain->GetCurrentColorTexture();
}

Rndr::Forge::Texture& Rndr::Forge::FrameContext::GetColorTexture()
{
    if (!m_swap_chain->HasAcquiredTexture())
    {
        throw Opal::Exception("There is no acquired texture outside of a frame - call BeginFrame first!");
    }
    return m_swap_chain->GetCurrentColorTexture();
}

Rndr::Vector2i Rndr::Forge::FrameContext::GetRenderSize() const
{
    const VkExtent2D extent = m_swap_chain->GetExtent();
    return {static_cast<i32>(extent.width), static_cast<i32>(extent.height)};
}
