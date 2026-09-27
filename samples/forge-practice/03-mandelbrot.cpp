/**
 * 03 - Mandelbrot to image (Headless compute, headless)
 *
 * Write the Mandelbrot set into a storage texture, generate its mip chain, blit a quarter-size copy and save
 * both as PNG.
 *
 * Exercises: Texture::Create, TextureUsageBits::Storage, TextureBarrier::ToGeneral, RWTexture2D,
 *            CmdGenerateMips, CmdBlitTexture, ReadBackTexture, GetMipLevelSize.
 *
 * Done when: Both PNGs open and look right; a handful of pixels checked against a CPU evaluation of the same
 *            iteration.
 * Stretch:   Pick the iteration count with a specialization constant and render two pipelines from one shader.
 * Watch out: CmdGenerateMips needs a format the device can blit with linear filtering; probe the format rather
 *            than assume it.
 */

#include "practice.hpp"

int main()
{
    // Context and device, then the work, then a readback checked against the CPU.
    return 0;
}
