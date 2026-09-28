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
 * Watch out: Wireframe and wide lines are device features. The device turns them on wherever it has them, so
 *            check device.GetEnabledFeatures().fill_mode_non_solid and wide_lines before building the right half, or
 *            require them in DeviceDesc::features to fail at creation instead.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
