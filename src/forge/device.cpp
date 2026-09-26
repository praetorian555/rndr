#include "rndr/forge/device.hpp"

#define NOMINMAX
#include "vk_mem_alloc.h"

#include "opal/container/hash-set.h"

#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/forge/swap-chain.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/log.hpp"

Opal::DynamicArray<Rndr::u32> Rndr::Forge::QueueFamilyIndices::GetValidQueueFamilies() const
{
    Opal::HashSet<u32> unique_indices(6);
    Opal::DynamicArray<u32> valid_queue_families;
    if (graphics_family != k_invalid_index)
    {
        unique_indices.Insert(graphics_family);
    }
    if (present_family != k_invalid_index)
    {
        unique_indices.Insert(present_family);
    }
    if (transfer_family != k_invalid_index)
    {
        unique_indices.Insert(transfer_family);
    }
    if (compute_family != k_invalid_index)
    {
        unique_indices.Insert(compute_family);
    }
    if (encode_family_index != k_invalid_index)
    {
        unique_indices.Insert(encode_family_index);
    }
    if (decode_family_index != k_invalid_index)
    {
        unique_indices.Insert(decode_family_index);
    }
    for (auto index : unique_indices)
    {
        valid_queue_families.PushBack(index);
    }
    return valid_queue_families;
}

Rndr::u32 Rndr::Forge::QueueFamilyIndices::GetQueueFamilyIndex(QueueFamily queue_family) const
{
    switch (queue_family)
    {
        case QueueFamily::Graphics:
            return graphics_family;
        case QueueFamily::Present:
            return present_family;
        case QueueFamily::Transfer:
            return transfer_family;
        case QueueFamily::AsyncCompute:
            return compute_family;
        case QueueFamily::Decode:
            return decode_family_index;
        case QueueFamily::Encode:
            return encode_family_index;
        default:
            // A value that names no family has no index, which is the same answer as a device that was not
            // created with that family - and k_invalid_index is what every caller already checks for.
            return k_invalid_index;
    }
}

namespace
{
using namespace Rndr;

#if defined(RNDR_FORGE_VULKAN_1_1)
/**
 * What a device on Vulkan 1.1 needs before it can do dynamic rendering: the extension, and the two it is built on.
 * Those two are built on multiview and maintenance2, which 1.1 made core.
 */
constexpr const char* k_dynamic_rendering_extensions[] = {
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};
#endif

/**
 * Every feature structure Vulkan keeps its features in, chained together once. It exists so that the mapping
 * from Forge's flat DeviceFeatures onto them is written in one place and used twice: to ask the device what
 * it supports, and to tell it what to enable.
 *
 * vk11, vk12 and vk13 hold the features by the version that made them core, and the rest of this file reads and
 * writes them there in either build. On Vulkan 1.3 they are what gets chained. On Vulkan 1.1 (RNDR_FORGE_VULKAN_1_1)
 * the device knows none of the three, so each feature travels in the structure of the extension it was promoted
 * from, and Query and Fill copy between those and the three; a feature whose extension has no structure is
 * supported exactly when the extension is there.
 *
 * The chain points at its own members, so an instance of this must not be copied or moved after it is built.
 */
struct FeatureChain
{
    VkPhysicalDeviceFeatures2 features2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan11Features vk11{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    VkPhysicalDeviceVulkan12Features vk12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features vk13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceMeshShaderFeaturesEXT mesh{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    VkPhysicalDeviceIndexTypeUint8Features index_type_uint8{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES};
    VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR local_read{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR};
#if defined(RNDR_FORGE_VULKAN_1_1)
    VkPhysicalDeviceMultiviewFeatures multiview{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES};
    VkPhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
    VkPhysicalDeviceSynchronization2Features synchronization2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};
    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
    VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
    VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    VkPhysicalDeviceScalarBlockLayoutFeatures scalar_block_layout{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES};
    VkPhysicalDeviceHostQueryResetFeatures host_query_reset{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES};
    VkPhysicalDeviceShaderFloat16Int8Features float16_int8{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES};
    VkPhysicalDeviceShaderAtomicInt64Features atomic_int64{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES};

    /** Which extensions the chain was built with, for the features they carry without a structure. */
    bool has_descriptor_indexing = false;
    bool has_sampler_mirror_clamp_to_edge = false;
    bool has_sampler_filter_minmax = false;
    bool has_draw_indirect_count = false;
    bool has_dynamic_rendering = false;
#endif

    /**
     * Chaining a structure that belongs to an extension the device does not have is not allowed, so each of
     * the extension backed ones goes in only once that extension is known to be there. The tail is walked
     * rather than each structure naming the next, so that which ones are in does not decide the order.
     *
     * @param has_extension Called with an extension name, true when it is in: supported by the device when the chain
     *                      asks what the device can do, enabled on it when the chain says what to turn on.
     */
    template <typename HasExtension>
    explicit FeatureChain(const HasExtension& has_extension)
    {
        m_tail = reinterpret_cast<VkBaseOutStructure*>(&features2);
        m_tail->pNext = nullptr;
#if defined(RNDR_FORGE_VULKAN_1_1)
        Append(&multiview);
        AppendIf(has_extension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME), &timeline_semaphore);
        AppendIf(has_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME), &synchronization2);
        has_dynamic_rendering = true;
        for (const char* name : k_dynamic_rendering_extensions)
        {
            has_dynamic_rendering = has_dynamic_rendering && has_extension(name);
        }
        AppendIf(has_dynamic_rendering, &dynamic_rendering);
        has_descriptor_indexing = has_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        AppendIf(has_descriptor_indexing, &descriptor_indexing);
        AppendIf(has_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME), &buffer_device_address);
        AppendIf(has_extension(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME), &scalar_block_layout);
        AppendIf(has_extension(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME), &host_query_reset);
        AppendIf(has_extension(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME), &float16_int8);
        AppendIf(has_extension(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME), &atomic_int64);
        has_sampler_mirror_clamp_to_edge = has_extension(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME);
        has_sampler_filter_minmax = has_extension(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME);
        has_draw_indirect_count = has_extension(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
#else
        Append(&vk11);
        Append(&vk12);
        Append(&vk13);
#endif
        AppendIf(has_extension(VK_EXT_MESH_SHADER_EXTENSION_NAME), &mesh);
        AppendIf(has_extension(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME) || has_extension(VK_KHR_INDEX_TYPE_UINT8_EXTENSION_NAME),
                 &index_type_uint8);
        AppendIf(has_extension(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME), &local_read);
    }

    FeatureChain(const FeatureChain&) = delete;
    FeatureChain& operator=(const FeatureChain&) = delete;

    /** Ask the device what it supports, into every structure in the chain and into vk11, vk12 and vk13. */
    void Query(VkPhysicalDevice physical_device)
    {
        vkGetPhysicalDeviceFeatures2(physical_device, &features2);
#if defined(RNDR_FORGE_VULKAN_1_1)
        vk11.multiview = multiview.multiview;

        vk12.descriptorIndexing = has_descriptor_indexing ? VK_TRUE : VK_FALSE;
        vk12.runtimeDescriptorArray = descriptor_indexing.runtimeDescriptorArray;
        vk12.descriptorBindingVariableDescriptorCount = descriptor_indexing.descriptorBindingVariableDescriptorCount;
        vk12.descriptorBindingPartiallyBound = descriptor_indexing.descriptorBindingPartiallyBound;
        vk12.descriptorBindingSampledImageUpdateAfterBind = descriptor_indexing.descriptorBindingSampledImageUpdateAfterBind;
        vk12.descriptorBindingStorageBufferUpdateAfterBind = descriptor_indexing.descriptorBindingStorageBufferUpdateAfterBind;
        vk12.descriptorBindingStorageImageUpdateAfterBind = descriptor_indexing.descriptorBindingStorageImageUpdateAfterBind;
        vk12.descriptorBindingUniformBufferUpdateAfterBind = descriptor_indexing.descriptorBindingUniformBufferUpdateAfterBind;
        vk12.descriptorBindingUpdateUnusedWhilePending = descriptor_indexing.descriptorBindingUpdateUnusedWhilePending;
        vk12.shaderSampledImageArrayNonUniformIndexing = descriptor_indexing.shaderSampledImageArrayNonUniformIndexing;
        vk12.shaderStorageBufferArrayNonUniformIndexing = descriptor_indexing.shaderStorageBufferArrayNonUniformIndexing;
        vk12.shaderStorageImageArrayNonUniformIndexing = descriptor_indexing.shaderStorageImageArrayNonUniformIndexing;
        vk12.shaderUniformBufferArrayNonUniformIndexing = descriptor_indexing.shaderUniformBufferArrayNonUniformIndexing;
        vk12.bufferDeviceAddress = buffer_device_address.bufferDeviceAddress;
        vk12.scalarBlockLayout = scalar_block_layout.scalarBlockLayout;
        vk12.hostQueryReset = host_query_reset.hostQueryReset;
        vk12.samplerMirrorClampToEdge = has_sampler_mirror_clamp_to_edge ? VK_TRUE : VK_FALSE;
        vk12.samplerFilterMinmax = has_sampler_filter_minmax ? VK_TRUE : VK_FALSE;
        vk12.drawIndirectCount = has_draw_indirect_count ? VK_TRUE : VK_FALSE;
        vk12.shaderInt8 = float16_int8.shaderInt8;
        vk12.shaderFloat16 = float16_int8.shaderFloat16;
        vk12.shaderBufferInt64Atomics = atomic_int64.shaderBufferInt64Atomics;
        vk12.shaderSharedInt64Atomics = atomic_int64.shaderSharedInt64Atomics;
        // VK_EXT_shader_viewport_index_layer would carry it, but Slang (2026.10) writes the layer from a vertex stage
        // with the ShaderLayer capability, which SPIR-V only has from 1.5, whatever older version it targets. No
        // module this build compiles for it would load, so no device is said to have it.
        vk12.shaderOutputLayer = VK_FALSE;
        vk12.timelineSemaphore = timeline_semaphore.timelineSemaphore;

        vk13.synchronization2 = synchronization2.synchronization2;
        // The extension structure is left out of the chain unless all three extensions are there, and then it
        // reports the feature; without them the feature is not there however the device answers.
        vk13.dynamicRendering = has_dynamic_rendering ? dynamic_rendering.dynamicRendering : VK_FALSE;
#endif
    }

    /** Set the Vulkan field behind every Forge field that was asked for. */
    void Fill(const Forge::DeviceFeatures& features)
    {
        features2.features.fillModeNonSolid = features.fill_mode_non_solid;
        features2.features.wideLines = features.wide_lines;
        features2.features.depthClamp = features.depth_clamp;
        features2.features.depthBiasClamp = features.depth_bias_clamp;
        features2.features.geometryShader = features.geometry_shader;
        features2.features.tessellationShader = features.tessellation_shader;
        features2.features.independentBlend = features.independent_blend;
        features2.features.multiDrawIndirect = features.multi_draw_indirect;
        features2.features.drawIndirectFirstInstance = features.draw_indirect_first_instance;
        features2.features.samplerAnisotropy = features.sampler_anisotropy;
        features2.features.textureCompressionBC = features.texture_compression_bc;
        features2.features.imageCubeArray = features.image_cube_array;
        features2.features.shaderInt16 = features.shader_int16;
        features2.features.shaderInt64 = features.shader_int64;
        features2.features.shaderFloat64 = features.shader_float64;

        vk11.multiview = features.multiview;

        vk12.descriptorIndexing = features.descriptor_indexing;
        vk12.runtimeDescriptorArray = features.runtime_descriptor_array;
        vk12.descriptorBindingVariableDescriptorCount = features.variable_descriptor_count;
        vk12.descriptorBindingPartiallyBound = features.partially_bound_descriptors;
        vk12.descriptorBindingSampledImageUpdateAfterBind = features.update_after_bind_descriptors;
        vk12.descriptorBindingStorageBufferUpdateAfterBind = features.update_after_bind_descriptors;
        vk12.descriptorBindingStorageImageUpdateAfterBind = features.update_after_bind_descriptors;
        vk12.descriptorBindingUniformBufferUpdateAfterBind = features.update_after_bind_descriptors;
        vk12.descriptorBindingUpdateUnusedWhilePending = features.update_unused_while_pending_descriptors;
        vk12.shaderSampledImageArrayNonUniformIndexing = features.non_uniform_descriptor_indexing;
        vk12.shaderStorageBufferArrayNonUniformIndexing = features.non_uniform_descriptor_indexing;
        vk12.shaderStorageImageArrayNonUniformIndexing = features.non_uniform_descriptor_indexing;
        vk12.shaderUniformBufferArrayNonUniformIndexing = features.non_uniform_descriptor_indexing;
        vk12.bufferDeviceAddress = features.buffer_device_address;
        vk12.scalarBlockLayout = features.scalar_block_layout;
        vk12.hostQueryReset = features.host_query_reset;
        vk12.samplerMirrorClampToEdge = features.sampler_mirror_clamp_to_edge;
        vk12.samplerFilterMinmax = features.sampler_filter_minmax;
        vk12.drawIndirectCount = features.draw_indirect_count;
        vk12.shaderInt8 = features.shader_int8;
        vk12.shaderFloat16 = features.shader_float16;
        vk12.shaderBufferInt64Atomics = features.shader_buffer_int64_atomics;
        vk12.shaderSharedInt64Atomics = features.shader_shared_int64_atomics;
        vk12.shaderOutputLayer = features.shader_output_layer;

        // Forge is written on all of these, so they are not the caller's to turn off.
        vk12.timelineSemaphore = VK_TRUE;
        vk13.synchronization2 = VK_TRUE;
        vk13.dynamicRendering = VK_TRUE;

        mesh.meshShader = features.mesh_shader;
        mesh.taskShader = features.task_shader;
        index_type_uint8.indexTypeUint8 = features.index_type_uint8;
        local_read.dynamicRenderingLocalRead = features.dynamic_rendering_local_read;

#if defined(RNDR_FORGE_VULKAN_1_1)
        // The features whose extension has no structure are turned on by enabling it, which
        // CollectDeviceExtensions has done; vk12.descriptorIndexing likewise stands for the extension.
        multiview.multiview = vk11.multiview;
        descriptor_indexing.runtimeDescriptorArray = vk12.runtimeDescriptorArray;
        descriptor_indexing.descriptorBindingVariableDescriptorCount = vk12.descriptorBindingVariableDescriptorCount;
        descriptor_indexing.descriptorBindingPartiallyBound = vk12.descriptorBindingPartiallyBound;
        descriptor_indexing.descriptorBindingSampledImageUpdateAfterBind = vk12.descriptorBindingSampledImageUpdateAfterBind;
        descriptor_indexing.descriptorBindingStorageBufferUpdateAfterBind = vk12.descriptorBindingStorageBufferUpdateAfterBind;
        descriptor_indexing.descriptorBindingStorageImageUpdateAfterBind = vk12.descriptorBindingStorageImageUpdateAfterBind;
        descriptor_indexing.descriptorBindingUniformBufferUpdateAfterBind = vk12.descriptorBindingUniformBufferUpdateAfterBind;
        descriptor_indexing.descriptorBindingUpdateUnusedWhilePending = vk12.descriptorBindingUpdateUnusedWhilePending;
        descriptor_indexing.shaderSampledImageArrayNonUniformIndexing = vk12.shaderSampledImageArrayNonUniformIndexing;
        descriptor_indexing.shaderStorageBufferArrayNonUniformIndexing = vk12.shaderStorageBufferArrayNonUniformIndexing;
        descriptor_indexing.shaderStorageImageArrayNonUniformIndexing = vk12.shaderStorageImageArrayNonUniformIndexing;
        descriptor_indexing.shaderUniformBufferArrayNonUniformIndexing = vk12.shaderUniformBufferArrayNonUniformIndexing;
        buffer_device_address.bufferDeviceAddress = vk12.bufferDeviceAddress;
        scalar_block_layout.scalarBlockLayout = vk12.scalarBlockLayout;
        host_query_reset.hostQueryReset = vk12.hostQueryReset;
        float16_int8.shaderInt8 = vk12.shaderInt8;
        float16_int8.shaderFloat16 = vk12.shaderFloat16;
        atomic_int64.shaderBufferInt64Atomics = vk12.shaderBufferInt64Atomics;
        atomic_int64.shaderSharedInt64Atomics = vk12.shaderSharedInt64Atomics;
        timeline_semaphore.timelineSemaphore = vk12.timelineSemaphore;
        synchronization2.synchronization2 = vk13.synchronization2;
        dynamic_rendering.dynamicRendering = vk13.dynamicRendering;
#endif
    }

private:
    VkBaseOutStructure* m_tail = nullptr;

    void Append(void* structure)
    {
        auto* next = static_cast<VkBaseOutStructure*>(structure);
        next->pNext = nullptr;
        m_tail->pNext = next;
        m_tail = next;
    }

    void AppendIf(bool condition, void* structure)
    {
        if (condition)
        {
            Append(structure);
        }
    }
};

/**
 * The queue families a desc needs, spelled once so that choosing a device and creating one cannot disagree
 * about what "async compute" or "dedicated transfer" means.
 */
constexpr VkQueueFlags k_graphics_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
constexpr VkQueueFlags k_async_compute_flags = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
constexpr VkQueueFlags k_async_compute_not_flags = VK_QUEUE_GRAPHICS_BIT;
constexpr VkQueueFlags k_dedicated_transfer_flags = VK_QUEUE_TRANSFER_BIT;
constexpr VkQueueFlags k_dedicated_transfer_not_flags =
    VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_VIDEO_DECODE_BIT_KHR | VK_QUEUE_VIDEO_ENCODE_BIT_KHR;

/**
 * Which of the two names for the 8-bit index extension this device has, or null when it has neither. The KHR
 * one is the promotion of the EXT one and both carry the same feature structure and index type value, so the
 * choice changes nothing about what the device does. The EXT name goes first because a validation layer
 * that predates the promotion does not credit the KHR name with VK_INDEX_TYPE_UINT8 and reports every
 * 8-bit bind as invalid, while every driver new enough to offer the KHR name still offers the EXT one.
 */
const char* FindIndexTypeUint8Extension(const Forge::PhysicalDevice& physical_device)
{
    if (physical_device.IsExtensionSupported(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME))
    {
        return VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME;
    }
    if (physical_device.IsExtensionSupported(VK_KHR_INDEX_TYPE_UINT8_EXTENSION_NAME))
    {
        return VK_KHR_INDEX_TYPE_UINT8_EXTENSION_NAME;
    }
    return nullptr;
}

/** Every extension a desc implies, which is what it names plus what its surface and its features pull in. */
Opal::DynamicArray<const char*> CollectDeviceExtensions(const Forge::PhysicalDevice& physical_device, const Forge::DeviceDesc& desc)
{
    Opal::DynamicArray<const char*> extensions(desc.extensions.Clone());
    if (desc.surface.IsValid() || desc.enable_presentation)
    {
        extensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    // A feature that lives in an extension only works when the extension is enabled too, so asking for the
    // feature is taken as asking for both rather than as a puzzle for the caller to solve.
    if (desc.features.mesh_shader || desc.features.task_shader)
    {
        extensions.PushBack(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    }
    if (desc.features.index_type_uint8)
    {
        // Neither name present leaves the newer one to be reported as unsupported, so a device that cannot do
        // this says which extension it is missing rather than enabling nothing and failing later.
        const char* name = FindIndexTypeUint8Extension(physical_device);
        extensions.PushBack(name != nullptr ? name : VK_KHR_INDEX_TYPE_UINT8_EXTENSION_NAME);
    }
    if (desc.features.dynamic_rendering_local_read)
    {
        extensions.PushBack(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME);
    }
#if defined(RNDR_FORGE_VULKAN_1_1)
    // What 1.2 and 1.3 made core, which a device asked for 1.1 only has as extensions: the three Forge is written on
    // always, and each of the others once a feature it carries is asked for.
    extensions.PushBack(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
    extensions.PushBack(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    // Shaders are SPIR-V 1.4 in this build (k_spirv_profile), which 1.2 made core and 1.1 has through this extension,
    // built on the float controls one.
    extensions.PushBack(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    extensions.PushBack(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    for (const char* name : k_dynamic_rendering_extensions)
    {
        extensions.PushBack(name);
    }
    // 1.3 lets a shader read or write a storage image without naming its format, which is what Slang emits for a
    // RWTexture, wherever the format allows it; this is where 1.3 got that. Taken when it is there, not demanded,
    // since only such a shader needs it.
    if (physical_device.IsExtensionSupported(VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME))
    {
        extensions.PushBack(VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME);
    }
    const Forge::DeviceFeatures& features = desc.features;
    if (features.descriptor_indexing || features.runtime_descriptor_array || features.variable_descriptor_count ||
        features.partially_bound_descriptors || features.update_after_bind_descriptors ||
        features.update_unused_while_pending_descriptors || features.non_uniform_descriptor_indexing)
    {
        extensions.PushBack(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    }
    if (features.buffer_device_address)
    {
        extensions.PushBack(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    }
    if (features.scalar_block_layout)
    {
        extensions.PushBack(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);
    }
    if (features.host_query_reset)
    {
        extensions.PushBack(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME);
    }
    if (features.sampler_mirror_clamp_to_edge)
    {
        extensions.PushBack(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME);
    }
    if (features.sampler_filter_minmax)
    {
        extensions.PushBack(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME);
    }
    if (features.draw_indirect_count)
    {
        extensions.PushBack(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
    }
    if (features.shader_int8 || features.shader_float16)
    {
        extensions.PushBack(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
    }
    if (features.shader_buffer_int64_atomics || features.shader_shared_int64_atomics)
    {
        extensions.PushBack(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME);
    }
#endif
    return extensions;
}

/**
 * The first requested feature this device does not support, or null when it supports them all. Reporting
 * rather than throwing, so that choosing between devices and creating one both go through it.
 */
const char* FindUnsupportedFeature(const Forge::PhysicalDevice& physical_device, const Forge::DeviceFeatures& requested)
{
    const bool has_mesh_extension = physical_device.IsExtensionSupported(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    const char* index_type_uint8_extension = FindIndexTypeUint8Extension(physical_device);
    const bool has_local_read_extension = physical_device.IsExtensionSupported(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME);
    FeatureChain supported([&physical_device](const char* name) { return physical_device.IsExtensionSupported(name); });
    supported.Query(physical_device.GetNativePhysicalDevice());

    const char* missing = nullptr;
    auto require = [&missing](bool is_requested, VkBool32 is_supported, const char* name)
    {
        if (missing == nullptr && is_requested && is_supported == VK_FALSE)
        {
            missing = name;
        }
    };
    const VkPhysicalDeviceFeatures& core = supported.features2.features;
    require(requested.fill_mode_non_solid, core.fillModeNonSolid, "fill_mode_non_solid");
    require(requested.wide_lines, core.wideLines, "wide_lines");
    require(requested.depth_clamp, core.depthClamp, "depth_clamp");
    require(requested.depth_bias_clamp, core.depthBiasClamp, "depth_bias_clamp");
    require(requested.geometry_shader, core.geometryShader, "geometry_shader");
    require(requested.tessellation_shader, core.tessellationShader, "tessellation_shader");
    require(requested.independent_blend, core.independentBlend, "independent_blend");
    require(requested.multi_draw_indirect, core.multiDrawIndirect, "multi_draw_indirect");
    require(requested.draw_indirect_first_instance, core.drawIndirectFirstInstance, "draw_indirect_first_instance");
    require(requested.sampler_anisotropy, core.samplerAnisotropy, "sampler_anisotropy");
    require(requested.texture_compression_bc, core.textureCompressionBC, "texture_compression_bc");
    require(requested.image_cube_array, core.imageCubeArray, "image_cube_array");
    require(requested.shader_int16, core.shaderInt16, "shader_int16");
    require(requested.shader_int64, core.shaderInt64, "shader_int64");
    require(requested.shader_float64, core.shaderFloat64, "shader_float64");

    require(requested.multiview, supported.vk11.multiview, "multiview");

    require(requested.descriptor_indexing, supported.vk12.descriptorIndexing, "descriptor_indexing");
    require(requested.runtime_descriptor_array, supported.vk12.runtimeDescriptorArray, "runtime_descriptor_array");
    require(requested.variable_descriptor_count, supported.vk12.descriptorBindingVariableDescriptorCount, "variable_descriptor_count");
    require(requested.partially_bound_descriptors, supported.vk12.descriptorBindingPartiallyBound, "partially_bound_descriptors");
    // One Forge flag turns on four Vulkan bits apiece, and vkCreateDevice fails on any one of them the
    // device lacks, so every bit that gets enabled is checked here rather than only the first. Naming the
    // descriptor kind keeps a device that supports most of a flag from reporting the whole flag as missing.
    require(requested.update_after_bind_descriptors, supported.vk12.descriptorBindingSampledImageUpdateAfterBind,
            "update_after_bind_descriptors (sampled images)");
    require(requested.update_after_bind_descriptors, supported.vk12.descriptorBindingStorageBufferUpdateAfterBind,
            "update_after_bind_descriptors (storage buffers)");
    require(requested.update_after_bind_descriptors, supported.vk12.descriptorBindingStorageImageUpdateAfterBind,
            "update_after_bind_descriptors (storage images)");
    require(requested.update_after_bind_descriptors, supported.vk12.descriptorBindingUniformBufferUpdateAfterBind,
            "update_after_bind_descriptors (constant buffers)");
    require(requested.update_unused_while_pending_descriptors, supported.vk12.descriptorBindingUpdateUnusedWhilePending,
            "update_unused_while_pending_descriptors");
    require(requested.non_uniform_descriptor_indexing, supported.vk12.shaderSampledImageArrayNonUniformIndexing,
            "non_uniform_descriptor_indexing (sampled images)");
    require(requested.non_uniform_descriptor_indexing, supported.vk12.shaderStorageBufferArrayNonUniformIndexing,
            "non_uniform_descriptor_indexing (storage buffers)");
    require(requested.non_uniform_descriptor_indexing, supported.vk12.shaderStorageImageArrayNonUniformIndexing,
            "non_uniform_descriptor_indexing (storage images)");
    require(requested.non_uniform_descriptor_indexing, supported.vk12.shaderUniformBufferArrayNonUniformIndexing,
            "non_uniform_descriptor_indexing (constant buffers)");
    require(requested.buffer_device_address, supported.vk12.bufferDeviceAddress, "buffer_device_address");
    require(requested.scalar_block_layout, supported.vk12.scalarBlockLayout, "scalar_block_layout");
    require(requested.host_query_reset, supported.vk12.hostQueryReset, "host_query_reset");
    require(requested.sampler_mirror_clamp_to_edge, supported.vk12.samplerMirrorClampToEdge, "sampler_mirror_clamp_to_edge");
    require(requested.sampler_filter_minmax, supported.vk12.samplerFilterMinmax, "sampler_filter_minmax");
    require(requested.draw_indirect_count, supported.vk12.drawIndirectCount, "draw_indirect_count");
    require(requested.shader_int8, supported.vk12.shaderInt8, "shader_int8");
    require(requested.shader_float16, supported.vk12.shaderFloat16, "shader_float16");
    require(requested.shader_buffer_int64_atomics, supported.vk12.shaderBufferInt64Atomics, "shader_buffer_int64_atomics");
    require(requested.shader_shared_int64_atomics, supported.vk12.shaderSharedInt64Atomics, "shader_shared_int64_atomics");
    require(requested.shader_output_layer, supported.vk12.shaderOutputLayer, "shader_output_layer");

    // Forge needs these three whatever the caller asked for, so a device without them cannot be used at all.
    require(true, supported.vk12.timelineSemaphore, "timeline semaphores, which Forge requires");
    require(true, supported.vk13.synchronization2, "synchronization2, which Forge requires");
    require(true, supported.vk13.dynamicRendering, "dynamic rendering, which Forge requires");

    require(requested.mesh_shader, has_mesh_extension ? supported.mesh.meshShader : VK_FALSE, "mesh_shader");
    require(requested.task_shader, has_mesh_extension ? supported.mesh.taskShader : VK_FALSE, "task_shader");
    require(requested.index_type_uint8, index_type_uint8_extension != nullptr ? supported.index_type_uint8.indexTypeUint8 : VK_FALSE,
            "index_type_uint8");
    require(requested.dynamic_rendering_local_read, has_local_read_extension ? supported.local_read.dynamicRenderingLocalRead : VK_FALSE,
            "dynamic_rendering_local_read");
    return missing;
}

Rndr::ErrorCode ReportUnsupportedFeatures(const Forge::PhysicalDevice& physical_device, const Forge::DeviceFeatures& requested)
{
    const char* missing = FindUnsupportedFeature(physical_device, requested);
    if (missing != nullptr)
    {
        RNDR_LOG_ERROR("Forge: this device does not support the requested feature: {}", missing);
        return Rndr::ErrorCode::FeatureNotSupported;
    }
    return Rndr::ErrorCode::Success;
}

/** "does not support the extension VK_EXT_mesh_shader", built without an allocating string concatenation. */
Opal::StringUtf8 Reason(const char* what, const char* detail)
{
    char buffer[256] = {};
    snprintf(buffer, sizeof(buffer), "%s%s", what, detail);
    return Opal::StringUtf8(buffer);
}

/**
 * Why this device cannot be created with this desc, or empty when it can. One function, so that a device
 * that passed the choice cannot then fail the creation.
 */
Opal::StringUtf8 FindUnmetRequirement(const Forge::PhysicalDevice& physical_device, const Forge::DeviceDesc& desc)
{
    // Before anything else, since the features are asked for in structures an older device does not know.
    const u32 api_version = physical_device.GetProperties().apiVersion;
    if (api_version < Forge::k_vulkan_api_version)
    {
        char buffer[64] = {};
        snprintf(buffer, sizeof(buffer), "supports only Vulkan %u.%u", VK_API_VERSION_MAJOR(api_version),
                 VK_API_VERSION_MINOR(api_version));
        return Opal::StringUtf8(buffer);
    }
    for (const char* extension_name : CollectDeviceExtensions(physical_device, desc))
    {
        if (!physical_device.IsExtensionSupported(extension_name))
        {
            return Reason("does not support the extension ", extension_name);
        }
    }
    const char* missing_feature = FindUnsupportedFeature(physical_device, desc.features);
    if (missing_feature != nullptr)
    {
        return Reason("does not support the feature ", missing_feature);
    }
    if (!physical_device.GetQueueFamilyIndex(k_graphics_flags).HasValue())
    {
        return Opal::StringUtf8("has no graphics queue");
    }
    if (desc.surface.IsValid() && !physical_device.GetPresentQueueFamilyIndex(desc.surface).HasValue())
    {
        return Opal::StringUtf8("cannot present to this surface");
    }
    if (!desc.surface.IsValid() && desc.enable_presentation && !physical_device.GetPresentQueueFamilyIndex().HasValue())
    {
        return Opal::StringUtf8("cannot present on this platform");
    }
    if (desc.use_async_compute_queue && !physical_device.GetQueueFamilyIndex(k_async_compute_flags, k_async_compute_not_flags).HasValue())
    {
        return Opal::StringUtf8("has no async compute queue");
    }
    if (desc.use_dedicated_transfer_queue &&
        !physical_device.GetQueueFamilyIndex(k_dedicated_transfer_flags, k_dedicated_transfer_not_flags).HasValue())
    {
        return Opal::StringUtf8("has no dedicated transfer queue");
    }
    if (desc.use_decode_queue && !physical_device.GetQueueFamilyIndex(VK_QUEUE_VIDEO_DECODE_BIT_KHR).HasValue())
    {
        return Opal::StringUtf8("has no video decode queue");
    }
    if (desc.use_encode_queue && !physical_device.GetQueueFamilyIndex(VK_QUEUE_VIDEO_ENCODE_BIT_KHR).HasValue())
    {
        return Opal::StringUtf8("has no video encode queue");
    }
    return Opal::StringUtf8("");
}

/** What a device is worth once it is known to qualify. Higher wins. */
Rndr::u64 ScorePhysicalDevice(const Forge::PhysicalDevice& physical_device, bool prefer_discrete)
{
    Rndr::u64 kind_score = 0;
    if (prefer_discrete)
    {
        switch (physical_device.GetProperties().deviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                kind_score = 4;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                kind_score = 3;
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                kind_score = 2;
                break;
            default:
                // A software device qualifies, but only when there is nothing else.
                break;
        }
    }
    // Device local memory breaks the tie between two devices of the same kind. Counted in gigabytes, so that
    // a small difference in memory cannot outweigh the kind of device it is.
    const VkPhysicalDeviceMemoryProperties& memory = physical_device.GetMemoryProperties();
    Rndr::u64 device_local_bytes = 0;
    for (Rndr::u32 heap = 0; heap < memory.memoryHeapCount; ++heap)
    {
        if ((memory.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0)
        {
            device_local_bytes += memory.memoryHeaps[heap].size;
        }
    }
    constexpr Rndr::u64 k_bytes_per_gigabyte = 1024ULL * 1024ULL * 1024ULL;
    return (kind_score * 1024) + (device_local_bytes / k_bytes_per_gigabyte);
}
}  // namespace

Opal::Optional<Rndr::u32> Rndr::Forge::FindPhysicalDevice(Opal::ArrayView<const PhysicalDevice> devices, const DeviceDesc& desc,
                                                          bool prefer_discrete)
{
    Opal::Optional<u32> best;
    u64 best_score = 0;
    for (i32 i = 0; i < devices.GetSize(); ++i)
    {
        const PhysicalDevice& physical_device = devices[i];
        if (!physical_device.IsValid() || !FindUnmetRequirement(physical_device, desc).IsEmpty())
        {
            continue;
        }
        const u64 score = ScorePhysicalDevice(physical_device, prefer_discrete);
        if (!best.HasValue() || score > best_score)
        {
            best = static_cast<u32>(i);
            best_score = score;
        }
    }
    return best;
}

Opal::Expected<Rndr::Forge::PhysicalDevice, Rndr::ErrorCode> Rndr::Forge::SelectPhysicalDevice(Opal::ArrayView<PhysicalDevice> devices,
                                                                                               const DeviceDesc& desc, bool prefer_discrete)
{
    using Result = Opal::Expected<PhysicalDevice, ErrorCode>;

    const Opal::ArrayView<const PhysicalDevice> const_devices(devices.GetData(), devices.GetSize());
    const Opal::Optional<u32> best = FindPhysicalDevice(const_devices, desc, prefer_discrete);
    if (best.HasValue())
    {
        return Result(std::move(devices[static_cast<i32>(best.GetValue())]));
    }
    if (devices.IsEmpty())
    {
        RNDR_LOG_ERROR("Forge: there is no Vulkan device on this machine");
        return Result(ErrorCode::NoGraphicsDevice);
    }
    // On a machine with several devices the last reason is not the whole story, but a reason beats none.
    const Opal::StringUtf8 reason = FindUnmetRequirement(devices[devices.GetSize() - 1], desc);
    RNDR_LOG_ERROR("Forge: no suitable device. The last one {}", reason.GetData());
    return Result(ErrorCode::FeatureNotSupported);
}

Opal::Expected<Rndr::Forge::Device, Rndr::ErrorCode> Rndr::Forge::Device::Create(PhysicalDevice physical_device,
                                                                                 const GraphicsContext& graphics_context,
                                                                                 const DeviceDesc& desc)
{
    using Result = Opal::Expected<Device, ErrorCode>;

    // Everything below is built into this, so a way out of here that leaves part of a device behind releases
    // it through the destructor rather than by hand.
    Device device;
    device.m_desc = desc.Clone();
    device.m_physical_device = std::move(physical_device);

    if (!device.m_physical_device.IsValid())
    {
        RNDR_LOG_ERROR("Forge: Device::Create was given an empty physical device");
        return Result(ErrorCode::InvalidArgument);
    }
    const u32 api_version = device.m_physical_device.GetProperties().apiVersion;
    if (api_version < k_vulkan_api_version)
    {
        RNDR_LOG_ERROR("Forge: this device supports only Vulkan {}.{}, and Forge needs {}.{}", VK_API_VERSION_MAJOR(api_version),
                       VK_API_VERSION_MINOR(api_version), VK_API_VERSION_MAJOR(k_vulkan_api_version),
                       VK_API_VERSION_MINOR(k_vulkan_api_version));
        return Result(ErrorCode::FeatureNotSupported);
    }

    Opal::DynamicArray<VkDeviceQueueCreateInfo> queue_create_infos;
    const ErrorCode queue_status = device.CollectQueueFamilies(queue_create_infos);
    if (queue_status != ErrorCode::Success)
    {
        return Result(queue_status);
    }

    Opal::DynamicArray<const char*> device_extensions = CollectDeviceExtensions(device.m_physical_device, device.m_desc);
    device.m_enabled_extensions = device_extensions.Clone();
    for (const char* extension_name : device_extensions)
    {
        if (!device.m_physical_device.IsExtensionSupported(extension_name))
        {
            RNDR_LOG_ERROR("Forge: device extension not supported: {}", extension_name);
            return Result(ErrorCode::FeatureNotSupported);
        }
    }

    // Checked against what the device reports before anything is asked for, so an unsupported feature names
    // itself instead of coming back as VK_ERROR_FEATURE_NOT_PRESENT from vkCreateDevice.
    const ErrorCode feature_status = ReportUnsupportedFeatures(device.m_physical_device, device.m_desc.features);
    if (feature_status != ErrorCode::Success)
    {
        return Result(feature_status);
    }

    FeatureChain enabled_features(
        [&device_extensions](const char* name)
        {
            for (const char* enabled_name : device_extensions)
            {
                if (strcmp(enabled_name, name) == 0)
                {
                    return true;
                }
            }
            return false;
        });
    enabled_features.Fill(device.m_desc.features);

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    // The features go in through the chain, which is what pEnabledFeatures staying null says.
    create_info.pNext = &enabled_features.features2;
    create_info.pQueueCreateInfos = queue_create_infos.GetData();
    create_info.queueCreateInfoCount = static_cast<u32>(queue_create_infos.GetSize());
    create_info.pEnabledFeatures = nullptr;
    create_info.ppEnabledExtensionNames = device_extensions.GetData();
    create_info.enabledExtensionCount = static_cast<u32>(device_extensions.GetSize());

    RNDR_FORGE_VK_CHECK_EXPECTED(
        vkCreateDevice(device.m_physical_device.GetNativePhysicalDevice(), &create_info, nullptr, &device.m_device), "vkCreateDevice",
        Result);

    // The device exists from here on, and so does the command pool of every queue made below. Both belong to
    // the local device, so a queue that cannot create its pool or an allocator that cannot be made leaves
    // through the destructor with everything that got that far already released.
    for (u8 queue_family_idx = 0; queue_family_idx < static_cast<u8>(QueueFamily::EnumCount); ++queue_family_idx)
    {
        const QueueFamily queue_family = static_cast<QueueFamily>(queue_family_idx);
        const u32 queue_family_index = device.m_queue_family_indices.GetQueueFamilyIndex(queue_family);
        if (queue_family_index == QueueFamilyIndices::k_invalid_index)
        {
            continue;
        }
        bool already_present = false;
        for (const auto& pair : device.m_queue_family_to_queue)
        {
            if (pair.value->GetQueueFamilyIndex() == queue_family_index)
            {
                device.m_queue_family_to_queue.Insert(queue_family, pair.value.Clone());
                already_present = true;
                break;
            }
        }
        if (already_present)
        {
            continue;
        }
        Opal::Expected<DeviceQueue, ErrorCode> queue = DeviceQueue::Create(device, queue_family_index);
        if (!queue.HasValue())
        {
            return Result(queue.GetError());
        }
        Opal::SharedPtr<DeviceQueue> queue_ptr(Opal::GetDefaultAllocator(), std::move(queue.GetValue()));
        device.m_queue_family_to_queue.Insert(queue_family, std::move(queue_ptr));
    }

    // Setup GPU allocator
    const VmaVulkanFunctions vk_functions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkCreateImage = vkCreateImage,
    };
    const VmaAllocatorCreateInfo vma_alloc_create_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = device.m_physical_device.GetNativePhysicalDevice(),
        .device = device.m_device,
        .pVulkanFunctions = &vk_functions,
        .instance = graphics_context.GetInstance(),
    };
    device.m_debug_utils_enabled = graphics_context.AreDebugUtilsEnabled();
    RNDR_FORGE_VK_CHECK_EXPECTED(vmaCreateAllocator(&vma_alloc_create_info, &device.m_gpu_allocator), "vmaCreateAllocator", Result);

    // Every queue holds a reference back to the device it was made from, and this one is about to be moved
    // into the Expected. The move constructor repoints them, so nothing here has to.
    return Result(std::move(device));
}

namespace
{
Rndr::f32 g_queue_priority = 1.0f;
}

Rndr::ErrorCode Rndr::Forge::Device::WaitForAll() const
{
    RNDR_FORGE_VK_CHECK(vkDeviceWaitIdle(m_device), "vkDeviceWaitIdle");
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::Device::CollectQueueFamilies(Opal::DynamicArray<VkDeviceQueueCreateInfo>& queue_create_infos)
{
    auto queue_family_index = m_physical_device.GetQueueFamilyIndex(k_graphics_flags);
    if (queue_family_index.HasValue())
    {
        m_queue_family_indices.graphics_family = queue_family_index.GetValue();
    }
    else
    {
        RNDR_LOG_ERROR("Forge: this device has no queue with graphics capabilities");
        return ErrorCode::FeatureNotSupported;
    }

    if (m_desc.surface.IsValid())
    {
        auto present_queue_family_index = m_physical_device.GetPresentQueueFamilyIndex(m_desc.surface);
        if (!present_queue_family_index.HasValue())
        {
            RNDR_LOG_ERROR("Forge: a surface was given but this device cannot present to it");
            return ErrorCode::FeatureNotSupported;
        }
        m_queue_family_indices.present_family = present_queue_family_index.GetValue();
    }
    else if (m_desc.enable_presentation)
    {
        // No surface to ask against, so the platform's surface-free query picks the family. Whether it can
        // present to a particular surface is verified when a swap chain is created over one.
        auto present_queue_family_index = m_physical_device.GetPresentQueueFamilyIndex();
        if (!present_queue_family_index.HasValue())
        {
            RNDR_LOG_ERROR("Forge: presentation was asked for but no queue family of this device can present");
            return ErrorCode::FeatureNotSupported;
        }
        m_queue_family_indices.present_family = present_queue_family_index.GetValue();
    }

    if (m_desc.use_async_compute_queue)
    {
        queue_family_index = m_physical_device.GetQueueFamilyIndex(k_async_compute_flags, k_async_compute_not_flags);
        if (queue_family_index.HasValue())
        {
            m_queue_family_indices.compute_family = queue_family_index.GetValue();
        }
        else
        {
            RNDR_LOG_ERROR("Forge: an async compute queue was requested but this device has none");
            return ErrorCode::FeatureNotSupported;
        }
    }

    if (m_desc.use_dedicated_transfer_queue)
    {
        queue_family_index = m_physical_device.GetQueueFamilyIndex(k_dedicated_transfer_flags, k_dedicated_transfer_not_flags);
        if (queue_family_index.HasValue())
        {
            m_queue_family_indices.transfer_family = queue_family_index.GetValue();
        }
        else
        {
            RNDR_LOG_ERROR("Forge: a dedicated transfer queue was requested but this device has none");
            return ErrorCode::FeatureNotSupported;
        }
    }

    if (m_desc.use_encode_queue)
    {
        queue_family_index = m_physical_device.GetQueueFamilyIndex(VK_QUEUE_VIDEO_ENCODE_BIT_KHR);
        if (queue_family_index.HasValue())
        {
            m_queue_family_indices.encode_family_index = queue_family_index.GetValue();
        }
        else
        {
            RNDR_LOG_ERROR("Forge: a video encode queue was requested but this device has none");
            return ErrorCode::FeatureNotSupported;
        }
    }

    if (m_desc.use_decode_queue)
    {
        queue_family_index = m_physical_device.GetQueueFamilyIndex(VK_QUEUE_VIDEO_DECODE_BIT_KHR);
        if (queue_family_index.HasValue())
        {
            m_queue_family_indices.decode_family_index = queue_family_index.GetValue();
        }
        else
        {
            RNDR_LOG_ERROR("Forge: a video decode queue was requested but this device has none");
            return ErrorCode::FeatureNotSupported;
        }
    }
    for (const u32 index : m_queue_family_indices.GetValidQueueFamilies())
    {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = index;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &g_queue_priority;
        queue_create_infos.PushBack(queue_create_info);
    }
    return ErrorCode::Success;
}

bool Rndr::Forge::Device::IsExtensionEnabled(const char* extension_name) const
{
    if (extension_name == nullptr)
    {
        return false;
    }
    for (const char* enabled : m_enabled_extensions)
    {
        if (strcmp(enabled, extension_name) == 0)
        {
            return true;
        }
    }
    return false;
}

Rndr::Forge::Device::~Device()
{
    Destroy();
}

void Rndr::Forge::Device::RepointQueues()
{
    // Every queue holds a reference back to the device it came out of, and a move leaves those pointing at the
    // object that was moved from. Its destructor would then release the command pool through a device that is
    // gone.
    for (auto queue_it = m_queue_family_to_queue.begin(); queue_it != m_queue_family_to_queue.end(); ++queue_it)
    {
        queue_it.GetValue()->m_device = *this;
    }
}

Rndr::Forge::Device::Device(Device&& other) noexcept
    : m_device(other.m_device),
      m_queue_family_to_queue(std::move(other.m_queue_family_to_queue)),
      m_physical_device(std::move(other.m_physical_device)),
      m_desc(std::move(other.m_desc)),
      m_enabled_extensions(std::move(other.m_enabled_extensions)),
      m_queue_family_indices(other.m_queue_family_indices),
      m_gpu_allocator(other.m_gpu_allocator),
      m_debug_utils_enabled(other.m_debug_utils_enabled)
{
    other.m_device = VK_NULL_HANDLE;
    other.m_queue_family_to_queue.Clear();
    other.m_physical_device = {};
    other.m_desc = {};
    other.m_enabled_extensions.Clear();
    other.m_queue_family_indices = {};
    other.m_gpu_allocator = VK_NULL_HANDLE;
    other.m_debug_utils_enabled = false;
    RepointQueues();
}

Rndr::Forge::Device& Rndr::Forge::Device::operator=(Device&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    Destroy();

    m_device = other.m_device;
    m_queue_family_to_queue = std::move(other.m_queue_family_to_queue);
    m_physical_device = std::move(other.m_physical_device);
    m_desc = std::move(other.m_desc);
    m_enabled_extensions = std::move(other.m_enabled_extensions);
    m_queue_family_indices = other.m_queue_family_indices;
    m_gpu_allocator = other.m_gpu_allocator;
    m_debug_utils_enabled = other.m_debug_utils_enabled;

    other.m_device = VK_NULL_HANDLE;
    other.m_queue_family_to_queue.Clear();
    other.m_physical_device = {};
    other.m_desc = {};
    other.m_enabled_extensions.Clear();
    other.m_queue_family_indices = {};
    other.m_gpu_allocator = VK_NULL_HANDLE;
    other.m_debug_utils_enabled = false;
    RepointQueues();

    return *this;
}

void Rndr::Forge::Device::Destroy()
{
    if (m_gpu_allocator != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(m_gpu_allocator);
        m_gpu_allocator = VK_NULL_HANDLE;
    }
    m_queue_family_to_queue.Clear();
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    m_physical_device = {};
    m_desc = {};
}

Opal::Expected<bool, Rndr::ErrorCode> Rndr::Forge::Device::CanPresentTo(const Surface& surface) const
{
    using Result = Opal::Expected<bool, ErrorCode>;

    if (!IsValid())
    {
        RNDR_LOG_ERROR("Forge: CanPresentTo was called on an empty device");
        return Result(ErrorCode::InvalidArgument);
    }
    if (!surface.IsValid())
    {
        RNDR_LOG_ERROR("Forge: CanPresentTo was given an empty surface");
        return Result(ErrorCode::InvalidArgument);
    }
    // A device created without presentation has no family to ask about, which is a no rather than a failure.
    const u32 present_family = m_queue_family_indices.present_family;
    if (present_family == QueueFamilyIndices::k_invalid_index)
    {
        return Result(false);
    }
    VkBool32 present_supported = VK_FALSE;
    RNDR_FORGE_VK_CHECK_EXPECTED(vkGetPhysicalDeviceSurfaceSupportKHR(GetNativePhysicalDevice(), present_family,
                                                                      surface.GetNativeSurface(), &present_supported),
                                 "vkGetPhysicalDeviceSurfaceSupportKHR", Result);
    return Result(present_supported == VK_TRUE);
}

Opal::Expected<Rndr::Forge::DeviceQueue&, Rndr::ErrorCode> Rndr::Forge::Device::GetQueue(QueueFamily queue_family)
{
    using Result = Opal::Expected<DeviceQueue&, ErrorCode>;

    auto queue_it = m_queue_family_to_queue.Find(queue_family);
    if (queue_it == m_queue_family_to_queue.end())
    {
        RNDR_LOG_ERROR("Forge: this device was not created with the queue family {}", static_cast<u32>(queue_family));
        return Result(ErrorCode::InvalidArgument);
    }
    return Result(*queue_it.GetValue().Get());
}

Opal::Expected<const Rndr::Forge::DeviceQueue&, Rndr::ErrorCode> Rndr::Forge::Device::GetQueue(QueueFamily queue_family) const
{
    using Result = Opal::Expected<const DeviceQueue&, ErrorCode>;

    auto queue_it = m_queue_family_to_queue.Find(queue_family);
    if (queue_it == m_queue_family_to_queue.end())
    {
        RNDR_LOG_ERROR("Forge: this device was not created with the queue family {}", static_cast<u32>(queue_family));
        return Result(ErrorCode::InvalidArgument);
    }
    return Result(*queue_it.GetValue().Get());
}

Rndr::Forge::DeviceQueue::~DeviceQueue()
{
    Destroy();
}

void Rndr::Forge::DeviceQueue::Destroy()
{
    m_queue = VK_NULL_HANDLE;
    if (m_command_pool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device->GetNativeDevice(), m_command_pool, nullptr);
        m_command_pool = VK_NULL_HANDLE;
    }
}

Opal::Expected<Rndr::Forge::DeviceQueue, Rndr::ErrorCode> Rndr::Forge::DeviceQueue::Create(const Device& device, u32 queue_family_index)
{
    using Result = Opal::Expected<DeviceQueue, ErrorCode>;

    DeviceQueue queue;
    queue.m_device = device;
    queue.m_queue_family_index = queue_family_index;
    vkGetDeviceQueue(device.GetNativeDevice(), queue_family_index, 0, &queue.m_queue);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_index;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateCommandPool(device.GetNativeDevice(), &pool_info, nullptr, &queue.m_command_pool),
                                 "vkCreateCommandPool", Result);
    return Result(std::move(queue));
}

Rndr::Forge::DeviceQueue::DeviceQueue(DeviceQueue&& other) noexcept
    : m_device(std::move(other.m_device)),
      m_queue_family_index(other.m_queue_family_index),
      m_queue(other.m_queue),
      m_command_pool(other.m_command_pool)
{
    other.m_device = nullptr;
    other.m_queue_family_index = 0;
    other.m_queue = VK_NULL_HANDLE;
    other.m_command_pool = VK_NULL_HANDLE;
}

Rndr::Forge::DeviceQueue& Rndr::Forge::DeviceQueue::operator=(DeviceQueue&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    // The device has to come along, or the command pool below is released through a device this queue no
    // longer holds, and Destroy has to run first, or the pool this queue already owns is leaked.
    Destroy();
    m_device = std::move(other.m_device);
    m_queue_family_index = other.m_queue_family_index;
    m_queue = other.m_queue;
    m_command_pool = other.m_command_pool;
    other.m_device = nullptr;
    other.m_queue_family_index = 0;
    other.m_queue = VK_NULL_HANDLE;
    other.m_command_pool = VK_NULL_HANDLE;
    return *this;
}

/** Which side of a submit a semaphore list is, which decides both the wording and one of the checks. */
enum class SemaphoreRole
{
    Wait,
    Signal
};

/** Translate one side of the semaphore list, checking that every semaphore in it actually holds a handle. */
static Opal::Expected<Opal::DynamicArray<VkSemaphoreSubmitInfo>, Rndr::ErrorCode> ToVkSemaphoreSubmitInfos(
    Opal::ArrayView<const Rndr::Forge::SemaphoreSubmit> semaphores, SemaphoreRole role)
{
    using Rndr::ErrorCode;
    using Result = Opal::Expected<Opal::DynamicArray<VkSemaphoreSubmitInfo>, ErrorCode>;

    const char* role_name = role == SemaphoreRole::Signal ? "signal" : "wait";
    Opal::DynamicArray<VkSemaphoreSubmitInfo> infos(semaphores.GetSize());
    for (Rndr::i32 i = 0; i < semaphores.GetSize(); ++i)
    {
        const Rndr::Forge::SemaphoreSubmit& semaphore = semaphores[i];
        if (!semaphore.semaphore.IsValid() || !semaphore.semaphore->IsValid())
        {
            RNDR_LOG_ERROR("Forge: Submit was given an empty {} semaphore", role_name);
            return Result(ErrorCode::InvalidArgument);
        }
        if (!semaphore.semaphore->IsTimeline() && semaphore.value != 0)
        {
            // Vulkan ignores the field on a binary semaphore rather than complaining about it, which makes
            // this exactly the kind of mistake that would otherwise be found by the frame it went wrong in.
            RNDR_LOG_ERROR("Forge: a binary {} semaphore was given a value, which only a timeline has", role_name);
            return Result(ErrorCode::InvalidArgument);
        }
        if (role == SemaphoreRole::Signal && semaphore.semaphore->IsTimeline() && semaphore.value == 0)
        {
            // A signal has to leave the count above where it was, and nothing is above zero. A wait for zero
            // is legal and trivially satisfied, so only this side is turned away.
            RNDR_LOG_ERROR("Forge: a timeline signal semaphore was given a value of zero, which no signal can reach");
            return Result(ErrorCode::InvalidArgument);
        }
        infos[i] = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = semaphore.semaphore->GetNativeSemaphore(),
                    .value = semaphore.value,
                    .stageMask = static_cast<VkPipelineStageFlags2>(semaphore.stages)};
    }
    return Result(std::move(infos));
}

Rndr::ErrorCode Rndr::Forge::DeviceQueue::Submit(const SubmitDesc& desc)
{
    Opal::DynamicArray<VkCommandBufferSubmitInfo> command_buffer_infos(desc.command_buffers.GetSize());
    for (i32 i = 0; i < desc.command_buffers.GetSize(); ++i)
    {
        const CommandBuffer& command_buffer = desc.command_buffers[i].Get();
        if (!command_buffer.IsValid())
        {
            RNDR_LOG_ERROR("Forge: Submit was given an empty command buffer");
            return ErrorCode::InvalidArgument;
        }
        command_buffer_infos[i] = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                   .commandBuffer = command_buffer.GetNativeCommandBuffer()};
    }
    Opal::Expected<Opal::DynamicArray<VkSemaphoreSubmitInfo>, ErrorCode> wait_infos_result =
        ToVkSemaphoreSubmitInfos(desc.wait_semaphores, SemaphoreRole::Wait);
    if (!wait_infos_result.HasValue())
    {
        return wait_infos_result.GetError();
    }
    Opal::Expected<Opal::DynamicArray<VkSemaphoreSubmitInfo>, ErrorCode> signal_infos_result =
        ToVkSemaphoreSubmitInfos(desc.signal_semaphores, SemaphoreRole::Signal);
    if (!signal_infos_result.HasValue())
    {
        return signal_infos_result.GetError();
    }
    const Opal::DynamicArray<VkSemaphoreSubmitInfo>& wait_infos = wait_infos_result.GetValue();
    const Opal::DynamicArray<VkSemaphoreSubmitInfo>& signal_infos = signal_infos_result.GetValue();
    // A fence is what the host waits on, and plenty of submits have nothing waiting for them.
    const VkFence native_fence = desc.fence.IsValid() ? desc.fence->GetNativeFence() : VK_NULL_HANDLE;

    const VkSubmitInfo2 submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                                    .waitSemaphoreInfoCount = static_cast<u32>(wait_infos.GetSize()),
                                    .pWaitSemaphoreInfos = wait_infos.GetData(),
                                    .commandBufferInfoCount = static_cast<u32>(command_buffer_infos.GetSize()),
                                    .pCommandBufferInfos = command_buffer_infos.GetData(),
                                    .signalSemaphoreInfoCount = static_cast<u32>(signal_infos.GetSize()),
                                    .pSignalSemaphoreInfos = signal_infos.GetData()};
    RNDR_FORGE_VK_CHECK(vkQueueSubmit2(m_queue, 1, &submit_info, native_fence), "vkQueueSubmit2");
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::DeviceQueue::Submit(const CommandBuffer& command_buffer, const Fence& fence)
{
    const Opal::Ref<const CommandBuffer> command_buffer_ref(command_buffer);
    return Submit({.command_buffers = {&command_buffer_ref, 1}, .fence = fence});
}

Rndr::ErrorCode Rndr::Forge::DeviceQueue::WaitIdle() const
{
    RNDR_FORGE_VK_CHECK(vkQueueWaitIdle(m_queue), "vkQueueWaitIdle");
    return ErrorCode::Success;
}
