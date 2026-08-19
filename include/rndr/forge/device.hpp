#pragma once

#include "volk/volk.h"

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/hash-map.h"
#include "opal/container/ref.h"
#include "opal/container/shared-ptr.h"

#include "rndr/forge/physical-device.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"

// Forward declare handle to avoid vma includes in headers.
using VmaAllocation = struct VmaAllocation_T*;
using VmaAllocator = struct VmaAllocator_T*;

namespace Rndr::Forge
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

struct DeviceDesc : Opal::ClonableBase<DeviceDesc>
{
    VkPhysicalDeviceFeatures features = {.samplerAnisotropy = VK_TRUE};
    Opal::DynamicArray<const char*> extensions;
    Opal::Ref<Surface> surface;
    bool use_async_compute_queue = true;
    bool use_dedicated_transfer_queue = true;
    bool use_decode_queue = false;
    bool use_encode_queue = false;

    OPAL_CLONE_FIELDS(features, extensions, surface, use_async_compute_queue, use_dedicated_transfer_queue, use_decode_queue, use_encode_queue);
};

struct QueueFamilyIndices
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

/**
 * One semaphore taking part in a submit, and the stages it is tied to: on the wait side the stages that have
 * to wait for it, on the signal side the stages that have to finish before it is signalled. Naming the stages
 * rather than the whole pipeline is what lets the work either side does not depend on run ahead.
 */
struct SemaphoreSubmit
{
    Opal::Ref<const Semaphore> semaphore;
    PipelineStageBits stages = PipelineStageBits::AllCommands;
};

/** One batch of work handed to a queue. Every part of it is optional except that an empty batch does nothing. */
struct SubmitDesc
{
    Opal::ArrayView<const Opal::Ref<const CommandBuffer>> command_buffers;
    Opal::ArrayView<const SemaphoreSubmit> wait_semaphores;
    Opal::ArrayView<const SemaphoreSubmit> signal_semaphores;
    /** Signalled once the whole batch has finished. Empty when nothing on the host waits for it. */
    Opal::Ref<const Fence> fence;
};

class DeviceQueue
{
public:
    DeviceQueue() = default;
    ~DeviceQueue();

    void Destroy();

    explicit DeviceQueue(const Device& device, u32 queue_family_index);

    DeviceQueue(const DeviceQueue&) = delete;
    DeviceQueue& operator=(const DeviceQueue&) = delete;
    DeviceQueue(DeviceQueue&& other) noexcept;
    DeviceQueue& operator=(DeviceQueue&& other) noexcept;

    [[nodiscard]] bool IsValid() const { return m_queue != VK_NULL_HANDLE; }
    [[nodiscard]] VkQueue GetNativeQueue() const { return m_queue; }
    [[nodiscard]] VkCommandPool GetNativeCommandPool() const { return m_command_pool; }
    [[nodiscard]] u32 GetQueueFamilyIndex() const { return m_queue_family_index; }

    /**
     * Submit the work a SubmitDesc describes as one batch.
     * @param desc Command buffers, the semaphores to wait on and to signal, and the fence to signal at the end.
     */
    void Submit(const SubmitDesc& desc);

    /** One command buffer, one fence, nothing to synchronize against on the device. */
    void Submit(const CommandBuffer& command_buffer, const Fence& fence);

    /** One command buffer between one wait and one signal. An empty semaphore on either side is skipped. */
    void Submit(const CommandBuffer& command_buffer, const Semaphore& wait_semaphore,
                PipelineStageBits wait_stage, const Semaphore& signal_semaphore, const Fence& fence);

    /**
     * Block until everything submitted to this queue has finished. Coarser than a fence and meant for
     * shutdown and for one-off setup work, not for the frame loop.
     */
    void WaitIdle() const;

private:

    friend class Device;
    friend class Opal::SharedPtr<DeviceQueue>;

    Opal::Ref<const Device> m_device;
    u32 m_queue_family_index = 0;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkCommandPool m_command_pool = VK_NULL_HANDLE;
};

class Device
{
public:
    Device() = default;
    explicit Device(PhysicalDevice physical_device, const GraphicsContext& graphics_context,
                            const DeviceDesc& desc = {});
    ~Device();

    Device(const Device&) = delete;
    const Device& operator=(const Device&) = delete;
    Device(Device&& other) noexcept;
    Device& operator=(Device&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_device != VK_NULL_HANDLE; }
    [[nodiscard]] VkDevice GetNativeDevice() const { return m_device; }
    [[nodiscard]] const PhysicalDevice& GetPhysicalDevice() const { return m_physical_device; }
    [[nodiscard]] VkPhysicalDevice GetNativePhysicalDevice() const { return m_physical_device.GetNativePhysicalDevice(); }
    [[nodiscard]] const DeviceDesc& GetDesc() const { return m_desc; }

    /**
     * Whether the device was created with the named extension. Commands that belong to an extension have to ask,
     * since the loader hands out a callable trampoline for them either way and calling one the device did not
     * enable crashes rather than failing.
     * @param extension_name Extension to look for, such as VK_EXT_MESH_SHADER_EXTENSION_NAME.
     */
    [[nodiscard]] bool IsExtensionEnabled(const char* extension_name) const;

    /**
     * Whether the instance this device came from enabled VK_EXT_debug_utils, which is what decides whether
     * SetDebugName does anything. See GraphicsContext::AreDebugUtilsEnabled.
     */
    [[nodiscard]] bool AreDebugUtilsEnabled() const { return m_debug_utils_enabled; }

    /** Queue of the given family. Throws when the device was not created with one, so the result is always valid. */
    DeviceQueue& GetQueue(QueueFamily queue_family);
    [[nodiscard]] const DeviceQueue& GetQueue(QueueFamily queue_family) const;

    [[nodiscard]] VmaAllocator GetGPUAllocator() const { return m_gpu_allocator; }

    void WaitForAll() const;

private:
    void CollectQueueFamilies(Opal::DynamicArray<VkDeviceQueueCreateInfo>& queue_create_infos);

    /** Point every queue back at this device, which a move has to do since the queues hold a reference to it. */
    void RepointQueues();

    VkDevice m_device = VK_NULL_HANDLE;
    Opal::HashMap<QueueFamily, Opal::SharedPtr<DeviceQueue>> m_queue_family_to_queue;
    PhysicalDevice m_physical_device;
    DeviceDesc m_desc;
    /** What was actually passed to vkCreateDevice, which is the desc plus what the device adds on its own. */
    Opal::DynamicArray<const char*> m_enabled_extensions;
    QueueFamilyIndices m_queue_family_indices;
    VmaAllocator m_gpu_allocator = VK_NULL_HANDLE;
    bool m_debug_utils_enabled = false;
};

}  // namespace Rndr::Forge