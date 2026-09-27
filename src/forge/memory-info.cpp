#include "memory-info.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "vk_mem_alloc.h"

#include "rndr/forge/device.hpp"
#include "rndr/forge/physical-device.hpp"

Opal::Optional<Rndr::Forge::MemoryInfo> Rndr::Forge::DescribeAllocation(const Device& device, VmaAllocation allocation)
{
    if (allocation == VK_NULL_HANDLE)
    {
        return {};
    }
    VmaAllocationInfo allocation_info{};
    vmaGetAllocationInfo(device.GetGPUAllocator(), allocation, &allocation_info);
    const VkPhysicalDeviceMemoryProperties& memory = device.GetPhysicalDevice().GetMemoryProperties();
    const VkMemoryType& type = memory.memoryTypes[allocation_info.memoryType];
    // The vendor bits above Protected - device coherent, uncached, RDMA - have no MemoryPropertyBits value, and
    // Forge never asks for any of them, so they are dropped rather than cast into values the enum does not name.
    constexpr VkMemoryPropertyFlags k_known = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
                                              VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_PROTECTED_BIT;
    return MemoryInfo{.properties = static_cast<MemoryPropertyBits>(type.propertyFlags & k_known),
                      .memory_type_index = allocation_info.memoryType,
                      .heap_index = type.heapIndex,
                      .heap_size = memory.memoryHeaps[type.heapIndex].size};
}
