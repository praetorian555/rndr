/**
 * 05 - Instanced sprite grid (Window basics, windowed)
 *
 * Draw a 64x64 grid of sprites from one atlas in a single instanced call, with per-instance data in a per-frame
 * buffer and a key to cycle sampler modes.
 *
 * Exercises: Texture::Create (from Bitmap), Sampler::Create, SamplerDesc, ImageAddressMode::MirrorRepeat,
 *            max_anisotropy, CmdDrawIndexed, GetFrameIndex, ScopedDebugLabel, SetDebugName.
 *
 * Done when: Nearest, linear, mirrored and anisotropic all switch live; a RenderDoc capture reads cleanly by
 *            label.
 * Stretch:   Animate the instances by updating only the current frame's buffer, and prove with validation that
 *            no frame in flight is written to.
 * Watch out: One per-frame buffer per frame in flight, indexed by GetFrameIndex(), or you write what the GPU is
 *            still reading.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
