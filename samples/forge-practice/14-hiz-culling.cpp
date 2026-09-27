/**
 * 14 - Hi-Z occlusion culling (GPU-driven, windowed)
 *
 * Build a depth pyramid from last frame's depth, one mip at a time, and cull instances whose bounds are behind
 * it.
 *
 * Exercises: TextureView per mip, SamplerDesc::reduction, SamplerReduction::Max, sampler_filter_minmax, storage
 *            image writes, CmdTextureBarrier per level.
 *
 * Done when: Instances behind a wall stop being drawn, with the count to prove it.
 * Stretch:   Two-phase culling: redraw what was culled against last frame but is visible against this one.
 * Watch out: Probe sampler_filter_minmax; without it, fall back to four taps and a max.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
