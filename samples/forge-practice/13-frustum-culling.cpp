/**
 * 13 - Frustum culling of 10k instances (GPU-driven, windowed)
 *
 * A compute pass tests 10,000 instance bounds against the frustum and writes DrawIndexedIndirectCommands and a
 * count. Materials are bindless.
 *
 * Exercises: CmdDrawIndexedIndirectCount, runtime descriptor array, DescriptorBindingFlagBits::PartiallyBound,
 *            variable descriptor count, NonUniformResourceIndex, buffer device address.
 *
 * Done when: Draw count on screen drops as the camera turns away; no culled instance is drawn.
 * Stretch:   A freeze-culling key that keeps the frustum fixed while the camera flies out to look at what was
 *            culled.
 * Watch out: non_uniform_descriptor_indexing must be on for an index that varies across a wave.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
