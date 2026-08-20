#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/ref.h"
#include "opal/container/string.h"

#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/swap-chain.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/math.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"

namespace Rndr::Forge
{

struct FrameContextDesc
{
    /**
     * How many frames the host may run ahead of the device. Two lets the host record the next frame while the
     * device works on this one; one makes every frame wait for the previous, which is only worth it while
     * debugging.
     */
    i32 frames_in_flight = 2;
};

/**
 * The frame loop of a windowed application: the frames in flight, the fence and command buffer of each, the
 * semaphores on both sides of the swap chain, and the order the four have to happen in.
 *
 * BeginFrame waits for the slot this frame will reuse, acquires an image, and hands back a command buffer
 * that is already recording. EndFrame closes it, submits it against the right semaphores, and presents.
 * Between them the caller records whatever it likes.
 *
 *     while (!window->IsClosed())
 *     {
 *         if (frame_context.BeginFrame() == SwapChainStatus::OutOfDate)
 *         {
 *             continue;  // the swap chain was rebuilt, nothing was recorded
 *         }
 *         CommandBuffer& command_buffer = frame_context.GetCommandBuffer();
 *         ...
 *         frame_context.EndFrame();
 *     }
 *
 * A resized or minimized window is not an error here either: BeginFrame and EndFrame return
 * SwapChainStatus::OutOfDate and have already rebuilt what went stale, so the caller skips the frame and
 * carries on. Nothing was submitted for a skipped frame, so the frame index does not advance and the fence
 * of that slot is left as it was.
 */
class FrameContext
{
public:
    FrameContext() = default;

    /**
     * @param device Device everything is created from.
     * @param swap_chain Swap chain to render into. Acquired from and presented to, so it is held by reference.
     * @param graphics_queue Queue the frame's command buffer is submitted to.
     * @param present_queue Queue the image is presented on. The same object as the graphics queue on most devices.
     */
    FrameContext(const Device& device, SwapChain& swap_chain, DeviceQueue& graphics_queue, DeviceQueue& present_queue,
                 const FrameContextDesc& desc = {});

    ~FrameContext();

    FrameContext(const FrameContext&) = delete;
    FrameContext& operator=(const FrameContext&) = delete;
    FrameContext(FrameContext&& other) noexcept;
    FrameContext& operator=(FrameContext&& other) noexcept;

    /** Releases the fences, semaphores and command buffers. Waits for the device first, since they may be in use. */
    void Destroy();

    [[nodiscard]] bool IsValid() const { return !m_fences.IsEmpty(); }
    [[nodiscard]] const FrameContextDesc& GetDesc() const { return m_desc; }

    /**
     * Wait for the slot this frame reuses, acquire an image, and begin its command buffer.
     * @return Success when the frame can be recorded, OutOfDate when the swap chain was rebuilt and this frame
     *         has to be skipped - nothing was recorded and nothing has to be undone.
     */
    SwapChainStatus BeginFrame();

    /**
     * End the command buffer, submit it, and present the image it rendered into.
     * @param color_image_layout Layout the caller left the swap chain image in. The transition to Present is
     *        made from it, which is the last thing every frame does and the easiest to forget.
     * @return OutOfDate when the swap chain stopped matching the surface, in which case it has been rebuilt.
     */
    SwapChainStatus EndFrame(ImageLayout color_image_layout = ImageLayout::ColorAttachment);

    /** The command buffer of the frame in flight. Only between BeginFrame and EndFrame. */
    [[nodiscard]] CommandBuffer& GetCommandBuffer();

    /** Which of the frames in flight this is, for indexing anything the caller keeps one of per frame. */
    [[nodiscard]] u32 GetFrameIndex() const { return m_frame_index; }

    /**
     * Which swap chain image this frame acquired. Only meaningful between BeginFrame and EndFrame; the swap
     * chain is what remembers it, and answers k_invalid_image_index outside a frame.
     */
    [[nodiscard]] u32 GetImageIndex() const { return m_swap_chain->GetCurrentImageIndex(); }

    /** The swap chain image this frame renders into. */
    [[nodiscard]] const Texture& GetColorImage() const;
    [[nodiscard]] VkImageView GetColorImageView() const;

    /**
     * The size to render at, which is the size of the swap chain rather than of the window - the window can be
     * a frame ahead of it.
     */
    [[nodiscard]] Vector2i GetRenderSize() const;

private:
    /** Names everything a frame context owns, which is the only reason anything outside needs to see them. */
    friend void SetDebugName(const Device& device, const FrameContext& frame_context, const Opal::StringUtf8& name);

    /** One semaphore per swap chain image, rebuilt whenever the swap chain is. */
    void MatchRenderSemaphoresToSwapChain();

    FrameContextDesc m_desc;
    Opal::Ref<const Device> m_device;
    Opal::Ref<SwapChain> m_swap_chain;
    Opal::Ref<DeviceQueue> m_graphics_queue;
    Opal::Ref<DeviceQueue> m_present_queue;

    /** Per frame in flight. */
    Opal::DynamicArray<Fence> m_fences;
    Opal::DynamicArray<Semaphore> m_image_ready_semaphores;
    Opal::DynamicArray<CommandBuffer> m_command_buffers;

    /**
     * Per swap chain image rather than per frame: the semaphore a present waits on has to belong to the image
     * being presented, not to the frame that drew it.
     */
    Opal::DynamicArray<Semaphore> m_render_finished_semaphores;

    i32 m_frame_index = 0;
    bool m_is_frame_recording = false;
};

}  // namespace Rndr::Forge
