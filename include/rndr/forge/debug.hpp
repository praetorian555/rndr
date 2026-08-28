#pragma once

#include "opal/container/ref.h"
#include "opal/container/string.h"

#include "rndr/math.hpp"
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
void SetDebugName(const Device& device, const TimestampQueryPool& query_pool, const Opal::StringUtf8& name);

/** Names the texture and its view together, since a message about either one wants the same name. */
void SetDebugName(const Device& device, const SwapChain& swap_chain, const Opal::StringUtf8& name);

/**
 * Names every fence, semaphore and command buffer a frame context owns, each with the index of the frame or
 * the image it belongs to appended, since which of them a message is about is the useful part.
 */
void SetDebugName(const Device& device, const FrameContext& frame_context, const Opal::StringUtf8& name);

/**
 * A debug label region that closes itself. Opens the region on construction and closes it on destruction, so
 * an early return cannot leave a capture with a region that never ends.
 *
 * It is not a recorded command, which is why it lives here rather than beside the CmdBeginDebugLabel it
 * wraps: it is the thing that keeps the matching CmdEndDebugLabel from being forgotten.
 *
 * @code
 *   {
 *       Forge::ScopedDebugLabel shadow_pass(command_buffer, "shadow pass", {0.2f, 0.4f, 1.0f, 1.0f});
 *       command_buffer.CmdBeginRendering(...);
 *       ...
 *       command_buffer.CmdEndRendering();
 *   }
 * @endcode
 */
class ScopedDebugLabel
{
public:
    // As on CmdBeginDebugLabel, the const char* overload is what a label written into the source wants: it
    // reaches Vulkan without a copy, where a StringUtf8 built from a literal is one and, past what fits
    // inline, an allocation as well.
    ScopedDebugLabel(CommandBuffer& command_buffer, const char* name, const Vector4f& color = {1.0f, 1.0f, 1.0f, 1.0f});
    ScopedDebugLabel(CommandBuffer& command_buffer, const Opal::StringUtf8& name,
                     const Vector4f& color = {1.0f, 1.0f, 1.0f, 1.0f});
    ~ScopedDebugLabel();

    // A scope, not a value: copying one would close the region twice and moving it would leave the question
    // of which copy owns the end.
    ScopedDebugLabel(const ScopedDebugLabel&) = delete;
    ScopedDebugLabel& operator=(const ScopedDebugLabel&) = delete;
    ScopedDebugLabel(ScopedDebugLabel&&) = delete;
    ScopedDebugLabel& operator=(ScopedDebugLabel&&) = delete;

private:
    Opal::Ref<CommandBuffer> m_command_buffer;
};

}  // namespace Rndr::Forge
