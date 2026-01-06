#include "rndr/advanced/physical-device.hpp"

#include "rndr/advanced/swap-chain.hpp"

Rndr::AdvancedPhysicalDevice::AdvancedPhysicalDevice(VkPhysicalDevice physical_device)
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

Rndr::AdvancedPhysicalDevice::~AdvancedPhysicalDevice()
{
    Destroy();
}

Rndr::AdvancedPhysicalDevice::AdvancedPhysicalDevice(AdvancedPhysicalDevice&& other) noexcept
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

Rndr::AdvancedPhysicalDevice& Rndr::AdvancedPhysicalDevice::operator=(AdvancedPhysicalDevice&& other) noexcept
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

Opal::Expected<Rndr::u32, VkResult> Rndr::AdvancedPhysicalDevice::GetQueueFamilyIndex(VkQueueFlags queue_flags) const
{
    for (u32 i = 0; i < m_queue_family_properties.GetSize(); i++)
    {
        const VkQueueFamilyProperties& props = m_queue_family_properties[i];
        if ((props.queueFlags & queue_flags) == queue_flags)
        {
            return Opal::Expected<u32, VkResult>(i);
        }
    }

    return Opal::Expected<u32, VkResult>(VK_ERROR_FEATURE_NOT_PRESENT);
}

Opal::Expected<Rndr::u32, VkResult> Rndr::AdvancedPhysicalDevice::GetPresentQueueFamilyIndex(const class AdvancedSurface& surface) const
{
    for (u32 i = 0; i < m_queue_family_properties.GetSize(); i++)
    {
        VkBool32 present_support = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, surface.GetNativeSurface(), &present_support);
        if (present_support == VK_TRUE)
        {
            return Opal::Expected<u32, VkResult>(i);
        }
    }
    return Opal::Expected<u32, VkResult>(VK_ERROR_FEATURE_NOT_PRESENT);
}

bool Rndr::AdvancedPhysicalDevice::IsExtensionSupported(const char* extension_name) const
{
    bool is_found = false;
    for (const Opal::StringUtf8& supported_extension : m_supported_extensions)
    {
        if (supported_extension == extension_name)
        {
            is_found = true;
            break;
        }
    }
    if (!is_found)
    {
        Opal::Exception("Device extension not supported!");
    }
    return true;
}

Rndr::u32 Rndr::AdvancedPhysicalDevice::FindMemoryTypeIndex(u32 type_filter, VkMemoryPropertyFlags properties) const
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

void Rndr::AdvancedPhysicalDevice::Destroy()
{
    m_physical_device = VK_NULL_HANDLE;
    m_queue_family_properties.Clear();
    m_supported_extensions.Clear();
    m_queue_family_properties = {};
}