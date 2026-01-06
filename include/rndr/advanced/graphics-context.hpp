#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/advanced/physical-device.hpp"

namespace Rndr
{
struct AdvancedGraphicsContextDesc
{
    bool collect_debug_messages = false;
    Opal::DynamicArray<Opal::StringUtf8> required_instance_extensions;
};

class AdvancedGraphicsContext
{
public:
    AdvancedGraphicsContext() = default;
    explicit AdvancedGraphicsContext(const AdvancedGraphicsContextDesc& desc);
    ~AdvancedGraphicsContext();
    AdvancedGraphicsContext(const AdvancedGraphicsContext&) = delete;
    AdvancedGraphicsContext& operator=(const AdvancedGraphicsContext&) = delete;
    AdvancedGraphicsContext(AdvancedGraphicsContext&&) noexcept;
    AdvancedGraphicsContext& operator=(AdvancedGraphicsContext&&) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_instance != VK_NULL_HANDLE; }
    [[nodiscard]] const AdvancedGraphicsContextDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] VkInstance GetInstance() const { return m_instance; }

    Opal::DynamicArray<AdvancedPhysicalDevice> EnumeratePhysicalDevices() const;

private:
    static Opal::DynamicArray<const char*> GetRequiredInstanceExtensions(const AdvancedGraphicsContextDesc& desc);
    static Opal::DynamicArray<VkExtensionProperties> GetSupportedInstanceExtensions();

    AdvancedGraphicsContextDesc m_desc;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
};

}  // namespace Rndr
