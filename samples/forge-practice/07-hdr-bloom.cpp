/**
 * 07 - HDR and bloom (Multi-pass, windowed)
 *
 * Render an emissive scene into an R16G16B16A16 target, build a bloom mip chain by downsampling one level at a
 * time, upsample back and tonemap to the swap chain.
 *
 * Exercises: TextureView::Create, RenderingAttachmentDesc::view, TextureBarrier::ToShaderRead, fullscreen
 *            triangle, multiple pipelines, CmdTextureBarriers.
 *
 * Done when: Bloom visibly bleeds from bright pixels, the chain resizes with the window, and the layer is quiet
 *            about layouts.
 * Stretch:   Exposure adapted from average luminance computed in a compute pass.
 * Watch out: An attachment view may cover only one mip level; take one view per level for writing and the whole
 *            texture for sampling.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
