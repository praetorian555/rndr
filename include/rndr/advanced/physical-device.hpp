#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/types.hpp"

namespace Rndr
{

class AdvancedPhysicalDevice
{
public:
    AdvancedPhysicalDevice() = default;
    explicit AdvancedPhysicalDevice(VkPhysicalDevice physical_device);
    ~AdvancedPhysicalDevice();

    AdvancedPhysicalDevice(const AdvancedPhysicalDevice&) = delete;
    AdvancedPhysicalDevice& operator=(const AdvancedPhysicalDevice&) = delete;
    AdvancedPhysicalDevice(AdvancedPhysicalDevice&&) noexcept;
    AdvancedPhysicalDevice& operator=(AdvancedPhysicalDevice&&) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_physical_device != VK_NULL_HANDLE; }

    [[nodiscard]] VkPhysicalDevice GetNativePhysicalDevice() const { return m_physical_device; }
    [[nodiscard]] const VkPhysicalDeviceProperties& GetProperties() const { return m_properties; }
    [[nodiscard]] const VkPhysicalDeviceFeatures& GetFeatures() const { return m_features; }
    [[nodiscard]] const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_memory_properties; }
    [[nodiscard]] const Opal::DynamicArray<VkQueueFamilyProperties>& GetQueueFamilyProperties() const { return m_queue_family_properties; }
    [[nodiscard]] const Opal::DynamicArray<Opal::StringUtf8>& GetSupportedExtensions() const { return m_supported_extensions; }
    [[nodiscard]] Opal::Expected<u32, VkResult> GetQueueFamilyIndex(VkQueueFlags queue_flags) const;
    [[nodiscard]] Opal::Expected<u32, VkResult> GetPresentQueueFamilyIndex(const class AdvancedSurface& surface) const;

    [[nodiscard]] bool IsExtensionSupported(const char* extension_name) const;

    [[nodiscard]] u32 FindMemoryTypeIndex(u32 type_filter, VkMemoryPropertyFlags properties) const;

private:
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties m_properties = {};
    VkPhysicalDeviceFeatures m_features = {};
    VkPhysicalDeviceMemoryProperties m_memory_properties = {};
    Opal::DynamicArray<VkQueueFamilyProperties> m_queue_family_properties;
    Opal::DynamicArray<Opal::StringUtf8> m_supported_extensions;
};

}