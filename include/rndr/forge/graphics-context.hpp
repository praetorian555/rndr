#pragma once

#include "volk/volk.h"

#include "opal/clonable-base.h"
#include "opal/enum-flags.h"
#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/shared-ptr.h"
#include "opal/container/string.h"

#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/forward.hpp"

namespace Rndr::Forge
{

/** How bad the thing a debug message reports is. Mirrors VkDebugUtilsMessageSeverityFlagBitsEXT. */
enum class DebugMessageSeverity : u8
{
    Info,
    Warning,
    Error
};

/**
 * Who is speaking. Mirrors VkDebugUtilsMessageTypeFlagBitsEXT, and the distinction matters: the loader
 * reports a missing layer manifest left behind by some other application at error severity, which says
 * nothing about this program. Validation is the one that means this code did something wrong.
 */
enum class DebugMessageTypeBits : u8
{
    None = 0,
    /** The loader or a layer talking about itself rather than about an API call. */
    General = 1,
    /** The validation layer reporting a use of Vulkan that breaks the specification. */
    Validation = 2,
    /** A layer reporting a use of Vulkan that is legal and wasteful. */
    Performance = 4,
    All = General | Validation | Performance
};
OPAL_ENUM_CLASS_FLAGS(DebugMessageTypeBits);

/** One thing the validation layer, the loader or a driver had to say. */
struct DebugMessage
{
    DebugMessageSeverity severity = DebugMessageSeverity::Info;
    DebugMessageTypeBits types = DebugMessageTypeBits::None;
    Opal::StringUtf8 text;
    /**
     * The objects the message is about, by the names SetDebugName gave them, comma separated. Empty when the
     * message names no object or none of them was named, since the message carries the handle either way and
     * a handle in a list of names is no better than a handle in the text.
     */
    Opal::StringUtf8 objects;
};

/**
 * Where a context keeps what it was told. It lives apart from the context because the messenger callback is
 * handed a pointer to it when the messenger is created and there is no way to update that pointer afterwards,
 * so it has to survive the context being moved.
 */
struct DebugMessageLog
{
    /** Warnings and errors only. Info messages are counted and logged, never stored - the loader emits many. */
    Opal::DynamicArray<DebugMessage> messages;
    /** Counts by severity and then by type, indexed the way GetDebugMessageCount reads them. */
    u32 counts[3][3] = {};
    u32 max_stored_messages = 64;
};

struct GraphicsContextDesc : Opal::ClonableBase<GraphicsContextDesc>
{
    bool collect_debug_messages = false;
    /** How many warnings and errors to keep. The counts keep rising past it; only the storage is capped. */
    u32 max_stored_debug_messages = 64;
    Opal::DynamicArray<Opal::StringUtf8> required_instance_extensions;

    OPAL_CLONE_FIELDS(collect_debug_messages, max_stored_debug_messages, required_instance_extensions);
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

    /**
     * The warnings and errors reported so far, oldest first, capped at
     * GraphicsContextDesc::max_stored_debug_messages. Empty when the context was not created with
     * collect_debug_messages and the build has no validation layer, since then nothing reports anything.
     */
    [[nodiscard]] Opal::ArrayView<const DebugMessage> GetDebugMessages() const;

    /**
     * How many messages of this severity and any of these types have been reported, including the ones
     * dropped past the storage cap. A test asserting that its work broke no rule asks for Error and
     * DebugMessageTypeBits::Validation - the default of every type would also count the loader complaining
     * about a layer manifest some other application left behind.
     * @note A message that names two types is counted under each, so a mask naming both can come out above
     *       the number of messages. Vulkan reports one type per message in practice.
     */
    [[nodiscard]] u32 GetDebugMessageCount(DebugMessageSeverity severity,
                                           DebugMessageTypeBits types = DebugMessageTypeBits::All) const;

    /** Drop the stored messages and zero the counts, so the next stretch of work is measured on its own. */
    void ClearDebugMessages();

    /**
     * Whether VK_EXT_debug_utils went to vkCreateInstance, which is what decides whether messages are
     * reported and whether an object can be given a name. Naming asks first, since the loader hands out a
     * callable pointer for the command either way and calling it without the extension crashes.
     */
    [[nodiscard]] bool AreDebugUtilsEnabled() const { return m_debug_utils_enabled; }

private:
    static Opal::DynamicArray<const char*> GetRequiredInstanceExtensions(const GraphicsContextDesc& desc, bool use_validation_layer);
    static Opal::DynamicArray<VkExtensionProperties> GetSupportedInstanceExtensions();

    GraphicsContextDesc m_desc;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    Opal::SharedPtr<DebugMessageLog> m_debug_log;
    bool m_debug_utils_enabled = false;
};

}  // namespace Rndr::Forge
