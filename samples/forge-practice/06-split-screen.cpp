/**
 * 06 - Split-screen viewer (Window basics, windowed)
 *
 * Load a mesh with depth testing and draw the scene twice into two viewports: shaded on the left, wireframe on
 * the right.
 *
 * Exercises: LoadMesh, PixelFormat::D32_SFLOAT, TextureBarrier::ToDepthStencilAttachment, depth_attachment,
 *            CmdSetViewport, CmdSetScissor, fill_mode_non_solid, wide_lines, CmdSetLineWidth.
 *
 * Done when: Both halves render with correct depth, and the depth texture is rebuilt on resize.
 * Stretch:   A third viewport showing the depth buffer linearised.
 * Watch out: Wireframe and wide lines are device features; request them in DeviceFeatures or the pipeline is
 *            refused.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
