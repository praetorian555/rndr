#pragma once

#include "volk/volk.h"

#include "opal/clonable-base.h"
#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/container/shared-ptr.h"
#include "opal/container/string.h"
#include "opal/enum-flags.h"

#include "rndr/error-codes.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/physical-device.hpp"

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
    /**
     * Which types reach the log, copied from GraphicsContextDesc. It lives here because the callback is
     * handed this and nothing else, and it applies to logging alone - a message of any type is still
     * counted and still stored.
     */
    DebugMessageTypeBits logged_types = DebugMessageTypeBits::All;
};

struct GraphicsContextDesc : Opal::ClonableBase<GraphicsContextDesc>
{
    bool collect_debug_messages = false;
    /** How many warnings and errors to keep. The counts keep rising past it; only the storage is capped. */
    u32 max_stored_debug_messages = 64;
    /**
     * Which message types are written to the log. Everything by default, which is what an application
     * wants: a loader that cannot load a driver says so as a General message and there is nowhere else to
     * hear it. Narrowing this to Validation is for a program that reads its own log looking for its own
     * mistakes, where the loader naming every manifest another application left on the machine - at error
     * severity, once per instance - buries them.
     *
     * Only the log is filtered. GetDebugMessages and GetDebugMessageCount answer for every type either way,
     * so nothing that asserts on the messages depends on this.
     */
    DebugMessageTypeBits logged_message_types = DebugMessageTypeBits::All;
    Opal::DynamicArray<Opal::StringUtf8> required_instance_extensions;

    OPAL_CLONE_FIELDS(collect_debug_messages, max_stored_debug_messages, logged_message_types, required_instance_extensions);
};

class GraphicsContext
{
public:
    GraphicsContext() = default;
    ~GraphicsContext();

    /**
     * Load Vulkan and create the instance, with the validation layer and the debug messenger when the build
     * and the desc ask for them.
     *
     * @param desc What to collect and which instance extensions to require.
     * @return The context, ErrorCode::FeatureNotSupported when a required instance extension is not there, or
     *         whatever the failing Vulkan call maps to - see VkResultToErrorCode.
     */
    [[nodiscard]] static Opal::Expected<GraphicsContext, ErrorCode> Create(const GraphicsContextDesc& desc = {});

    GraphicsContext(const GraphicsContext&) = delete;
    GraphicsContext& operator=(const GraphicsContext&) = delete;
    GraphicsContext(GraphicsContext&&) noexcept;
    GraphicsContext& operator=(GraphicsContext&&) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_instance != VK_NULL_HANDLE; }
    [[nodiscard]] const GraphicsContextDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] VkInstance GetInstance() const { return m_instance; }

    /**
     * Every Vulkan capable device on this machine, in the order the loader reports them.
     *
     * Never an empty list: a machine with no such device reports ErrorCode::NoGraphicsDevice rather than
     * handing back a list with nothing in it. There is nothing a caller can do with an empty one - the next
     * step is always to index it or to hand it to SelectPhysicalDevice - so the absence is reported where it
     * happens instead of becoming a check every caller has to remember.
     *
     * @return The devices, ErrorCode::NoGraphicsDevice when this machine has none, or whatever the failing
     *         enumeration maps to, which is a different thing entirely.
     */
    [[nodiscard]] Opal::Expected<Opal::DynamicArray<PhysicalDevice>, ErrorCode> EnumeratePhysicalDevices() const;

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
     * @return The count, or ErrorCode::InvalidArgument when severity is not one of the three.
     */
    [[nodiscard]] Opal::Expected<u32, ErrorCode> GetDebugMessageCount(DebugMessageSeverity severity,
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
