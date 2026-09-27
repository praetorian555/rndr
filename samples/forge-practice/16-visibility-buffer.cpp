/**
 * 16 - Visibility buffer (GPU-driven, windowed)
 *
 * Rasterize small triangles in compute, packing depth, cluster and triangle into a u64 resolved with
 * InterlockedMax, then shade from that buffer in a material pass.
 *
 * Exercises: shader_buffer_int64_atomics, InterlockedMax (u64), CmdFillBuffer, storage buffers,
 *            CmdDispatchIndirect, everything above.
 *
 * Done when: Same picture as the hardware rasterizer path for a dense mesh.
 * Stretch:   Mix both: hardware raster for big triangles, compute for small.
 * Watch out: Clear the visibility buffer to the value that loses every InterlockedMax, every frame.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
