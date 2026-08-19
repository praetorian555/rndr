#pragma once

#include "opal/container/string.h"

#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"

namespace Rndr::Forge
{

/**
 * Give a Vulkan object a name, which the validation layer prints beside the handle and a capture shows in
 * place of it. Worth doing for anything that outlives a frame: a message naming "shadow map" instead of
 * VkImage 0x2a0000000042 is the difference between reading a report and hunting for a handle.
 *
 * Every overload is a no-op when the instance did not enable VK_EXT_debug_utils, which is the case in a
 * build with neither RNDR_FORGE_VALIDATION nor GraphicsContextDesc::collect_debug_messages, so a call site
 * never has to ask first. Naming an empty object is a no-op as well.
 *
 * There is no debug_name on the descs on purpose: the types with a desc are a subset of the types with a
 * handle, and one shape that names all of them beats a field on most of them.
 */

void SetDebugName(const Device& device, const Buffer& buffer, const Opal::StringUtf8& name);
void SetDebugName(const Device& device, const Texture& texture, const Opal::StringUtf8& name);
void SetDebugName(const Device& device, const Sampler& sampler, const Opal::StringUtf8& name);
void SetDebugName(const Device& device, const Shader& shader, const Opal::StringUtf8& name);
void SetDebugName(const Device& device, const Pipeline& pipeline, const Opal::StringUtf8& name);
void SetDebugName(const Device& device, const DescriptorSetLayout& layout, const Opal::StringUtf8& name);
void SetDebugName(const Device& device, const DescriptorPool& pool, const Opal::StringUtf8& name);
void SetDebugName(const Device& device, const DescriptorSet& descriptor_set, const Opal::StringUtf8& name);
void SetDebugName(const Device& device, const CommandBuffer& command_buffer, const Opal::StringUtf8& name);
void SetDebugName(const Device& device, const Fence& fence, const Opal::StringUtf8& name);
void SetDebugName(const Device& device, const Semaphore& semaphore, const Opal::StringUtf8& name);
void SetDebugName(const Device& device, const DeviceQueue& queue, const Opal::StringUtf8& name);

/** Names the texture and its view together, since a message about either one wants the same name. */
void SetDebugName(const Device& device, const SwapChain& swap_chain, const Opal::StringUtf8& name);

}  // namespace Rndr::Forge
