#include "rndr/forge/graphics-context.hpp"

#include "opal/defines.h"

#if defined(OPAL_PLATFORM_WINDOWS)
#include "rndr/platform/windows-header.hpp"
#endif

#include <mutex>

#include "opal/container/dynamic-array.h"

#include "rndr/forge/vulkan-exception.hpp"
#include "rndr/log.hpp"

namespace
{
/**
 * volk keeps one set of function pointers for the whole process: volkInitialize loads them and volkFinalize
 * nulls them and unloads the library. Neither counts its callers, so a second context calling volkFinalize
 * on the way out used to null the pointers a live context was still calling through - a jump to address zero
 * on its next Vulkan call. Count the contexts here instead, so that only the last one out tears volk down.
 *
 * What this does not fix is volkLoadInstance: it points the global instance and device tables at whichever
 * instance was created last, so two live contexts share one table and the older one dispatches through the
 * newer one's instance. Ending that means per-object tables - volkLoadDeviceTable and friends - rather than
 * a counter, so a second live context is warned about below instead.
 */
std::mutex g_volk_mutex;
Rndr::i32 g_volk_users = 0;

/** Load volk if this is the first context, and count this one in. Throws without counting if the load fails. */
void AcquireVolk()
{
    const std::lock_guard<std::mutex> lock(g_volk_mutex);
    if (g_volk_users == 0)
    {
        const VkResult result = volkInitialize();
        if (result != VK_SUCCESS)
        {
            throw Rndr::Forge::VulkanException(result, "volkInitialize");
        }
    }
    else
    {
        RNDR_LOG_WARNING(
            "A second Forge::GraphicsContext is live. volk dispatches through one set of function pointers for the "
            "whole process, and creating this instance repointed them, so the older context now calls through this "
            "one's instance.");
    }
    ++g_volk_users;
}

/** Count this context out, and unload volk once none are left. */
void ReleaseVolk()
{
    const std::lock_guard<std::mutex> lock(g_volk_mutex);
    if (g_volk_users > 0 && --g_volk_users == 0)
    {
        volkFinalize();
    }
}

/**
 * Holds the count for the length of the constructor. A context that throws part way through never reaches
 * its destructor, so without this the count it took would never be given back.
 */
struct VolkUse
{
    bool committed = false;

    VolkUse() { AcquireVolk(); }
    ~VolkUse()
    {
        if (!committed)
        {
            ReleaseVolk();
        }
    }

    VolkUse(const VolkUse&) = delete;
    VolkUse& operator=(const VolkUse&) = delete;
};

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* create_info,
                                      const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* debug_messenger)
{
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (func != nullptr)
    {
        return func(instance, create_info, allocator, debug_messenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types,
                                             const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    using namespace Rndr::Forge;
    auto* log = static_cast<DebugMessageLog*>(user_data);
    // The values of DebugMessageTypeBits mirror VkDebugUtilsMessageTypeFlagBitsEXT, so the mask translates as
    // a cast, except for the bits of later extensions this does not name.
    const auto forge_types = static_cast<DebugMessageTypeBits>(types) & DebugMessageTypeBits::All;

    // Which types are worth writing down is the caller's to say, and a context that collects nothing has
    // nowhere to have said it, so it gets the default: everything.
    const DebugMessageTypeBits logged_types = log != nullptr ? log->logged_types : DebugMessageTypeBits::All;
    const bool is_logged = !!(forge_types & logged_types);

    DebugMessageSeverity forge_severity = DebugMessageSeverity::Info;
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
    {
        forge_severity = DebugMessageSeverity::Error;
        if (is_logged)
        {
            RNDR_LOG_ERROR("[Vulkan Validation] {}", callback_data->pMessage);
        }
    }
    else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
    {
        forge_severity = DebugMessageSeverity::Warning;
        if (is_logged)
        {
            RNDR_LOG_WARNING("[Vulkan Validation] {}", callback_data->pMessage);
        }
    }
    else if (is_logged)
    {
        RNDR_LOG_INFO("[Vulkan Validation] {}", callback_data->pMessage);
    }

    if (log == nullptr)
    {
        return VK_FALSE;
    }
    for (Rndr::i32 type_index = 0; type_index < 3; ++type_index)
    {
        const auto type_bit = static_cast<DebugMessageTypeBits>(1 << type_index);
        if (!!(forge_types & type_bit))
        {
            ++log->counts[static_cast<Rndr::i32>(forge_severity)][type_index];
        }
    }
    if (forge_severity == DebugMessageSeverity::Info)
    {
        // Only the count is kept. A long run reports thousands of these and none of them says anything went
        // wrong, so storing them would grow without bound to no purpose.
        return VK_FALSE;
    }
    if (log->messages.GetSize() < static_cast<Rndr::i32>(log->max_stored_messages))
    {
        // The names of the objects a message is about are handed over beside the text rather than inside it,
        // so a message about "shadow map" reads as one only if they are collected here.
        Opal::StringUtf8 objects;
        for (Rndr::u32 object_index = 0; object_index < callback_data->objectCount; ++object_index)
        {
            const char* object_name = callback_data->pObjects[object_index].pObjectName;
            if (object_name == nullptr)
            {
                continue;
            }
            if (!objects.IsEmpty())
            {
                objects += Opal::StringUtf8(", ");
            }
            objects += Opal::StringUtf8(object_name);
        }
        log->messages.PushBack(DebugMessage{
            .severity = forge_severity, .types = forge_types, .text = callback_data->pMessage, .objects = Opal::Move(objects)});
    }
    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo()
{
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_create_info.pfnUserCallback = DebugCallback;
    debug_create_info.pUserData = nullptr;
    return debug_create_info;
}

constexpr const char* k_validation_layer_name = "VK_LAYER_KHRONOS_validation";

#if defined(RNDR_FORGE_VALIDATION)
/**
 * Check whether the validation layer is installed on this machine. It ships with the Vulkan SDK, so a build with
 * RNDR_FORGE_VALIDATION still has to cope with it being missing at run time.
 */
bool IsValidationLayerAvailable()
{
    Rndr::u32 layer_count = 0;
    if (vkEnumerateInstanceLayerProperties(&layer_count, nullptr) != VK_SUCCESS || layer_count == 0)
    {
        return false;
    }
    Opal::DynamicArray<VkLayerProperties> layers(layer_count);
    if (vkEnumerateInstanceLayerProperties(&layer_count, layers.GetData()) != VK_SUCCESS)
    {
        return false;
    }
    for (const VkLayerProperties& layer : layers)
    {
        if (strcmp(layer.layerName, k_validation_layer_name) == 0)
        {
            return true;
        }
    }
    return false;
}
#endif
}  // namespace

Rndr::Forge::GraphicsContext::GraphicsContext(const GraphicsContextDesc& desc) : m_desc(desc.Clone())
{
    // Handed over to the destructor on the last line of this function, and given back by the guard on any
    // path out of here that throws before then.
    VolkUse volk_use;

    bool use_validation_layer = false;
#if defined(RNDR_FORGE_VALIDATION)
    use_validation_layer = IsValidationLayerAvailable();
    if (!use_validation_layer)
    {
        RNDR_LOG_WARNING("Vulkan validation layer requested but {} is not installed, continuing without it!", k_validation_layer_name);
    }
#endif

    // Check if all the requested instance extensions are supported
    Opal::DynamicArray<const char*> required_extensions = GetRequiredInstanceExtensions(desc, use_validation_layer);
    const Opal::DynamicArray<VkExtensionProperties> supported_extensions = GetSupportedInstanceExtensions();
    for (const char* required_extension_name : required_extensions)
    {
        bool is_found = false;
        for (const VkExtensionProperties& supported_extension : supported_extensions)
        {
            if (strcmp(required_extension_name, supported_extension.extensionName) == 0)
            {
                is_found = true;
                break;
            }
        }
        if (!is_found)
        {
            throw Opal::Exception("Extension not supported!");
        }
    }

    // Creation of instance
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Rndr Forge API";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "RNDR";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<u32>(required_extensions.GetSize());
    create_info.ppEnabledExtensionNames = required_extensions.GetData();
    create_info.enabledLayerCount = 0;

    // The messenger below only covers the lifetime of the instance, so chaining a second one into the create info is
    // what makes the messages of vkCreateInstance and vkDestroyInstance visible.
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = MakeDebugMessengerCreateInfo();
    const bool collect_debug_messages = m_desc.collect_debug_messages || use_validation_layer;
    m_debug_utils_enabled = collect_debug_messages;
    if (collect_debug_messages)
    {
        m_debug_log = Opal::MakeShared<DebugMessageLog>(Opal::GetDefaultAllocator());
        m_debug_log->max_stored_messages = m_desc.max_stored_debug_messages;
        m_debug_log->logged_types = m_desc.logged_message_types;
        // The callback is handed the log rather than the context, since the context can be moved afterwards
        // and there is no way to update this pointer once the messenger exists.
        debug_create_info.pUserData = m_debug_log.Get();
        create_info.pNext = &debug_create_info;
    }
    if (use_validation_layer)
    {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = &k_validation_layer_name;
    }

    VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateInstance");
    }
    volkLoadInstance(m_instance);

    // Creation of debug messanger
    if (collect_debug_messages)
    {
        result = CreateDebugUtilsMessengerEXT(m_instance, &debug_create_info, nullptr, &m_debug_messenger);
        if (result != VK_SUCCESS)
        {
            // The instance above is this constructor's to release, since the destructor does not run for an
            // object whose constructor threw. Not through Destroy(), which gives the volk count back as well
            // and would take it a second time from the guard on the way out.
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            throw VulkanException(result, "vkCreateDebugUtilsMessengerEXT");
        }
    }
    if (use_validation_layer)
    {
        RNDR_LOG_INFO("Vulkan validation layer enabled.");
    }
    // Past every throw, so the count this context holds is now the destructor's to give back.
    volk_use.committed = true;
}

Rndr::Forge::GraphicsContext::~GraphicsContext()
{
    Destroy();
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* allocator)
{
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (func != nullptr)
    {
        func(instance, debug_messenger, allocator);
    }
}

Rndr::Forge::GraphicsContext::GraphicsContext(Rndr::Forge::GraphicsContext&& other) noexcept
    : m_instance(other.m_instance),
      m_debug_messenger(other.m_debug_messenger),
      m_desc(Opal::Move(other.m_desc)),
      m_debug_log(Opal::Move(other.m_debug_log)),
      m_debug_utils_enabled(other.m_debug_utils_enabled)
{
    other.m_instance = VK_NULL_HANDLE;
    other.m_debug_messenger = VK_NULL_HANDLE;
    other.m_desc = {};
    other.m_debug_log = {};
    other.m_debug_utils_enabled = false;
}

Rndr::Forge::GraphicsContext& Rndr::Forge::GraphicsContext::operator=(Rndr::Forge::GraphicsContext&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    Destroy();

    m_instance = other.m_instance;
    m_debug_messenger = other.m_debug_messenger;
    m_desc = Opal::Move(other.m_desc);
    m_debug_log = Opal::Move(other.m_debug_log);
    m_debug_utils_enabled = other.m_debug_utils_enabled;

    other.m_instance = VK_NULL_HANDLE;
    other.m_debug_messenger = VK_NULL_HANDLE;
    other.m_desc = {};
    other.m_debug_log = {};
    other.m_debug_utils_enabled = false;

    return *this;
}

Opal::ArrayView<const Rndr::Forge::DebugMessage> Rndr::Forge::GraphicsContext::GetDebugMessages() const
{
    if (!m_debug_log.IsValid())
    {
        return {};
    }
    return {m_debug_log->messages.GetData(), m_debug_log->messages.GetSize()};
}

Rndr::u32 Rndr::Forge::GraphicsContext::GetDebugMessageCount(DebugMessageSeverity severity, DebugMessageTypeBits types) const
{
    if (!m_debug_log.IsValid())
    {
        return 0;
    }
    if (severity != DebugMessageSeverity::Info && severity != DebugMessageSeverity::Warning && severity != DebugMessageSeverity::Error)
    {
        throw Opal::Exception("Unknown debug message severity!");
    }
    u32 count = 0;
    for (i32 type_index = 0; type_index < 3; ++type_index)
    {
        const auto type_bit = static_cast<DebugMessageTypeBits>(1 << type_index);
        if (!!(types & type_bit))
        {
            count += m_debug_log->counts[static_cast<i32>(severity)][type_index];
        }
    }
    return count;
}

void Rndr::Forge::GraphicsContext::ClearDebugMessages()
{
    if (!m_debug_log.IsValid())
    {
        return;
    }
    m_debug_log->messages.Clear();
    for (i32 severity_index = 0; severity_index < 3; ++severity_index)
    {
        for (i32 type_index = 0; type_index < 3; ++type_index)
        {
            m_debug_log->counts[severity_index][type_index] = 0;
        }
    }
}

void Rndr::Forge::GraphicsContext::Destroy()
{
    if (m_debug_messenger != VK_NULL_HANDLE)
    {
        DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
        m_debug_messenger = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
        // Holding an instance is what says this context counted itself in: a moved-from or empty one has none
        // and must not give back a count it never took.
        ReleaseVolk();
    }
}

Opal::DynamicArray<const char*> Rndr::Forge::GraphicsContext::GetRequiredInstanceExtensions(const Rndr::Forge::GraphicsContextDesc& desc,
                                                                                            bool use_validation_layer)
{
    Opal::DynamicArray<const char*> required_extension_names;
    if (desc.required_instance_extensions.GetSize() > 0)
    {
        required_extension_names.Resize(desc.required_instance_extensions.GetSize());
        for (int i = 0; i < desc.required_instance_extensions.GetSize(); ++i)
        {
            required_extension_names[i] = desc.required_instance_extensions[i].GetData();
        }
    }
    // We need this extension if we want to display the image to the display
    required_extension_names.PushBack(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(OPAL_PLATFORM_WINDOWS)
    // We need it if we want to display the image to the display on Windows
    required_extension_names.PushBack(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
    // The validation layer has nowhere to report to without the debug messenger, so it pulls in the extension itself.
    if (desc.collect_debug_messages || use_validation_layer)
    {
        required_extension_names.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return required_extension_names;
}

Opal::DynamicArray<VkExtensionProperties> Rndr::Forge::GraphicsContext::GetSupportedInstanceExtensions()
{
    u32 count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

    Opal::DynamicArray<VkExtensionProperties> extensions;
    if (count == 0)
    {
        return extensions;
    }
    extensions.Resize(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.GetData());
    return extensions;
}

Opal::DynamicArray<Rndr::Forge::PhysicalDevice> Rndr::Forge::GraphicsContext::EnumeratePhysicalDevices() const
{
    u32 gpu_count = 0;
    VkResult result = vkEnumeratePhysicalDevices(m_instance, &gpu_count, nullptr);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkEnumeratePhysicalDevices");
    }

    // A machine with no Vulkan capable device throws rather than handing back an empty list. Every caller of
    // this either indexes the first element or hands the list to SelectPhysicalDevice, and there is nothing
    // useful either can do with nothing - an empty list is a result every caller has to check for and none of
    // them did, which makes it an out of bounds read waiting for the one machine that has no device.
    //
    // Distinct from the throws above it, which say the enumeration itself failed. This one says it succeeded
    // and found nothing, so it carries that sentence rather than a VkResult that never came back.
    if (gpu_count == 0)
    {
        throw Opal::Exception("This machine has no Vulkan capable physical device!");
    }

    Opal::DynamicArray<VkPhysicalDevice> physical_devices(gpu_count);
    result = vkEnumeratePhysicalDevices(m_instance, &gpu_count, physical_devices.GetData());
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkEnumeratePhysicalDevices");
    }

    Opal::DynamicArray<PhysicalDevice> gpu_list;
    for (const VkPhysicalDevice& device : physical_devices)
    {
        gpu_list.PushBack(PhysicalDevice(device));
    }
    return gpu_list;
}
