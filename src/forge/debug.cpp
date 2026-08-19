#include "rndr/forge/debug.hpp"

#include "rndr/forge/buffer.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/descriptor-set.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/frame-context.hpp"
#include "rndr/forge/pipeline.hpp"
#include "rndr/forge/shader.hpp"
#include "rndr/forge/swap-chain.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/texture.hpp"
#include "rndr/forge/vulkan-exception.hpp"

namespace
{
/**
 * The one call every overload is. Silent when the object holds no handle or the instance has no debug utils:
 * a name is a convenience, and refusing to attach one is never worth failing a frame over.
 */
void SetName(const Rndr::Forge::Device& device, VkObjectType object_type, Rndr::u64 handle, const Opal::StringUtf8& name)
{
    // The loader hands out a callable pointer for an extension command whether or not the extension was
    // enabled, so asking the device is what keeps this from being an access violation. See CmdDrawMeshTasks.
    if (handle == 0 || !device.AreDebugUtilsEnabled() || vkSetDebugUtilsObjectNameEXT == nullptr)
    {
        return;
    }
    const VkDebugUtilsObjectNameInfoEXT name_info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                                                  .objectType = object_type,
                                                  .objectHandle = handle,
                                                  .pObjectName = reinterpret_cast<const char*>(name.GetData())};
    const VkResult result = vkSetDebugUtilsObjectNameEXT(device.GetNativeDevice(), &name_info);
    if (result != VK_SUCCESS)
    {
        throw Rndr::Forge::VulkanException(result, "vkSetDebugUtilsObjectNameEXT");
    }
}

template <typename Handle>
Rndr::u64 ToHandle(Handle handle)
{
    return reinterpret_cast<Rndr::u64>(handle);
}
}  // namespace

void Rndr::Forge::SetDebugName(const Device& device, const Buffer& buffer, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_BUFFER, ToHandle(buffer.GetNativeBuffer()), name);
}

void Rndr::Forge::SetDebugName(const Device& device, const Texture& texture, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_IMAGE, ToHandle(texture.GetNativeImage()), name);
    SetName(device, VK_OBJECT_TYPE_IMAGE_VIEW, ToHandle(texture.GetNativeImageView()), name);
}

void Rndr::Forge::SetDebugName(const Device& device, const Sampler& sampler, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_SAMPLER, ToHandle(sampler.GetNativeSampler()), name);
}

void Rndr::Forge::SetDebugName(const Device& device, const Shader& shader, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_SHADER_MODULE, ToHandle(shader.GetNativeShaderModule()), name);
}

void Rndr::Forge::SetDebugName(const Device& device, const Pipeline& pipeline, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_PIPELINE, ToHandle(pipeline.GetNativePipeline()), name);
    SetName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, ToHandle(pipeline.GetNativePipelineLayout()), name);
}

void Rndr::Forge::SetDebugName(const Device& device, const DescriptorSetLayout& layout, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, ToHandle(layout.GetNativeDescriptorSetLayout()), name);
}

void Rndr::Forge::SetDebugName(const Device& device, const DescriptorPool& pool, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, ToHandle(pool.GetNativeDescriptorPool()), name);
}

void Rndr::Forge::SetDebugName(const Device& device, const DescriptorSet& descriptor_set, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, ToHandle(descriptor_set.GetNativeDescriptorSet()), name);
}

void Rndr::Forge::SetDebugName(const Device& device, const CommandBuffer& command_buffer, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_COMMAND_BUFFER, ToHandle(command_buffer.GetNativeCommandBuffer()), name);
}

void Rndr::Forge::SetDebugName(const Device& device, const Fence& fence, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_FENCE, ToHandle(fence.GetNativeFence()), name);
}

void Rndr::Forge::SetDebugName(const Device& device, const Semaphore& semaphore, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_SEMAPHORE, ToHandle(semaphore.GetNativeSemaphore()), name);
}

void Rndr::Forge::SetDebugName(const Device& device, const DeviceQueue& queue, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_QUEUE, ToHandle(queue.GetNativeQueue()), name);
    SetName(device, VK_OBJECT_TYPE_COMMAND_POOL, ToHandle(queue.GetNativeCommandPool()), name);
}

/** "shadow pass" plus " fence 1", since a name that does not say which frame it belongs to is half a name. */
static Opal::StringUtf8 Indexed(const Opal::StringUtf8& name, const char* what, Rndr::u32 index)
{
    char buffer[128] = {};
    snprintf(buffer, sizeof(buffer), "%s %s %u", reinterpret_cast<const char*>(name.GetData()), what, index);
    return Opal::StringUtf8(buffer);
}

void Rndr::Forge::SetDebugName(const Device& device, const FrameContext& frame_context, const Opal::StringUtf8& name)
{
    for (i32 frame = 0; frame < frame_context.m_fences.GetSize(); ++frame)
    {
        SetDebugName(device, frame_context.m_fences[frame], Indexed(name, "fence", static_cast<u32>(frame)));
        SetDebugName(device, frame_context.m_image_ready_semaphores[frame], Indexed(name, "image ready", static_cast<u32>(frame)));
        SetDebugName(device, frame_context.m_command_buffers[frame], Indexed(name, "commands", static_cast<u32>(frame)));
    }
    for (i32 image = 0; image < frame_context.m_render_finished_semaphores.GetSize(); ++image)
    {
        SetDebugName(device, frame_context.m_render_finished_semaphores[image], Indexed(name, "render finished", static_cast<u32>(image)));
    }
}

void Rndr::Forge::SetDebugName(const Device& device, const SwapChain& swap_chain, const Opal::StringUtf8& name)
{
    SetName(device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, ToHandle(swap_chain.GetNativeSwapChain()), name);
    for (u32 image_index = 0; image_index < swap_chain.GetColorImageCount(); ++image_index)
    {
        SetDebugName(device, swap_chain.GetColorImage(image_index), name);
    }
}
