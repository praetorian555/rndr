#include "rndr/forge/physical-device.hpp"

#include "rndr/forge/swap-chain.hpp"
#include "rndr/log.hpp"

Opal::Expected<Rndr::Forge::PhysicalDevice, Rndr::ErrorCode> Rndr::Forge::PhysicalDevice::Create(VkPhysicalDevice physical_device,
                                                                                                       u32 instance_api_version)
{
    using Result = Opal::Expected<PhysicalDevice, ErrorCode>;

    if (physical_device == VK_NULL_HANDLE)
    {
        RNDR_LOG_ERROR("Forge: PhysicalDevice::Create was given a null handle");
        return Result(ErrorCode::InvalidArgument);
    }

    PhysicalDevice device;
    vkGetPhysicalDeviceProperties(physical_device, &device.m_properties);
    const u32 device_version = VK_MAKE_API_VERSION(0, VK_API_VERSION_MAJOR(device.m_properties.apiVersion),
                                                   VK_API_VERSION_MINOR(device.m_properties.apiVersion), 0);
    const u32 instance_version =
        VK_MAKE_API_VERSION(0, VK_API_VERSION_MAJOR(instance_api_version), VK_API_VERSION_MINOR(instance_api_version), 0);
    device.m_api_version = device_version < instance_version ? device_version : instance_version;
    vkGetPhysicalDeviceFeatures(physical_device, &device.m_features);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &device.m_memory_properties);

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    if (queue_family_count == 0)
    {
        RNDR_LOG_ERROR("Forge: physical device {} reports no queue family", static_cast<const char*>(device.m_properties.deviceName));
        return Result(ErrorCode::GraphicsAPIError);
    }

    device.m_queue_family_properties.Resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, device.m_queue_family_properties.GetData());

    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
    if (extension_count > 0)
    {
        Opal::DynamicArray<VkExtensionProperties> extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, extensions.GetData());
        for (const VkExtensionProperties& extension : extensions)
        {
            device.m_supported_extensions.PushBack(extension.extensionName);
        }
    }

    // Which codecs each family decodes is only asked of a device that has video at all: the structure belongs to the
    // extension, and chaining it without the extension is not allowed.
    if (device.IsExtensionSupported(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME))
    {
        Opal::DynamicArray<VkQueueFamilyVideoPropertiesKHR> video_properties(queue_family_count);
        Opal::DynamicArray<VkQueueFamilyQueryResultStatusPropertiesKHR> status_properties(queue_family_count);
        Opal::DynamicArray<VkQueueFamilyProperties2> properties2(queue_family_count);
        for (u32 i = 0; i < queue_family_count; ++i)
        {
            status_properties[i] = {.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_QUERY_RESULT_STATUS_PROPERTIES_KHR};
            video_properties[i] = {.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR, .pNext = &status_properties[i]};
            properties2[i] = {.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2, .pNext = &video_properties[i]};
        }
        vkGetPhysicalDeviceQueueFamilyProperties2(physical_device, &queue_family_count, properties2.GetData());
        device.m_video_codec_operations.Resize(queue_family_count, 0u);
        device.m_query_result_status_support.Resize(queue_family_count, false);
        for (u32 i = 0; i < queue_family_count; ++i)
        {
            device.m_video_codec_operations[i] = video_properties[i].videoCodecOperations;
            device.m_query_result_status_support[i] = status_properties[i].queryResultStatusSupport == VK_TRUE;
        }
    }

    // Core since Vulkan 1.2 and an extension before, and chaining the structure is only allowed on a device that has
    // one or the other. One with neither cannot run Forge, which needs dynamic rendering and so this extension too,
    // and is turned down when a device is chosen; its resolve modes stay zero.
    if (device.m_api_version >= VK_API_VERSION_1_2 || device.IsExtensionSupported(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME))
    {
        VkPhysicalDeviceProperties2 properties2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                                .pNext = &device.m_depth_stencil_resolve_properties};
        vkGetPhysicalDeviceProperties2(physical_device, &properties2);
        device.m_depth_stencil_resolve_properties.pNext = nullptr;
    }

    device.m_physical_device = physical_device;
    return Result(std::move(device));
}

Rndr::Forge::PhysicalDevice::~PhysicalDevice()
{
    Destroy();
}

Rndr::Forge::PhysicalDevice::PhysicalDevice(PhysicalDevice&& other) noexcept
    : m_physical_device(other.m_physical_device),
      m_properties(other.m_properties),
      m_api_version(other.m_api_version),
      m_features(other.m_features),
      m_memory_properties(other.m_memory_properties),
      m_depth_stencil_resolve_properties(other.m_depth_stencil_resolve_properties),
      m_queue_family_properties(Opal::Move(other.m_queue_family_properties)),
      m_video_codec_operations(Opal::Move(other.m_video_codec_operations)),
      m_query_result_status_support(Opal::Move(other.m_query_result_status_support)),
      m_supported_extensions(Opal::Move(other.m_supported_extensions))
{
    other.m_physical_device = VK_NULL_HANDLE;
    other.m_properties = {};
    other.m_api_version = 0;
    other.m_features = {};
    other.m_memory_properties = {};
    other.m_queue_family_properties.Clear();
    other.m_supported_extensions.Clear();
}

Rndr::Forge::PhysicalDevice& Rndr::Forge::PhysicalDevice::operator=(PhysicalDevice&& other) noexcept
{
    // Without the guard, assigning this to itself releases what it holds and then moves from the wreck,
    // which leaves a live object empty. Every other type here guards it the same way.
    if (this == &other)
    {
        return *this;
    }

    Destroy();

    m_physical_device = other.m_physical_device;
    m_properties = other.m_properties;
    m_api_version = other.m_api_version;
    m_features = other.m_features;
    m_memory_properties = other.m_memory_properties;
    m_depth_stencil_resolve_properties = other.m_depth_stencil_resolve_properties;
    m_queue_family_properties = Opal::Move(other.m_queue_family_properties);
    m_video_codec_operations = Opal::Move(other.m_video_codec_operations);
    m_query_result_status_support = Opal::Move(other.m_query_result_status_support);
    m_supported_extensions = Opal::Move(other.m_supported_extensions);

    other.m_physical_device = VK_NULL_HANDLE;
    other.m_properties = {};
    other.m_api_version = 0;
    other.m_features = {};
    other.m_memory_properties = {};
    other.m_queue_family_properties.Clear();
    other.m_supported_extensions.Clear();

    return *this;
}

Opal::Optional<Rndr::u32> Rndr::Forge::PhysicalDevice::GetQueueFamilyIndex(VkQueueFlags queue_flags, VkQueueFlags not_queue_flags) const
{
    for (u32 i = 0; i < m_queue_family_properties.GetSize(); i++)
    {
        const VkQueueFlags flags = AddImpliedQueueFlags(m_queue_family_properties[i].queueFlags);
        if ((flags & queue_flags) == queue_flags && (flags & not_queue_flags) == 0)
        {
            return Opal::Optional<u32>(i);
        }
    }

    return {};
}

Opal::Optional<Rndr::u32> Rndr::Forge::PhysicalDevice::GetPresentQueueFamilyIndex(const Surface& surface) const
{
    for (u32 i = 0; i < m_queue_family_properties.GetSize(); i++)
    {
        VkBool32 present_support = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, surface.GetNativeSurface(), &present_support);
        if (present_support == VK_TRUE)
        {
            return Opal::Optional<u32>(i);
        }
    }
    return {};
}

Opal::Optional<Rndr::u32> Rndr::Forge::PhysicalDevice::GetPresentQueueFamilyIndex() const
{
#if RNDR_WINDOWS
    for (u32 i = 0; i < m_queue_family_properties.GetSize(); i++)
    {
        if (vkGetPhysicalDeviceWin32PresentationSupportKHR(m_physical_device, i) == VK_TRUE)
        {
            return Opal::Optional<u32>(i);
        }
    }
    return {};
#else
    // The XCB query needs a connection and a visual, both of which only a window supplies, and Android
    // has no surface-free query at all. The graphics family presents on every stack either runs on; a
    // surface it cannot present to is caught when a swap chain is created over one.
    for (u32 i = 0; i < m_queue_family_properties.GetSize(); i++)
    {
        if ((m_queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            return Opal::Optional<u32>(i);
        }
    }
    return {};
#endif
}

VkVideoCodecOperationFlagsKHR Rndr::Forge::PhysicalDevice::GetVideoCodecOperations(u32 queue_family) const
{
    return queue_family < m_video_codec_operations.GetSize() ? m_video_codec_operations[queue_family] : 0;
}

bool Rndr::Forge::PhysicalDevice::SupportsQueryResultStatus(u32 queue_family) const
{
    return queue_family < m_query_result_status_support.GetSize() && m_query_result_status_support[queue_family];
}

Opal::Optional<Rndr::u32> Rndr::Forge::PhysicalDevice::GetDecodeQueueFamilyIndex(VkVideoCodecOperationFlagsKHR codecs) const
{
    for (u32 i = 0; i < m_queue_family_properties.GetSize(); i++)
    {
        if ((m_queue_family_properties[i].queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) != 0 && (GetVideoCodecOperations(i) & codecs) == codecs)
        {
            return Opal::Optional<u32>(i);
        }
    }
    return {};
}

bool Rndr::Forge::PhysicalDevice::IsExtensionSupported(const char* extension_name) const
{
    for (const Opal::StringUtf8& supported_extension : m_supported_extensions)
    {
        if (supported_extension == extension_name)
        {
            return true;
        }
    }
    return false;
}

/** What an optimally tiled image of this format is allowed to do on this device. */
static VkFormatFeatureFlags GetOptimalTilingFeatures(VkPhysicalDevice physical_device, Rndr::PixelFormat format)
{
    VkFormatProperties format_properties = {};
    vkGetPhysicalDeviceFormatProperties(physical_device, Rndr::ToVkFormat(format), &format_properties);
    return format_properties.optimalTilingFeatures;
}

bool Rndr::Forge::PhysicalDevice::SupportsBlit(PixelFormat format, bool as_source) const
{
    const VkFormatFeatureFlags features = GetOptimalTilingFeatures(m_physical_device, format);
    const VkFormatFeatureFlags required = as_source ? VK_FORMAT_FEATURE_BLIT_SRC_BIT : VK_FORMAT_FEATURE_BLIT_DST_BIT;
    return (features & required) != 0;
}

Rndr::Forge::YcbcrFormatSupport Rndr::Forge::ReadYcbcrFormatFeatures(VkFormatFeatureFlags features)
{
    YcbcrFormatSupport support;
    support.sampled = (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
    if (!support.sampled)
    {
        return support;
    }
    support.transfer_destination = (features & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) != 0;
    support.cosited_chroma = (features & VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT) != 0;
    support.midpoint_chroma = (features & VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT) != 0;
    support.linear_chroma_filter = (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT) != 0;
    support.linear_filter = (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    support.separate_reconstruction_filter = (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT) != 0;
    support.explicit_reconstruction_forceable =
        (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT) != 0;
    return support;
}

Rndr::Forge::YcbcrFormatSupport Rndr::Forge::PhysicalDevice::GetYcbcrFormatSupport(PixelFormat format) const
{
    YcbcrFormatSupport support;
    if (!IsYcbcrFormat(format))
    {
        return support;
    }
    support = ReadYcbcrFormatFeatures(GetOptimalTilingFeatures(m_physical_device, format));
    if (!support.sampled)
    {
        return support;
    }

    const VkPhysicalDeviceImageFormatInfo2 format_info{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
                                                       .format = ToVkFormat(format),
                                                       .type = VK_IMAGE_TYPE_2D,
                                                       .tiling = VK_IMAGE_TILING_OPTIMAL,
                                                       .usage = VK_IMAGE_USAGE_SAMPLED_BIT};
    VkSamplerYcbcrConversionImageFormatProperties ycbcr_properties{.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES};
    VkImageFormatProperties2 properties{.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2, .pNext = &ycbcr_properties};
    if (vkGetPhysicalDeviceImageFormatProperties2(m_physical_device, &format_info, &properties) == VK_SUCCESS)
    {
        support.combined_image_sampler_descriptor_count = ycbcr_properties.combinedImageSamplerDescriptorCount;
    }
    return support;
}

bool Rndr::Forge::PhysicalDevice::SupportsLinearFilter(PixelFormat format) const
{
    const VkFormatFeatureFlags features = GetOptimalTilingFeatures(m_physical_device, format);
    return (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
}

Opal::Expected<Rndr::u32, Rndr::ErrorCode> Rndr::Forge::PhysicalDevice::FindMemoryTypeIndex(u32 type_filter,
                                                                                            VkMemoryPropertyFlags properties) const
{
    using Result = Opal::Expected<u32, ErrorCode>;

    // The properties read once at construction rather than asked for again. They cannot change for the life
    // of the physical device, and the accessor beside this one already hands out the cached copy - so asking
    // twice was only a way for the two to disagree.
    for (u32 i = 0; i < m_memory_properties.memoryTypeCount; ++i)
    {
        // Each bit of the filter corresponds to one entry of the device's memory type array, and the
        // properties say whether that memory is device local, host visible and so on.
        if ((type_filter & (1U << i)) != 0 && (m_memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return Result(i);
        }
    }
    // Nothing matched. This used to hand back index zero, which is a real memory type with real properties
    // and never the one that was asked for - a caller could not tell it from a match, so an allocation went
    // to the wrong heap and the mistake surfaced somewhere else entirely. See the error handling section of
    // docs/forge.md: a default the caller cannot distinguish from an answer is the one thing Forge does not do.
    RNDR_LOG_ERROR("Forge: no memory type on this device has the requested properties");
    return Result(ErrorCode::FeatureNotSupported);
}

void Rndr::Forge::PhysicalDevice::Destroy()
{
    m_physical_device = VK_NULL_HANDLE;
    m_api_version = 0;
    m_queue_family_properties.Clear();
    m_video_codec_operations.Clear();
    m_query_result_status_support.Clear();
    m_supported_extensions.Clear();
    m_queue_family_properties = {};
}