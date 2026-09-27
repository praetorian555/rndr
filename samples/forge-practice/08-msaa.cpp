/**
 * 08 - MSAA two ways (Multi-pass, windowed)
 *
 * Draw thin geometry at 4x MSAA. Resolve once inside the pass via resolve_texture on a transient attachment,
 * once with a standalone CmdResolveTexture, and time both.
 *
 * Exercises: SampleCount, TextureUsageBits::TransientAttachment, RenderingAttachmentDesc::resolve_texture,
 *            ResolveMode, CmdResolveTexture, TimestampQueryPool.
 *
 * Done when: Both paths give the same picture; the timings for each show on screen.
 * Stretch:   Resolve depth as well (only the in-pass resolve can) and show it.
 * Watch out: A transient attachment cannot be a transfer source, so the standalone resolve needs a
 *            non-transient one.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
