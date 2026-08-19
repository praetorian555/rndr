#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/optional.h"
#include "opal/container/string.h"

#include "rndr/pixel-format.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"

namespace Rndr::Forge
{

class PhysicalDevice
{
public:
    PhysicalDevice() = default;
    explicit PhysicalDevice(VkPhysicalDevice physical_device);
    ~PhysicalDevice();

    PhysicalDevice(const PhysicalDevice&) = delete;
    PhysicalDevice& operator=(const PhysicalDevice&) = delete;
    PhysicalDevice(PhysicalDevice&&) noexcept;
    PhysicalDevice& operator=(PhysicalDevice&&) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_physical_device != VK_NULL_HANDLE; }

    [[nodiscard]] VkPhysicalDevice GetNativePhysicalDevice() const { return m_physical_device; }
    [[nodiscard]] const VkPhysicalDeviceProperties& GetProperties() const { return m_properties; }
    [[nodiscard]] const VkPhysicalDeviceFeatures& GetFeatures() const { return m_features; }
    [[nodiscard]] const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_memory_properties; }
    [[nodiscard]] const Opal::DynamicArray<VkQueueFamilyProperties>& GetQueueFamilyProperties() const { return m_queue_family_properties; }
    [[nodiscard]] const Opal::DynamicArray<Opal::StringUtf8>& GetSupportedExtensions() const { return m_supported_extensions; }
    /**
     * Find a queue family that has all of queue_flags and none of not_queue_flags. Empty when this device has no such
     * family, which is an answer rather than a failure - see the error handling section of docs/forge.md.
     */
    [[nodiscard]] Opal::Optional<u32> GetQueueFamilyIndex(VkQueueFlags queue_flags, VkQueueFlags not_queue_flags = 0) const;

    /** Find a queue family that can present to the given surface. Empty when this device cannot present to it. */
    [[nodiscard]] Opal::Optional<u32> GetPresentQueueFamilyIndex(const Surface& surface) const;

    [[nodiscard]] bool IsExtensionSupported(const char* extension_name) const;

    /**
     * Whether an optimally tiled image of this format can take part in a blit. Support is per format and per
     * side, so a format that can be blitted from cannot always be blitted into.
     * @param format Format of the image.
     * @param as_source True to ask about the source of the blit, false about the destination.
     */
    [[nodiscard]] bool SupportsBlit(PixelFormat format, bool as_source) const;

    /**
     * Whether an optimally tiled image of this format can be filtered linearly, which a blit with a linear
     * filter needs of its source.
     */
    [[nodiscard]] bool SupportsLinearFilter(PixelFormat format) const;

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