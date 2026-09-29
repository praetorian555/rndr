#pragma once

namespace Rndr::Forge
{

#if defined(RNDR_FORGE_VULKAN_1_1)
/**
 * Points volk's pointers for the 1.2 and 1.3 commands Forge calls at the core commands, for a device used at 1.3, or at
 * the extension commands they were promoted from, for one used below it. volk keeps one set of pointers for the whole
 * process, so the device created last decides. A context points them at the extension commands until a device says
 * otherwise.
 *
 * @param use_core True for a device whose PhysicalDevice::GetApiVersion reaches 1.3.
 */
void SelectPromotedCommands(bool use_core);
#endif

}  // namespace Rndr::Forge
