/**
 * 04 - Spinning triangle, from scratch (Window basics, windowed)
 *
 * Open a window, build surface, swap chain and frame context, and draw a triangle rotated by a time push
 * constant. No copying from modern-vulkan.
 *
 * Exercises: Application::Create, Surface::Create, SwapChain::Create, FrameContext::Create, BeginFrame,
 *            EndFrame, SwapChainStatus, CmdBeginRendering, TextureBarrier::ToColorAttachment,
 *            TextureBarrier::ToPresent, VertexInputDesc::FromShader, CmdPushConstants.
 *
 * Done when: Resize, minimise and restore all work with zero validation messages.
 * Stretch:   Switch present mode (FIFO, Mailbox, Immediate where offered) at runtime and show the frame time in
 *            the title.
 * Watch out: BeginFrame returns a status as well as an error: anything but Success means rebuild or skip, not
 *            draw.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
