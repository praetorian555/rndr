#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/container/optional.h"
#include "opal/container/string.h"

#include "rndr/error-codes.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/pixel-format.hpp"
#include "rndr/types.hpp"

namespace Rndr::Forge
{

class PhysicalDevice
{
public:
    PhysicalDevice() = default;
    ~PhysicalDevice();

    /**
     * Read everything this device reports about itself: its properties, its features, its memory, its queue
     * families and its extensions.
     *
     * @param physical_device Handle the loader reported.
     * @return The device, ErrorCode::InvalidArgument for a null handle, or ErrorCode::GraphicsAPIError when
     *         the device reports no queue family at all, which no usable device does.
     */
    [[nodiscard]] static Opal::Expected<PhysicalDevice, ErrorCode> Create(VkPhysicalDevice physical_device);

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

    /**
     * The first memory type this device offers that the filter allows and that has all of `properties`.
     *
     * Forge allocates through VMA and never calls this; it is here for a caller reaching past Forge to
     * Vulkan, where the filter is `VkMemoryRequirements::memoryTypeBits` and the properties are whatever
     * the allocation needs to be able to do.
     *
     * @param type_filter Bit per memory type of this device, set for the ones the allocation may use.
     * @param properties Every property the type has to have. Zero accepts the first type the filter allows.
     * @return Index into the device's memory type array, or ErrorCode::FeatureNotSupported when no type
     *         satisfies both. Not a returned sentinel: index zero is a real memory type, so a caller could
     *         not tell one from a match and would allocate from the wrong heap.
     */
    [[nodiscard]] Opal::Expected<u32, ErrorCode> FindMemoryTypeIndex(u32 type_filter, VkMemoryPropertyFlags properties) const;

private:
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties m_properties = {};
    VkPhysicalDeviceFeatures m_features = {};
    VkPhysicalDeviceMemoryProperties m_memory_properties = {};
    Opal::DynamicArray<VkQueueFamilyProperties> m_queue_family_properties;
    Opal::DynamicArray<Opal::StringUtf8> m_supported_extensions;
};

}  // namespace Rndr::Forge