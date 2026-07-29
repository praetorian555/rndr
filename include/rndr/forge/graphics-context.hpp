#pragma once

#include "volk/volk.h"

#include "opal/clonable-base.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/forward.hpp"

namespace Rndr::Forge
{
struct GraphicsContextDesc : Opal::ClonableBase<GraphicsContextDesc>
{
    bool collect_debug_messages = false;
    Opal::DynamicArray<Opal::StringUtf8> required_instance_extensions;

    OPAL_CLONE_FIELDS(collect_debug_messages, required_instance_extensions);
};

class GraphicsContext
{
public:
    GraphicsContext() = default;
    explicit GraphicsContext(const GraphicsContextDesc& desc);
    ~GraphicsContext();
    GraphicsContext(const GraphicsContext&) = delete;
    GraphicsContext& operator=(const GraphicsContext&) = delete;
    GraphicsContext(GraphicsContext&&) noexcept;
    GraphicsContext& operator=(GraphicsContext&&) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_instance != VK_NULL_HANDLE; }
    [[nodiscard]] const GraphicsContextDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] VkInstance GetInstance() const { return m_instance; }

    Opal::DynamicArray<PhysicalDevice> EnumeratePhysicalDevices() const;

private:
    static Opal::DynamicArray<const char*> GetRequiredInstanceExtensions(const GraphicsContextDesc& desc);
    static Opal::DynamicArray<VkExtensionProperties> GetSupportedInstanceExtensions();

    GraphicsContextDesc m_desc;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
};

}  // namespace Rndr::Forge
