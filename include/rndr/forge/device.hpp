#pragma once

#include "volk.h"

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/container/hash-map.h"
#include "opal/container/ref.h"
#include "opal/container/scope-ptr.h"
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

class RenderPassCache;
struct RenderPassKey;

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
 * The optional capabilities of a device, named by what they do rather than by the Vulkan version that introduced
 * them. Forge maps each field onto whichever feature structure Vulkan keeps it in and builds the chain itself,
 * so nothing here has to be kept alive past the call and no caller has to know which release added what.
 *
 * Used two ways. In DeviceDesc::features a field set true is a requirement: a device that does not support it
 * is passed over by SelectPhysicalDevice and refused by Device::Create, with the log naming the field, rather
 * than failing inside vkCreateDevice with a result that names nothing. Everything not required is turned on
 * anyway wherever the device supports it, unless DeviceDesc::enable_supported_features is off. From
 * Device::GetEnabledFeatures, it is what the device ended up with, and what to ask before relying on a field that was
 * not required. A field below that says it is refused without the feature means without it in GetEnabledFeatures.
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
    /** A geometry stage between the vertex and the fragment one, which GraphicsPipelineDesc::geometry_shader names. */
    bool geometry_shader = false;
    /** The two tessellation stages, and with them PrimitiveTopology::Patch. */
    bool tessellation_shader = false;
    /**
     * A pass that renders every view named by RenderingDesc::view_mask with one draw, each view into the
     * attachment layer of its index - the pipeline says the same mask in GraphicsPipelineDesc::view_mask.
     * Forge asks for multiview alone, so a pipeline with a geometry, tessellation or mesh stage takes no mask.
     */
    bool multiview = false;
    /**
     * SV_RenderTargetArrayIndex written by a vertex shader, which picks the attachment layer of a layered
     * pass (RenderingDesc::layer_count) without a geometry stage to do it.
     */
    bool shader_output_layer = false;
    /**
     * DescriptorType::InputAttachment inside a dynamic rendering pass, which is how a tiled device reads a
     * G-buffer without it leaving tile memory. VK_KHR_dynamic_rendering_local_read, enabled with it.
     */
    bool dynamic_rendering_local_read = false;
    /** Blend state per color attachment rather than one shared by all of them. */
    bool independent_blend = false;
    /** More than one command in a CmdDrawIndirect or CmdDrawIndexedIndirect. */
    bool multi_draw_indirect = false;
    /** A non-zero first_instance in an indirect draw. */
    bool draw_indirect_first_instance = false;
    /**
     * CmdDrawIndirectCount and CmdDrawIndexedIndirectCount, whose command count the device reads out of a
     * buffer - which is what lets a culling pass on the device decide how many draws there are.
     */
    bool draw_indirect_count = false;

    // Sampling.
    /** Anisotropic filtering, which SamplerDesc::max_anisotropy above one asks for. */
    bool sampler_anisotropy = true;
    /**
     * The MIRROR_CLAMP_TO_EDGE address mode, which ImageAddressMode::MirrorOnce asks for. Core since Vulkan
     * 1.2 but still a feature rather than something every device does, so a sampler naming that mode on a
     * device created without this is refused.
     */
    bool sampler_mirror_clamp_to_edge = false;
    /** SamplerDesc::reduction other than WeightedAverage: a filter returning the least or the greatest texel it reads. */
    bool sampler_filter_minmax = false;
    /** BC compressed texture formats, which a texture naming one is refused without. */
    bool texture_compression_bc = false;
    /** Views of TextureViewType::CubeArray, which a texture naming one is refused without. */
    bool image_cube_array = false;

    // Shaders.
    bool shader_int16 = false;
    bool shader_int64 = false;
    bool shader_float64 = false;
    /**
     * 8-bit integers in shader arithmetic and constants - a uint8_t specialization constant among them, which
     * reflection reports with a byte_size of one. Reading or writing them in a buffer is a storage feature
     * of its own that Forge does not ask for.
     */
    bool shader_int8 = false;
    /** half arithmetic in a shader. As with shader_int8, a half in a buffer needs a storage feature beside it. */
    bool shader_float16 = false;
    /**
     * 64-bit atomics on storage buffers - an InterlockedMax over a u64, which is how a software rasterizer
     * resolves depth and the id beside it in one operation. Needs shader_int64 for the type itself.
     */
    bool shader_buffer_int64_atomics = false;
    /** 64-bit atomics on groupshared memory, which a workgroup reducing into one u64 before writing it out wants. */
    bool shader_shared_int64_atomics = false;
    /**
     * Vulkan 1.3's maintenance4, asked for for what it lets a shader do: take its workgroup size from
     * specialization constants. Slang compiles `[numthreads(GROUP_SIZE, 1, 1)]` over a [SpecializationConstant]
     * to the LocalSizeId execution mode, which the layer refuses without this - so one compute shader can be
     * built as pipelines of different workgroup sizes, set through ComputePipelineDesc::specialization like any
     * other constant. On a Vulkan 1.1 build it pulls in VK_KHR_maintenance4.
     */
    bool maintenance4 = false;

    // Descriptors. None required by default: a renderer that binds a fixed set of resources per draw needs none
    // of them. Bindless - one large array of every texture or buffer, indexed by a number the draw pushes - wants
    // most of them together, which BindlessFeatures() below hands out. On a Vulkan 1.1 build any of them pulls
    // in VK_EXT_descriptor_indexing.
    /**
     * Descriptor arrays the shader declares without a length, the form a bindless table takes:
     * `Texture2D textures[];` or `RWStructuredBuffer<uint> buffers[];`. The length comes from the layout, or from
     * the set with variable_descriptor_count. Without it a shader with such an array is refused by the layer.
     */
    bool runtime_descriptor_array = false;
    /**
     * DescriptorBindingFlagBits::VariableDescriptorCount: a set picks the length of its last binding when it is
     * allocated - the variable_descriptor_count argument of DescriptorSet::Create - up to the count the layout
     * declares. One layout then serves a table of 16 textures and one of 10,000, and the pool only pays for what
     * each set asked for.
     */
    bool variable_descriptor_count = false;
    /**
     * DescriptorBindingFlagBits::PartiallyBound: descriptors of the binding may be left unwritten, or keep a
     * resource that has since been destroyed, as long as no shader reads them. Without it every descriptor a
     * pipeline could statically reach has to be valid at draw time, which a table with free slots never is.
     */
    bool partially_bound_descriptors = false;
    /**
     * DescriptorBindingFlagBits::UpdateAfterBind and DescriptorPoolDesc::use_update_after_bind: descriptors may
     * be written after the set is bound in a command buffer and while that work runs on the device, as long as
     * no shader invocation reads them. A bindless table then takes new textures while frames are in flight,
     * rather than waiting for the device or keeping one table per frame. Such bindings also get the higher
     * maxDescriptorSetUpdateAfterBind* limits, which is how a table grows past the ordinary per-stage ones.
     */
    bool update_after_bind_descriptors = false;
    /**
     * DescriptorBindingFlagBits::UpdateUnusedWhilePending: descriptors that no pending command buffer uses may be
     * written after the set is bound and while those command buffers are pending execution. Beside
     * UpdateAfterBind, "uses" means read by a shader invocation; alone, it means reachable by the pipeline at
     * all. Lets slots nobody is drawing with be refilled without waiting for the frame to finish.
     */
    bool update_unused_while_pending_descriptors = false;
    /**
     * Indexing a descriptor array with a value that differs between invocations of one draw or dispatch - a
     * material index read per pixel, per instance or per thread. The index has to be wrapped as
     * `textures[NonUniformResourceIndex(index)]` as well; without both, only an index that is the same across
     * the whole draw, such as one from a push constant, is defined.
     */
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
     * VK_EXT_index_type_uint8, or the VK_KHR_index_type_uint8 it was promoted to on a device that only
     * has the newer name.
     */
    bool index_type_uint8 = false;
};

/**
 * The requirements of a bindless renderer: unsized descriptor arrays, a length chosen per set, slots left empty,
 * an index that differs per invocation, and writes to the table while frames that read it are in flight. Handed
 * to SelectPhysicalDevice and Device::Create as DeviceDesc::features, it picks a device that has all of them and
 * reports the one a device lacks by name. A renderer that can do without one - update_after_bind_descriptors is
 * the one mobile devices limit - stops requiring it, and asks Device::GetEnabledFeatures whether it came anyway.
 */
[[nodiscard]] constexpr DeviceFeatures BindlessFeatures()
{
    return {.runtime_descriptor_array = true,
            .variable_descriptor_count = true,
            .partially_bound_descriptors = true,
            .update_after_bind_descriptors = true,
            .non_uniform_descriptor_indexing = true};
}

struct DeviceDesc : Opal::ClonableBase<DeviceDesc>
{
    /**
     * What the device has to have. Every field set here is a requirement: SelectPhysicalDevice passes over a
     * device that lacks one, and Device::Create refuses it with the field named in the log.
     */
    DeviceFeatures features;
    /**
     * Turn on, beside what `features` requires, every other field of DeviceFeatures this device supports. Enabling
     * a feature costs nothing at run time, and leaving one off only moves the failure to the pipeline, sampler or
     * command that needed it, so this is on by default: a device then does whatever the machine can, and
     * Device::GetEnabledFeatures says what that came to. A field left false here means "not required", not "keep off".
     *
     * Off, the device turns on exactly what `features` names. That is for a test of a refusal, which needs the
     * feature off on a device that has it, and for a driver that misbehaves with a feature it reports.
     */
    bool enable_supported_features = true;
    Opal::DynamicArray<const char*> extensions;
    /**
     * Surface the present queue is picked against. The precise way to ask for presentation when a window
     * already exists; a device created before any window asks with enable_presentation instead, and checks a
     * surface that arrives later with Device::CanPresentTo. Only read during creation - the device never
     * touches it again, so it need not outlive the device.
     */
    Opal::Ref<Surface> surface;
    /**
     * Ask for a present queue and the swap chain extension without naming a surface, so the device can be
     * created before any window exists and swap chains built over surfaces that arrive later. The family is
     * picked with the platform's surface-free query (see PhysicalDevice::GetPresentQueueFamilyIndex()); that
     * it can present to a particular surface is verified when a swap chain is created over one. Ignored when
     * `surface` is set, which answers the same question more precisely.
     */
    bool enable_presentation = false;
    bool use_async_compute_queue = false;
    bool use_dedicated_transfer_queue = false;
    bool use_decode_queue = false;
    bool use_encode_queue = false;

    OPAL_CLONE_FIELDS(features, enable_supported_features, extensions, surface, enable_presentation, use_async_compute_queue,
                      use_dedicated_transfer_queue, use_decode_queue, use_encode_queue);
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
Opal::Expected<PhysicalDevice, ErrorCode> SelectPhysicalDevice(Opal::ArrayView<PhysicalDevice> devices,
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
    ErrorCode Submit(const SubmitDesc& desc);

    /** One command buffer, one fence, nothing to synchronize against on the device. */
    ErrorCode Submit(const CommandBuffer& command_buffer, const Fence& fence);

    /**
     * Block until everything submitted to this queue has finished. Coarser than a fence and meant for
     * shutdown and for one-off setup work, not for the frame loop.
     * @return ErrorCode::Success, or whatever the failing wait maps to.
     */
    ErrorCode WaitIdle() const;

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
    /** Out of line, since it has to be able to destroy the render pass cache, which only the source can see. */
    Device();
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

    /**
     * The features that are on: what DeviceDesc::features required, plus, unless
     * DeviceDesc::enable_supported_features was off, every other one this device supports. What the guards ask
     * before using a feature, and what a caller asks before relying on one it did not require. GetDesc().features
     * keeps what was required.
     */
    [[nodiscard]] const DeviceFeatures& GetEnabledFeatures() const { return m_features; }

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
     * Whether this device records its passes with VkRenderPass and VkFramebuffer objects instead of dynamic rendering,
     * which is what a RNDR_FORGE_VULKAN_1_1 build does on a device without VK_KHR_dynamic_rendering. CmdBeginRendering
     * and pipeline creation take care of the difference, so nothing a caller writes changes; it is here to be asked.
     */
    [[nodiscard]] bool UsesRenderPasses() const { return m_render_pass_cache.IsValid(); }

    /**
     * Whether this device has synchronization2. Always on 1.3; a RNDR_FORGE_VULKAN_1_1 build takes a device without
     * VK_KHR_synchronization2 too, and records its barriers, submits and timestamps with the commands synchronization2
     * replaced (src/forge/synchronization2.cpp). Nothing a caller writes changes; it is here to be asked.
     */
    [[nodiscard]] bool HasSynchronization2() const { return m_has_synchronization2; }

    /**
     * Whether this device can make timeline semaphores. Always on 1.3; a RNDR_FORGE_VULKAN_1_1 build takes a device
     * without VK_KHR_timeline_semaphore too, and on it Semaphore::Create refuses SemaphoreType::Timeline with
     * ErrorCode::FeatureNotSupported. FrameContext paces its frames with fences there instead, so a frame loop works
     * the same either way.
     */
    [[nodiscard]] bool HasTimelineSemaphores() const { return m_has_timeline_semaphores; }

    /**
     * Whether a shader may read, or write, a storage image without naming its format - what Slang emits for a RWTexture
     * with no format attribute. Always on 1.3, where it is core. A RNDR_FORGE_VULKAN_1_1 build gets it from
     * VK_KHR_format_feature_flags2 when the device has that, and from the shaderStorageImageReadWithoutFormat and
     * shaderStorageImageWriteWithoutFormat features when it has only those, which it then turns on by itself. Where
     * neither is there, Shader creation refuses a module that needs it.
     */
    [[nodiscard]] bool CanReadStorageImagesWithoutFormat() const { return m_can_read_storage_images_without_format; }
    [[nodiscard]] bool CanWriteStorageImagesWithoutFormat() const { return m_can_write_storage_images_without_format; }

    /**
     * The render pass this device records a pass of this shape with, made the first time it is asked for and kept
     * until the device goes. Only on a device that UsesRenderPasses; Forge's own, for CommandBuffer and Pipeline.
     * @return The render pass, ErrorCode::InvalidArgument on a device that renders dynamically, or whatever the failing
     *         creation maps to.
     */
    [[nodiscard]] Opal::Expected<VkRenderPass, ErrorCode> GetRenderPass(const RenderPassKey& key) const;

    /**
     * Whether this device's present queue family can present to the given surface - what every swap chain has
     * to ask of the surface it is built over, asked on its own so that a device created before any window,
     * with DeviceDesc::enable_presentation, can be checked against a surface the moment one exists.
     *
     * The device presents to no surface of its own: a window's surface belongs to the swap chain built over
     * it, and a device drives as many of those at once as there are windows. This only answers the question.
     *
     * False, rather than an error, for a device created without presentation: that is an answer.
     * @return The answer, ErrorCode::InvalidArgument for an empty device or an empty surface, or whatever the
     *         failing query maps to.
     */
    [[nodiscard]] Opal::Expected<bool, ErrorCode> CanPresentTo(const Surface& surface) const;

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
    ErrorCode WaitForAll() const;

private:
    ErrorCode CollectQueueFamilies(Opal::DynamicArray<VkDeviceQueueCreateInfo>& queue_create_infos);

    /** Point every queue back at this device, which a move has to do since the queues hold a reference to it. */
    void RepointQueues();

    VkDevice m_device = VK_NULL_HANDLE;
    Opal::HashMap<QueueFamily, Opal::SharedPtr<DeviceQueue>> m_queue_family_to_queue;
    PhysicalDevice m_physical_device;
    DeviceDesc m_desc;
    /** What was turned on, which is m_desc.features plus what enable_supported_features found; see GetEnabledFeatures. */
    DeviceFeatures m_features;
    /** What was actually passed to vkCreateDevice, which is the desc plus what the device adds on its own. */
    Opal::DynamicArray<const char*> m_enabled_extensions;
    QueueFamilyIndices m_queue_family_indices;
    VmaAllocator m_gpu_allocator = VK_NULL_HANDLE;
    bool m_debug_utils_enabled = false;
    /** Set only on a device without dynamic rendering; see UsesRenderPasses. */
    Opal::ScopePtr<RenderPassCache> m_render_pass_cache;
    bool m_has_synchronization2 = true;
    bool m_has_timeline_semaphores = true;
    bool m_can_read_storage_images_without_format = true;
    bool m_can_write_storage_images_without_format = true;
};

}  // namespace Rndr::Forge