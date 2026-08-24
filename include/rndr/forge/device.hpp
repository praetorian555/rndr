#pragma once

#include "volk/volk.h"

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/container/hash-map.h"
#include "opal/container/ref.h"
#include "opal/container/shared-ptr.h"

#include "rndr/error-codes.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/types.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"

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

/**
 * What a device is asked to turn on, named by what it does rather than by the Vulkan version that introduced
 * it. Forge maps each field onto whichever feature structure Vulkan keeps it in and builds the chain itself,
 * so nothing here has to be kept alive past the call and no caller has to know which release added what.
 *
 * A field that this device does not support is reported at creation, with the log naming the field, rather
 * than failing inside vkCreateDevice with a result that names nothing.
 *
 * Synchronization2 and dynamic rendering are not here: Forge is written on both - every barrier and every
 * CmdBeginRendering - so they are always enabled and turning them off would only break it.
 */
struct DeviceFeatures
{
    // Rasterization and geometry.
    /** Wireframe, which is RasterizerDesc::fill_mode set to FillMode::Wireframe. */
    bool fill_mode_non_solid = false;
    /** Lines thicker than one pixel. */
    bool wide_lines = false;
    /** Clamp depth instead of clipping against the near and far planes. */
    bool depth_clamp = false;
    /** Clamp the depth bias, which RasterizerDesc::depth_bias_clamp asks for. */
    bool depth_bias_clamp = false;
    bool geometry_shader = false;
    bool tessellation_shader = false;
    /** Blend state per color attachment rather than one shared by all of them. */
    bool independent_blend = false;
    /** More than one command in a CmdDrawIndirect or CmdDrawIndexedIndirect. */
    bool multi_draw_indirect = false;
    /** A non-zero first_instance in an indirect draw. */
    bool draw_indirect_first_instance = false;

    // Sampling.
    /** Anisotropic filtering, which SamplerDesc::max_anisotropy above one asks for. */
    bool sampler_anisotropy = true;
    /**
     * The MIRROR_CLAMP_TO_EDGE address mode, which ImageAddressMode::MirrorOnce asks for. Core since Vulkan
     * 1.2 but still a feature rather than something every device does, so a sampler naming that mode on a
     * device created without this throws.
     */
    bool sampler_mirror_clamp_to_edge = false;
    /** BC compressed texture formats. */
    bool texture_compression_bc = false;

    // Shaders.
    bool shader_int16 = false;
    bool shader_int64 = false;
    bool shader_float64 = false;

    // Descriptors.
    /** Indexing into arrays of descriptors. The rest of the descriptor fields build on this one. */
    bool descriptor_indexing = true;
    /** Arrays of descriptors whose length the shader does not have to know. */
    bool runtime_descriptor_array = true;
    /** The variable_descriptor_count argument of DescriptorSet. */
    bool variable_descriptor_count = true;
    /** Bindings whose descriptors need not all be written, which bindless allocation depends on. */
    bool partially_bound_descriptors = false;
    /** DescriptorPoolDesc::use_update_after_bind. */
    bool update_after_bind_descriptors = false;
    /** Indexing descriptor arrays with a value that differs between invocations. */
    bool non_uniform_descriptor_indexing = false;

    // Memory and synchronization.
    /** BufferDesc::use_device_address. */
    bool buffer_device_address = true;
    /** Lets a shader lay out buffer blocks the way C does. */
    bool scalar_block_layout = false;
    /** Reset query pools from the host, which timestamp queries want. */
    bool host_query_reset = false;

    // Extension backed. Asking for one of these enables the extension it belongs to as well.
    /** Task and mesh shaders, which CmdDrawMeshTasks needs. Pulls in VK_EXT_mesh_shader. */
    bool mesh_shader = false;
    /** The task stage in front of the mesh stage. Needs mesh_shader too. */
    bool task_shader = false;
    /**
     * 8-bit indices, which CmdBindIndexBuffer asks for with IndexSize::uint8. Pulls in
     * VK_KHR_index_type_uint8, or the VK_EXT_index_type_uint8 it was promoted from on a device that only
     * has the older name.
     */
    bool index_type_uint8 = false;
};

struct DeviceDesc : Opal::ClonableBase<DeviceDesc>
{
    DeviceFeatures features;
    Opal::DynamicArray<const char*> extensions;
    Opal::Ref<Surface> surface;
    bool use_async_compute_queue = true;
    bool use_dedicated_transfer_queue = true;
    bool use_decode_queue = false;
    bool use_encode_queue = false;

    OPAL_CLONE_FIELDS(features, extensions, surface, use_async_compute_queue, use_dedicated_transfer_queue, use_decode_queue,
                      use_encode_queue);
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
    /** Index of that family, or k_invalid_index when the device has none - including for a value that names no family. */
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
    /** Timeline only: the count to wait for, or the count to signal. Must be zero for a binary semaphore. */
    u64 value = 0;
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

/**
 * Index of the device best suited to being created with this desc, or empty when none of them can be. What
 * the desc asks for is what a device has to provide - its surface, its extensions, its features and its
 * queues - so there is no second description of the requirements to keep in step with it.
 *
 * @param devices Devices to choose from, as EnumeratePhysicalDevices returned them.
 * @param desc The desc the chosen device will be created with.
 * @param prefer_discrete Rank a discrete device above an integrated one. With this off, the first device
 *        that can do the job wins, which is what a machine with one device does either way.
 */
[[nodiscard]] Opal::Optional<u32> FindPhysicalDevice(Opal::ArrayView<const PhysicalDevice> devices, const DeviceDesc& desc = {},
                                                     bool prefer_discrete = true);

/**
 * The device FindPhysicalDevice picked, moved out of the list.
 *
 * @return The device, ErrorCode::NoGraphicsDevice for an empty list, or ErrorCode::FeatureNotSupported when
 *         none of them qualifies. The log names the requirement the last one failed, since "no suitable
 *         device" on its own tells nobody anything.
 */
[[nodiscard]] Opal::Expected<PhysicalDevice, ErrorCode> SelectPhysicalDevice(Opal::ArrayView<PhysicalDevice> devices,
                                                                             const DeviceDesc& desc = {}, bool prefer_discrete = true);

class DeviceQueue
{
public:
    DeviceQueue() = default;
    ~DeviceQueue();

    /**
     * Releases the command pool this queue owns. Public because the constructor below is: a queue built from
     * a device and a family index owns its pool and is its caller's to release, the way every other type here
     * is.
     *
     * A queue that came from Device::GetQueue is not one of those. Those belong to the device and are handed
     * out by reference, so destroying one leaves the device holding a queue with no command pool, and the
     * next command buffer allocated on it fails. Destroy the ones you constructed and leave the rest alone.
     */
    void Destroy();

    /**
     * A queue of the given family on this device, with a command pool of its own.
     *
     * @param device Device to take the queue from. Has to outlive the queue.
     * @param queue_family_index Family the queue belongs to.
     * @return The queue, or whatever the failing command pool creation maps to.
     */
    [[nodiscard]] static Opal::Expected<DeviceQueue, ErrorCode> Create(const Device& device, u32 queue_family_index);

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
     * @return ErrorCode::Success, ErrorCode::InvalidArgument when the desc names an empty command buffer or an
     *         empty semaphore, or gives a value to a semaphore that has no counter, or whatever the failing
     *         submit maps to.
     */
    [[nodiscard]] ErrorCode Submit(const SubmitDesc& desc);

    /** One command buffer, one fence, nothing to synchronize against on the device. */
    [[nodiscard]] ErrorCode Submit(const CommandBuffer& command_buffer, const Fence& fence);

    /**
     * Block until everything submitted to this queue has finished. Coarser than a fence and meant for
     * shutdown and for one-off setup work, not for the frame loop.
     * @return ErrorCode::Success, or whatever the failing wait maps to.
     */
    [[nodiscard]] ErrorCode WaitIdle() const;

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
    ~Device();

    /**
     * Create the logical device, its queues and its allocator.
     *
     * @param physical_device The device to create it on, moved in. SelectPhysicalDevice picks one.
     * @param graphics_context Context the physical device came from. Has to outlive the device.
     * @param desc Features, extensions, surface and which queues to take.
     * @return The device, ErrorCode::InvalidArgument for an empty physical device,
     *         ErrorCode::FeatureNotSupported when the desc asks for an extension, a feature or a queue this
     *         device does not have - the log names which - or whatever the failing Vulkan call maps to.
     */
    [[nodiscard]] static Opal::Expected<Device, ErrorCode> Create(PhysicalDevice physical_device, const GraphicsContext& graphics_context,
                                                                  const DeviceDesc& desc = {});

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

    /** The features this device was created with, for the guards that have to ask before using one. */
    [[nodiscard]] const DeviceFeatures& GetFeatures() const { return m_desc.features; }

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

    /**
     * Queue of the given family.
     * @return The queue, or ErrorCode::InvalidArgument when the device was not created with one.
     */
    [[nodiscard]] Opal::Expected<DeviceQueue&, ErrorCode> GetQueue(QueueFamily queue_family);
    [[nodiscard]] Opal::Expected<const DeviceQueue&, ErrorCode> GetQueue(QueueFamily queue_family) const;

    [[nodiscard]] VmaAllocator GetGPUAllocator() const { return m_gpu_allocator; }

    /**
     * Block until everything submitted to any of this device's queues has finished.
     * @return ErrorCode::Success, or whatever the failing wait maps to.
     */
    [[nodiscard]] ErrorCode WaitForAll() const;

private:
    [[nodiscard]] ErrorCode CollectQueueFamilies(Opal::DynamicArray<VkDeviceQueueCreateInfo>& queue_create_infos);

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