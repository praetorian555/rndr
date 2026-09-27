/**
 * 10 - Stencil outline and portal (Multi-pass, windowed)
 *
 * Outline the selected object by writing stencil in one draw and drawing a scaled copy where stencil is not
 * equal. Then a portal: a quad that shows a second scene only inside itself.
 *
 * Exercises: StencilOperation, per-face stencil, CmdSetStencilReference, CmdSetStencilCompareMask,
 *            CmdSetStencilWriteMask, PixelFormat::D24_UNORM_S8_UINT.
 *
 * Done when: Outline follows the silhouette from every angle; the portal hides the second scene outside the
 *            quad.
 * Stretch:   Nested portals with the stencil value as recursion depth.
 * Watch out: Stencil reference and masks are dynamic state only if the pipeline says so.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
