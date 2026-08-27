#include "rndr/forge/physical-device.hpp"

#include "rndr/forge/swap-chain.hpp"
#include "rndr/log.hpp"

Opal::Expected<Rndr::Forge::PhysicalDevice, Rndr::ErrorCode> Rndr::Forge::PhysicalDevice::Create(VkPhysicalDevice physical_device)
{
    using Result = Opal::Expected<PhysicalDevice, ErrorCode>;

    if (physical_device == VK_NULL_HANDLE)
    {
        RNDR_LOG_ERROR("Forge: PhysicalDevice::Create was given a null handle");
        return Result(ErrorCode::InvalidArgument);
    }

    PhysicalDevice device;
    vkGetPhysicalDeviceProperties(physical_device, &device.m_properties);
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
      m_features(other.m_features),
      m_memory_properties(other.m_memory_properties),
      m_queue_family_properties(Opal::Move(other.m_queue_family_properties)),
      m_supported_extensions(Opal::Move(other.m_supported_extensions))
{
    other.m_physical_device = VK_NULL_HANDLE;
    other.m_properties = {};
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
    m_features = other.m_features;
    m_memory_properties = other.m_memory_properties;
    m_queue_family_properties = Opal::Move(other.m_queue_family_properties);
    m_supported_extensions = Opal::Move(other.m_supported_extensions);

    other.m_physical_device = VK_NULL_HANDLE;
    other.m_properties = {};
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
        const VkQueueFamilyProperties& props = m_queue_family_properties[i];
        if ((props.queueFlags & queue_flags) == queue_flags && (props.queueFlags & not_queue_flags) == 0)
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
#if defined(OPAL_PLATFORM_WINDOWS)
    for (u32 i = 0; i < m_queue_family_properties.GetSize(); i++)
    {
        if (vkGetPhysicalDeviceWin32PresentationSupportKHR(m_physical_device, i) == VK_TRUE)
        {
            return Opal::Optional<u32>(i);
        }
    }
    return {};
#else
    // The XCB query needs a connection and a visual, both of which only a window supplies. The
    // graphics family presents on every desktop stack; a surface it cannot present to is caught
    // when a swap chain is created over one.
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
    m_queue_family_properties.Clear();
    m_supported_extensions.Clear();
    m_queue_family_properties = {};
}