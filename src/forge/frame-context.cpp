#include "rndr/forge/frame-context.hpp"

#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/log.hpp"

Opal::Expected<Rndr::Forge::FrameContext, Rndr::ErrorCode> Rndr::Forge::FrameContext::Create(const Device& device, SwapChain& swap_chain,
                                                                                             DeviceQueue& graphics_queue,
                                                                                             DeviceQueue& present_queue,
                                                                                             const FrameContextDesc& desc)
{
    using Result = Opal::Expected<FrameContext, ErrorCode>;

    // Below one rather than zero: a negative count used to be survivable by accident, since the loop below
    // never ran and left every array empty, but the timeline is not built in that loop.
    if (desc.frames_in_flight < 1)
    {
        RNDR_LOG_ERROR("Forge: a frame context needs at least one frame in flight, not {}", desc.frames_in_flight);
        return Result(ErrorCode::InvalidArgument);
    }

    FrameContext frame_context;
    frame_context.m_desc = desc;
    frame_context.m_device = device;
    frame_context.m_swap_chain = swap_chain;
    frame_context.m_graphics_queue = graphics_queue;
    frame_context.m_present_queue = present_queue;

    // Starting at the number of frames in flight is what replaces a fence per slot created signaled: the
    // first frames_in_flight frames wait for a value at or below it, so none of them waits for work that was
    // never submitted.
    Opal::Expected<Semaphore, ErrorCode> timeline =
        Semaphore::Create(device, {.type = SemaphoreType::Timeline, .initial_value = static_cast<u64>(desc.frames_in_flight)});
    if (!timeline.HasValue())
    {
        return Result(timeline.GetError());
    }
    frame_context.m_frame_timeline = std::move(timeline.GetValue());
    for (i32 frame = 0; frame < desc.frames_in_flight; ++frame)
    {
        Opal::Expected<Semaphore, ErrorCode> texture_ready = Semaphore::Create(device);
        if (!texture_ready.HasValue())
        {
            return Result(texture_ready.GetError());
        }
        Opal::Expected<CommandBuffer, ErrorCode> command_buffer = CommandBuffer::Create(device, graphics_queue);
        if (!command_buffer.HasValue())
        {
            return Result(command_buffer.GetError());
        }
        frame_context.m_texture_ready_semaphores.EmplaceBack(std::move(texture_ready.GetValue()));
        frame_context.m_command_buffers.EmplaceBack(std::move(command_buffer.GetValue()));
    }
    RNDR_FORGE_CHECK_EXPECTED(frame_context.MatchRenderSemaphoresToSwapChain(), Result);
    return Result(std::move(frame_context));
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

Rndr::ErrorCode Rndr::Forge::FrameContext::MatchRenderSemaphoresToSwapChain()
{
    const u32 texture_count = m_swap_chain->GetColorTextureCount();
    if (static_cast<u32>(m_render_finished_semaphores.GetSize()) == texture_count)
    {
        return ErrorCode::Success;
    }
    m_render_finished_semaphores.Clear();
    for (u32 texture = 0; texture < texture_count; ++texture)
    {
        Opal::Expected<Semaphore, ErrorCode> render_finished = Semaphore::Create(*m_device);
        if (!render_finished.HasValue())
        {
            return render_finished.GetError();
        }
        m_render_finished_semaphores.EmplaceBack(std::move(render_finished.GetValue()));
    }
    return ErrorCode::Success;
}

Opal::Expected<Rndr::Forge::SwapChainStatus, Rndr::ErrorCode> Rndr::Forge::FrameContext::BeginFrame()
{
    using Result = Opal::Expected<SwapChainStatus, ErrorCode>;

    if (m_is_frame_recording)
    {
        RNDR_LOG_ERROR("Forge: BeginFrame was called twice without an EndFrame in between");
        return Result(ErrorCode::InvalidArgument);
    }
    // Frame k waits for k + 1, which is what frame k - frames_in_flight signalled - the frame whose slot,
    // command buffer and texture-ready semaphore this one is about to reuse. Nothing to reset afterwards, so
    // there is no ordering question about where the reset goes either.
    RNDR_FORGE_CHECK_EXPECTED(m_frame_timeline.Wait(m_frames_submitted + 1), Result);

    const u32 frame_index = GetFrameIndex();
    Opal::Expected<AcquiredTexture, ErrorCode> acquired_texture = m_swap_chain->AcquireTexture(m_texture_ready_semaphores[frame_index]);
    if (!acquired_texture.HasValue())
    {
        return Result(acquired_texture.GetError());
    }
    if (acquired_texture.GetValue().status == SwapChainStatus::OutOfDate)
    {
        // The swap chain was rebuilt, or the window has no client area and there is none. Nothing was submitted
        // for this frame, so the counter does not advance.
        RNDR_FORGE_CHECK_EXPECTED(MatchRenderSemaphoresToSwapChain(), Result);
        return Result(SwapChainStatus::OutOfDate);
    }

    const CommandBuffer& command_buffer = m_command_buffers[frame_index];
    RNDR_FORGE_CHECK_EXPECTED(command_buffer.Reset(), Result);
    RNDR_FORGE_CHECK_EXPECTED(command_buffer.Begin(), Result);
    m_is_frame_recording = true;
    return Result(SwapChainStatus::Success);
}

Opal::Expected<Rndr::Forge::SwapChainStatus, Rndr::ErrorCode> Rndr::Forge::FrameContext::EndFrame()
{
    using Result = Opal::Expected<SwapChainStatus, ErrorCode>;

    if (!m_is_frame_recording)
    {
        RNDR_LOG_ERROR("Forge: EndFrame was called without a BeginFrame that succeeded");
        return Result(ErrorCode::InvalidArgument);
    }
    // The recording flag and the acquired texture are set together in BeginFrame, so this only fires when
    // something outside the frame context released the swap chain mid-frame. Above the texture is reached for
    // rather than below, since every use of it from here on reports that as a missing BeginFrame instead.
    if (!m_swap_chain->HasAcquiredTexture())
    {
        RNDR_LOG_ERROR("Forge: the swap chain gave up its acquired texture in the middle of a frame");
        return Result(ErrorCode::InvalidArgument);
    }
    const u32 frame_index = GetFrameIndex();
    CommandBuffer& command_buffer = m_command_buffers[frame_index];
    Opal::Expected<Texture&, ErrorCode> color_texture = GetColorTexture();
    if (!color_texture.HasValue())
    {
        return Result(color_texture.GetError());
    }
    const Opal::Expected<ImageLayout, ErrorCode> color_layout = color_texture.GetValue().GetCurrentLayout();
    if (!color_layout.HasValue())
    {
        return Result(color_layout.GetError());
    }
    if (color_layout.GetValue() != ImageLayout::Present)
    {
        RNDR_FORGE_CHECK_EXPECTED(command_buffer.CmdTextureBarrier(TextureBarrier::ToPresent(color_texture.GetValue())), Result);
    }
    RNDR_FORGE_CHECK_EXPECTED(command_buffer.End(), Result);
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
    RNDR_FORGE_CHECK_EXPECTED(
        m_graphics_queue->Submit(
            {.command_buffers = {&command_buffer_ref, 1}, .wait_semaphores = {&wait, 1}, .signal_semaphores = {signals, 2}}),
        Result);

    // Advanced before the present, so that a present that comes back out of date still leaves the next frame on
    // the following slot - the work of this one was submitted either way.
    ++m_frames_submitted;

    Opal::Expected<SwapChainStatus, ErrorCode> status = m_swap_chain->Present(*m_present_queue, render_finished);
    if (!status.HasValue())
    {
        return Result(status.GetError());
    }
    if (status.GetValue() == SwapChainStatus::OutOfDate)
    {
        RNDR_FORGE_CHECK_EXPECTED(MatchRenderSemaphoresToSwapChain(), Result);
    }
    return Result(status.GetValue());
}

Opal::Expected<Rndr::Forge::CommandBuffer&, Rndr::ErrorCode> Rndr::Forge::FrameContext::GetCommandBuffer()
{
    using Result = Opal::Expected<CommandBuffer&, ErrorCode>;

    if (!m_is_frame_recording)
    {
        RNDR_LOG_ERROR("Forge: there is no command buffer outside of a frame - call BeginFrame first");
        return Result(ErrorCode::InvalidArgument);
    }
    return Result(m_command_buffers[GetFrameIndex()]);
}

Opal::Expected<const Rndr::Forge::Texture&, Rndr::ErrorCode> Rndr::Forge::FrameContext::GetColorTexture() const
{
    using Result = Opal::Expected<const Texture&, ErrorCode>;

    if (!m_swap_chain->HasAcquiredTexture())
    {
        RNDR_LOG_ERROR("Forge: there is no acquired texture outside of a frame - call BeginFrame first");
        return Result(ErrorCode::InvalidArgument);
    }
    // The const overload of the swap chain's own accessor, since this one hands out a const reference.
    const SwapChain& swap_chain = *m_swap_chain;
    return swap_chain.GetCurrentColorTexture();
}

Opal::Expected<Rndr::Forge::Texture&, Rndr::ErrorCode> Rndr::Forge::FrameContext::GetColorTexture()
{
    using Result = Opal::Expected<Texture&, ErrorCode>;

    if (!m_swap_chain->HasAcquiredTexture())
    {
        RNDR_LOG_ERROR("Forge: there is no acquired texture outside of a frame - call BeginFrame first");
        return Result(ErrorCode::InvalidArgument);
    }
    return m_swap_chain->GetCurrentColorTexture();
}

Rndr::Vector2i Rndr::Forge::FrameContext::GetRenderSize() const
{
    const VkExtent2D extent = m_swap_chain->GetExtent();
    return {static_cast<i32>(extent.width), static_cast<i32>(extent.height)};
}
