/**
 * 09 - Shadow mapping (Multi-pass, windowed)
 *
 * Render a directional light's depth in a depth-only pass with no fragment shader, then sample it with a
 * comparison sampler for hardware PCF.
 *
 * Exercises: depth-only pipeline, CmdSetDepthBias, depth_bias_clamp, SamplerDesc::compare_enabled,
 *            compare_operator, SampleCmp, ImageAspectBits::Depth.
 *
 * Done when: Shadows with soft PCF edges, no acne, no peter-panning worth mentioning.
 * Stretch:   Cascades in a single pass with RenderingDesc::layer_count and shader_output_layer, then a point
 *            light into a cube map.
 * Watch out: Sampling a combined depth-stencil format needs a view restricted to the depth aspect.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
