/**
 * 11 - Deferred shading (Multi-pass, windowed)
 *
 * Fill a G-buffer (albedo, normal, material) with several colour targets, then light it with many point lights
 * as stencil-marked light volumes reading the depth through a depth-aspect view.
 *
 * Exercises: color_attachments (several), independent_blend, ColorBlendDesc per target, depth-aspect
 *            TextureView, stencil light volumes, additive blend.
 *
 * Done when: A hundred moving lights at interactive frame rate, each lighting only what its volume touches.
 * Stretch:   Read the G-buffer as input attachments inside one pass
 *            (DeviceFeatures::dynamic_rendering_local_read, General layout).
 * Watch out: Different blend states per target need independent_blend. It is on wherever the device has it;
 *            check device.GetEnabledFeatures().independent_blend, or require it in DeviceDesc::features.
 */

#include "practice.hpp"

int main()
{
    // Application and window, context, surface, device, swap chain and frame context, then the frame loop.
    return 0;
}
