#include "rndr/forge/physical-device.hpp"

#include "rndr/forge/swap-chain.hpp"

Rndr::Forge::PhysicalDevice::PhysicalDevice(VkPhysicalDevice physical_device)
{
    if (physical_device == VK_NULL_HANDLE)
    {
        throw Opal::Exception("Physical device handle is invalid!");
    }

    vkGetPhysicalDeviceProperties(physical_device, &m_properties);
    vkGetPhysicalDeviceFeatures(physical_device, &m_features);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &m_memory_properties);

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    if (queue_family_count == 0)
    {
        throw Opal::Exception("No queue families found!");
    }

    m_queue_family_properties.Resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, m_queue_family_properties.GetData());

    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
    if (extension_count > 0)
    {
        Opal::DynamicArray<VkExtensionProperties> extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, extensions.GetData());
        for (const VkExtensionProperties& extension : extensions)
        {
            m_supported_extensions.PushBack(extension.extensionName);
        }
    }

    m_physical_device = physical_device;
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

Rndr::u32 Rndr::Forge::PhysicalDevice::FindMemoryTypeIndex(u32 type_filter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memory_properties);

    for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i)
    {
        // Properties here specify if the memory is device local, host visible, etc.
        // Device has an array of memory types, and each bit in the filter corresponds to one memory type in that array.
        if ((type_filter & (1 << i)) != 0 && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    // Just use first memory type available
    return 0;
}

void Rndr::Forge::PhysicalDevice::Destroy()
{
    m_physical_device = VK_NULL_HANDLE;
    m_queue_family_properties.Clear();
    m_supported_extensions.Clear();
    m_queue_family_properties = {};
}