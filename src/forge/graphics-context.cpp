#include "rndr/forge/graphics-context.hpp"

#include "opal/defines.h"

#if defined(OPAL_PLATFORM_WINDOWS)
#include "rndr/platform/windows-header.hpp"
#endif

#include "opal/container/dynamic-array.h"

#include "rndr/forge/vulkan-exception.hpp"
#include "rndr/log.hpp"

namespace
{
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

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void*)
{
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
    {
        RNDR_LOG_ERROR("[Vulkan Validation] {}", callback_data->pMessage);
    }
    else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
    {
        RNDR_LOG_WARNING("[Vulkan Validation] {}", callback_data->pMessage);
    }
    else
    {
        RNDR_LOG_INFO("[Vulkan Validation] {}", callback_data->pMessage);
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
    VkResult result = volkInitialize();
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "volkInitialize");
    }

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
    const VkDebugUtilsMessengerCreateInfoEXT debug_create_info = MakeDebugMessengerCreateInfo();
    const bool collect_debug_messages = m_desc.collect_debug_messages || use_validation_layer;
    if (collect_debug_messages)
    {
        create_info.pNext = &debug_create_info;
    }
    if (use_validation_layer)
    {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = &k_validation_layer_name;
    }

    result = vkCreateInstance(&create_info, nullptr, &m_instance);
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
            throw VulkanException(result, "vkCreateDebugUtilsMessengerEXT");
        }
    }
    if (use_validation_layer)
    {
        RNDR_LOG_INFO("Vulkan validation layer enabled.");
    }
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
    : m_instance(other.m_instance), m_debug_messenger(other.m_debug_messenger), m_desc(Opal::Move(other.m_desc))
{
    other.m_instance = VK_NULL_HANDLE;
    other.m_debug_messenger = VK_NULL_HANDLE;
    other.m_desc = {};
}

Rndr::Forge::GraphicsContext& Rndr::Forge::GraphicsContext::operator=(Rndr::Forge::GraphicsContext&& other) noexcept
{
    Destroy();

    m_instance = other.m_instance;
    m_debug_messenger = other.m_debug_messenger;
    m_desc = Opal::Move(other.m_desc);

    other.m_instance = VK_NULL_HANDLE;
    other.m_debug_messenger = VK_NULL_HANDLE;
    other.m_desc = {};

    return *this;
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
        // volk holds process wide state, so only the context that initialized it may tear it down. Doing this
        // unconditionally would let an empty or moved-from context unload Vulkan out from under a live one.
        volkFinalize();
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

    // An empty list is a valid answer - the machine has no Vulkan capable device - so a failure has to throw rather
    // than return one, otherwise the caller cannot tell the two apart.
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
