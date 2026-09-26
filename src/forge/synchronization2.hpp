#pragma once

#include "volk.h"

#include "rndr/forge/forward.hpp"
#include "rndr/types.hpp"

namespace Rndr::Forge
{

/**
 * The three synchronization2 commands Forge records, each run as itself on a device that has synchronization2 and
 * translated to the command it replaced on one that does not (Device::HasSynchronization2) - which only a
 * RNDR_FORGE_VULKAN_1_1 build takes. Forge builds the synchronization2 structures in either case; these are the one
 * place that knows the difference.
 */

/**
 * vkCmdPipelineBarrier2, or one vkCmdPipelineBarrier per barrier: the original takes one pair of stage masks for a
 * whole call, and a barrier each keeps the stages every barrier named rather than widening them all to the union.
 */
void CmdPipelineBarrier2(const Device& device, VkCommandBuffer command_buffer, const VkDependencyInfo& dependency_info);

/**
 * vkQueueSubmit2 of one batch, or vkQueueSubmit of the same, the timeline values going in a
 * VkTimelineSemaphoreSubmitInfo. A signal there happens once the whole batch is done rather than at the stages the
 * synchronization2 info names, which is later and never wrong.
 */
[[nodiscard]] VkResult QueueSubmit2(const Device& device, VkQueue queue, const VkSubmitInfo2& submit_info, VkFence fence);

/**
 * vkCmdWriteTimestamp2, or vkCmdWriteTimestamp at the original stage the given one maps to - the latest of them when
 * it maps to several, so the tick still comes after all of the work the stage named.
 */
void CmdWriteTimestamp2(const Device& device, VkCommandBuffer command_buffer, VkPipelineStageFlags2 stage, VkQueryPool query_pool,
                        u32 query_index);

/**
 * The original stage mask a synchronization2 one stands for on this device. The stages synchronization2 added are
 * parts of original ones - Copy of Transfer, IndexInput of VertexInput - and PreRasterizationShaders is every shader
 * stage before rasterization the device has enabled. Nothing at all, which synchronization2 allows, becomes the top of
 * the pipe on the source side and the bottom on the destination side, which is what it means there.
 */
[[nodiscard]] VkPipelineStageFlags ToOriginalStages(const Device& device, VkPipelineStageFlags2 stages, bool is_source);

/** The original access mask a synchronization2 one stands for: the shader reads and writes it split fold back together. */
[[nodiscard]] VkAccessFlags ToOriginalAccess(VkAccessFlags2 access);

}  // namespace Rndr::Forge
