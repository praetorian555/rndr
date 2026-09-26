#include "synchronization2.hpp"

#include "opal/container/dynamic-array.h"

#include "rndr/forge/device.hpp"

VkPipelineStageFlags Rndr::Forge::ToOriginalStages(const Device& device, VkPipelineStageFlags2 stages, bool is_source)
{
    // Below bit 32 the two sets are the same bits for the same stages, the task and mesh ones of VK_EXT_mesh_shader
    // included.
    auto original = static_cast<VkPipelineStageFlags>(stages & 0xFFFFFFFFull);
    constexpr VkPipelineStageFlags2 k_transfer_parts =
        VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_RESOLVE_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT;
    if ((stages & k_transfer_parts) != 0)
    {
        original |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    if ((stages & (VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT)) != 0)
    {
        original |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }
    if ((stages & VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT) != 0)
    {
        // Only the stages the device enabled: naming one it did not is an error rather than a stage that never runs.
        const DeviceFeatures& features = device.GetFeatures();
        original |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        if (features.tessellation_shader)
        {
            original |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        }
        if (features.geometry_shader)
        {
            original |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        }
        if (features.task_shader)
        {
            original |= VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT;
        }
        if (features.mesh_shader)
        {
            original |= VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
        }
    }
    if (original == 0)
    {
        return is_source ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    return original;
}

VkAccessFlags Rndr::Forge::ToOriginalAccess(VkAccessFlags2 access)
{
    auto original = static_cast<VkAccessFlags>(access & 0xFFFFFFFFull);
    if ((access & (VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT)) != 0)
    {
        original |= VK_ACCESS_SHADER_READ_BIT;
    }
    if ((access & VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT) != 0)
    {
        original |= VK_ACCESS_SHADER_WRITE_BIT;
    }
    return original;
}

void Rndr::Forge::CmdPipelineBarrier2(const Device& device, VkCommandBuffer command_buffer, const VkDependencyInfo& dependency_info)
{
    if (device.HasSynchronization2())
    {
        vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        return;
    }
    const VkDependencyFlags flags = dependency_info.dependencyFlags;
    for (u32 i = 0; i < dependency_info.memoryBarrierCount; ++i)
    {
        const VkMemoryBarrier2& barrier = dependency_info.pMemoryBarriers[i];
        const VkMemoryBarrier original{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                       .srcAccessMask = ToOriginalAccess(barrier.srcAccessMask),
                                       .dstAccessMask = ToOriginalAccess(barrier.dstAccessMask)};
        vkCmdPipelineBarrier(command_buffer, ToOriginalStages(device, barrier.srcStageMask, true),
                             ToOriginalStages(device, barrier.dstStageMask, false), flags, 1, &original, 0, nullptr, 0, nullptr);
    }
    for (u32 i = 0; i < dependency_info.bufferMemoryBarrierCount; ++i)
    {
        const VkBufferMemoryBarrier2& barrier = dependency_info.pBufferMemoryBarriers[i];
        const VkBufferMemoryBarrier original{.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                             .srcAccessMask = ToOriginalAccess(barrier.srcAccessMask),
                                             .dstAccessMask = ToOriginalAccess(barrier.dstAccessMask),
                                             .srcQueueFamilyIndex = barrier.srcQueueFamilyIndex,
                                             .dstQueueFamilyIndex = barrier.dstQueueFamilyIndex,
                                             .buffer = barrier.buffer,
                                             .offset = barrier.offset,
                                             .size = barrier.size};
        vkCmdPipelineBarrier(command_buffer, ToOriginalStages(device, barrier.srcStageMask, true),
                             ToOriginalStages(device, barrier.dstStageMask, false), flags, 0, nullptr, 1, &original, 0, nullptr);
    }
    for (u32 i = 0; i < dependency_info.imageMemoryBarrierCount; ++i)
    {
        const VkImageMemoryBarrier2& barrier = dependency_info.pImageMemoryBarriers[i];
        const VkImageMemoryBarrier original{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                            .srcAccessMask = ToOriginalAccess(barrier.srcAccessMask),
                                            .dstAccessMask = ToOriginalAccess(barrier.dstAccessMask),
                                            .oldLayout = barrier.oldLayout,
                                            .newLayout = barrier.newLayout,
                                            .srcQueueFamilyIndex = barrier.srcQueueFamilyIndex,
                                            .dstQueueFamilyIndex = barrier.dstQueueFamilyIndex,
                                            .image = barrier.image,
                                            .subresourceRange = barrier.subresourceRange};
        vkCmdPipelineBarrier(command_buffer, ToOriginalStages(device, barrier.srcStageMask, true),
                             ToOriginalStages(device, barrier.dstStageMask, false), flags, 0, nullptr, 0, nullptr, 1, &original);
    }
}

VkResult Rndr::Forge::QueueSubmit2(const Device& device, VkQueue queue, const VkSubmitInfo2& submit_info, VkFence fence)
{
    if (device.HasSynchronization2())
    {
        return vkQueueSubmit2(queue, 1, &submit_info, fence);
    }
    Opal::DynamicArray<VkSemaphore> wait_semaphores(submit_info.waitSemaphoreInfoCount);
    // uint64_t rather than u64: Vulkan points at the former, which is unsigned long on Linux and not unsigned long long.
    Opal::DynamicArray<uint64_t> wait_values(submit_info.waitSemaphoreInfoCount);
    Opal::DynamicArray<VkPipelineStageFlags> wait_stages(submit_info.waitSemaphoreInfoCount);
    for (u32 i = 0; i < submit_info.waitSemaphoreInfoCount; ++i)
    {
        const VkSemaphoreSubmitInfo& info = submit_info.pWaitSemaphoreInfos[i];
        wait_semaphores[static_cast<i32>(i)] = info.semaphore;
        wait_values[static_cast<i32>(i)] = info.value;
        // The stages that wait: nothing named is nothing waiting, which the bottom of the pipe says here.
        wait_stages[static_cast<i32>(i)] = ToOriginalStages(device, info.stageMask, false);
    }
    Opal::DynamicArray<VkSemaphore> signal_semaphores(submit_info.signalSemaphoreInfoCount);
    Opal::DynamicArray<uint64_t> signal_values(submit_info.signalSemaphoreInfoCount);
    for (u32 i = 0; i < submit_info.signalSemaphoreInfoCount; ++i)
    {
        signal_semaphores[static_cast<i32>(i)] = submit_info.pSignalSemaphoreInfos[i].semaphore;
        signal_values[static_cast<i32>(i)] = submit_info.pSignalSemaphoreInfos[i].value;
    }
    Opal::DynamicArray<VkCommandBuffer> command_buffers(submit_info.commandBufferInfoCount);
    for (u32 i = 0; i < submit_info.commandBufferInfoCount; ++i)
    {
        command_buffers[static_cast<i32>(i)] = submit_info.pCommandBufferInfos[i].commandBuffer;
    }
    // Read for the timeline semaphores and ignored for the binary ones, so every semaphore gets a slot either way. Only
    // chained on a device that has timelines: the structure belongs to their extension.
    const VkTimelineSemaphoreSubmitInfo timeline_info{.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
                                                      .waitSemaphoreValueCount = static_cast<u32>(wait_values.GetSize()),
                                                      .pWaitSemaphoreValues = wait_values.GetData(),
                                                      .signalSemaphoreValueCount = static_cast<u32>(signal_values.GetSize()),
                                                      .pSignalSemaphoreValues = signal_values.GetData()};
    const VkSubmitInfo original{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                .pNext = device.HasTimelineSemaphores() ? &timeline_info : nullptr,
                                .waitSemaphoreCount = static_cast<u32>(wait_semaphores.GetSize()),
                                .pWaitSemaphores = wait_semaphores.GetData(),
                                .pWaitDstStageMask = wait_stages.GetData(),
                                .commandBufferCount = static_cast<u32>(command_buffers.GetSize()),
                                .pCommandBuffers = command_buffers.GetData(),
                                .signalSemaphoreCount = static_cast<u32>(signal_semaphores.GetSize()),
                                .pSignalSemaphores = signal_semaphores.GetData()};
    return vkQueueSubmit(queue, 1, &original, fence);
}

void Rndr::Forge::CmdWriteTimestamp2(const Device& device, VkCommandBuffer command_buffer, VkPipelineStageFlags2 stage,
                                     VkQueryPool query_pool, u32 query_index)
{
    if (device.HasSynchronization2())
    {
        vkCmdWriteTimestamp2(command_buffer, stage, query_pool, query_index);
        return;
    }
    // One bit is what the original takes. The stages go in pipeline order by bit, so the highest is the latest.
    const VkPipelineStageFlags stages = ToOriginalStages(device, stage, false);
    VkPipelineStageFlags latest = 1;
    while ((stages & ~((latest << 1) - 1)) != 0)
    {
        latest <<= 1;
    }
    vkCmdWriteTimestamp(command_buffer, static_cast<VkPipelineStageFlagBits>(latest), query_pool, query_index);
}
