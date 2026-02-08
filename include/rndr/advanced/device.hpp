#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/hash-map.h"
#include "opal/container/shared-ptr.h"

#include "rndr/advanced/physical-device.hpp"
#include "rndr/types.hpp"

// Forward declare handle to avoid vma includes in headers.
using VmaAllocation = struct VmaAllocation_T*;
using VmaAllocator = struct VmaAllocator_T*;

namespace Rndr
{

enum class QueueFamily : u8
{
    Graphics,
    Present,
    AsyncCompute,
    Transfer,
    Decode,
    Encode,

    EnumCount
};

struct AdvancedDeviceDesc
{
    VkPhysicalDeviceFeatures features = {.samplerAnisotropy = VK_TRUE};
    Opal::DynamicArray<const char*> extensions;
    Opal::Ref<class AdvancedSurface> surface;
    bool use_async_compute_queue = true;
    bool use_dedicated_transfer_queue = true;
    bool use_decode_queue = false;
    bool use_encode_queue = false;
};

struct AdvancedQueueFamilyIndices
{
    static constexpr u32 k_invalid_index = 0xFFFFFFFF;

    u32 graphics_family = k_invalid_index;
    u32 present_family = k_invalid_index;
    u32 compute_family = k_invalid_index;
    u32 transfer_family = k_invalid_index;
    u32 encode_family_index = k_invalid_index;
    u32 decode_family_index = k_invalid_index;

    [[nodiscard]] Opal::DynamicArray<u32> GetValidQueueFamilies() const;
    [[nodiscard]] Rndr::u32 GetQueueFamilyIndex(QueueFamily queue_family) const;
};

struct AdvancedDescriptorPoolDesc
{
    u32 max_sets = 0;
    Opal::DynamicArray<VkDescriptorPoolSize> pool_sizes;
    VkDescriptorPoolCreateFlags flags = 0;
};

struct AdvancedDescriptorSetLayoutBinding
{
    u32 binding = 0;
    VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    u32 descriptor_count = 1;
    VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT;
    const VkSampler* sampler = nullptr;
};

struct AdvancedDescriptorSetLayoutDesc
{
    Opal::DynamicArray<AdvancedDescriptorSetLayoutBinding> bindings;
    VkDescriptorSetLayoutCreateFlags flags = 0;
};

struct AdvancedUpdateDescriptorSet
{
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    u32 binding = 0xFFFFFFFF;
    VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    union
    {
        VkDescriptorBufferInfo buffer_info;
        VkDescriptorImageInfo image_info;
    };
};

class AdvancedDeviceQueue
{
public:
    AdvancedDeviceQueue() = default;
    ~AdvancedDeviceQueue() = default;

    explicit AdvancedDeviceQueue(const class AdvancedDevice& device, u32 queue_family_index);

    AdvancedDeviceQueue(const AdvancedDeviceQueue&) = delete;
    AdvancedDeviceQueue& operator=(const AdvancedDeviceQueue&) = delete;
    AdvancedDeviceQueue(AdvancedDeviceQueue&& other) noexcept;
    AdvancedDeviceQueue& operator=(AdvancedDeviceQueue&& other) noexcept;

    [[nodiscard]] VkQueue GetNativeQueue() const { return m_queue; }
    [[nodiscard]] VkCommandPool GetNativeCommandPool() const { return m_command_pool; }
    [[nodiscard]] u32 GetQueueFamilyIndex() const { return m_queue_family_index; }

    [[nodiscard]] Opal::DynamicArray<VkCommandBuffer> CreateCommandBuffers(u32 count) const;
    void DestroyCommandBuffer(VkCommandBuffer command_buffer) const;
    void DestroyCommandBuffers(Opal::ArrayView<VkCommandBuffer> command_buffers) const;

    void Submit(const class AdvancedCommandBuffer& command_buffer, const class AdvancedFence& fence);

private:

    friend class AdvancedDevice;
    friend class Opal::SharedPtr<AdvancedDeviceQueue>;

    Opal::Ref<const AdvancedDevice> m_device;
    u32 m_queue_family_index = 0;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkCommandPool m_command_pool = VK_NULL_HANDLE;
};

class AdvancedDevice
{
public:
    AdvancedDevice() = default;
    explicit AdvancedDevice(AdvancedPhysicalDevice physical_device, const class AdvancedGraphicsContext& graphics_context,
                            const AdvancedDeviceDesc& desc = {});
    ~AdvancedDevice();

    AdvancedDevice(const AdvancedDevice&) = delete;
    const AdvancedDevice& operator=(const AdvancedDevice&) = delete;
    AdvancedDevice(AdvancedDevice&& other) noexcept;
    AdvancedDevice& operator=(AdvancedDevice&& other) noexcept;

    void Destroy();

    [[nodiscard]] VkDevice GetNativeDevice() const { return m_device; }
    [[nodiscard]] const AdvancedPhysicalDevice& GetPhysicalDevice() const { return m_physical_device; }
    [[nodiscard]] VkPhysicalDevice GetNativePhysicalDevice() const { return m_physical_device.GetNativePhysicalDevice(); }
    [[nodiscard]] const AdvancedDeviceDesc& GetDesc() const { return m_desc; }

    Opal::Ref<AdvancedDeviceQueue> GetQueue(QueueFamily queue_family);
    Opal::Ref<const AdvancedDeviceQueue> GetQueue(QueueFamily queue_family) const;

    [[nodiscard]] VmaAllocator GetGPUAllocator() const { return m_gpu_allocator; }

    [[nodiscard]] VkCommandBuffer CreateCommandBuffer(QueueFamily queue_family) const;
    [[nodiscard]] Opal::DynamicArray<VkCommandBuffer> CreateCommandBuffers(QueueFamily queue_family, u32 count) const;

    void DestroyCommandBuffer(VkCommandBuffer command_buffer, QueueFamily queue_family) const;
    void DestroyCommandBuffers(const Opal::DynamicArray<VkCommandBuffer>& command_buffers, QueueFamily queue_family) const;

    [[nodiscard]] VkDescriptorPool CreateDescriptorPool(const AdvancedDescriptorPoolDesc& desc = {}) const;
    bool DestroyDescriptorPool(VkDescriptorPool descriptor_pool) const;

    [[nodiscard]] VkDescriptorSetLayout CreateDescriptorSetLayout(const AdvancedDescriptorSetLayoutDesc& desc) const;
    bool DestroyDescriptorSetLayout(VkDescriptorSetLayout descriptor_set_layout) const;

    [[nodiscard]] Opal::DynamicArray<VkDescriptorSet> AllocateDescriptorSets(const VkDescriptorPool& descriptor_pool, u32 count,
                                                                             const VkDescriptorSetLayout& layout) const;

    void UpdateDescriptorSets(const Opal::DynamicArray<AdvancedUpdateDescriptorSet>& updates) const;

private:
    void CollectQueueFamilies(Opal::DynamicArray<VkDeviceQueueCreateInfo>& queue_create_infos);

    VkDevice m_device = VK_NULL_HANDLE;
    Opal::HashMap<QueueFamily, Opal::SharedPtr<AdvancedDeviceQueue>> m_queue_family_to_queue;
    AdvancedPhysicalDevice m_physical_device;
    AdvancedDeviceDesc m_desc;
    AdvancedQueueFamilyIndices m_queue_family_indices;
    VmaAllocator m_gpu_allocator = VK_NULL_HANDLE;
};

}  // namespace Rndr