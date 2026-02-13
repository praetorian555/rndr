#include "rndr/advanced/swap-chain.hpp"

#include "opal/math-base.h"

#if defined(OPAL_PLATFORM_WINDOWS)
#include "rndr/platform/windows-header.hpp"
#endif

#include "../../build/opengl-msvc-opt-debug/_deps/opal-src/include/opal/container/in-place-array.h"
#include "rndr/pixel-format.hpp"
#include "rndr/advanced/device.hpp"
#include "rndr/advanced/physical-device.hpp"
#include "rndr/log.hpp"

Rndr::AdvancedSurface::AdvancedSurface(const AdvancedGraphicsContext& context, NativeWindowHandle window_handle)
{
#if defined(OPAL_PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR surface_create_info{};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hwnd = reinterpret_cast<HWND>(window_handle);
    surface_create_info.hinstance = GetModuleHandle(nullptr);
    const VkResult result = vkCreateWin32SurfaceKHR(context.GetInstance(), &surface_create_info, nullptr, &m_surface);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create Vulkan surface!");
    }
    m_context = &context;
#else
#error "Platform not supported!"
    return false;
#endif
}

Rndr::AdvancedSurface::~AdvancedSurface()
{
    Destroy();
}

Rndr::AdvancedSurface::AdvancedSurface(AdvancedSurface&& other) noexcept : m_surface(other.m_surface), m_context(Opal::Move(other.m_context))
{
    other.m_surface = VK_NULL_HANDLE;
    other.m_context = nullptr;
}

Rndr::AdvancedSurface& Rndr::AdvancedSurface::operator=(AdvancedSurface&& other) noexcept
{
    Destroy();
    m_surface = other.m_surface;
    m_context = other.m_context;
    other.m_surface = VK_NULL_HANDLE;
    other.m_context = nullptr;
    return *this;
}

void Rndr::AdvancedSurface::Destroy()
{
    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_context->GetInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    m_context = nullptr;
}

Rndr::AdvancedSwapChainSupportDetails Rndr::AdvancedSurface::GetSwapChainSupportDetails(const AdvancedPhysicalDevice& device) const
{
    AdvancedSwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.GetNativePhysicalDevice(), m_surface, &details.capabilities);

    u32 format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.GetNativePhysicalDevice(), m_surface, &format_count, nullptr);
    if (format_count > 0)
    {
        details.formats.Resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device.GetNativePhysicalDevice(), m_surface, &format_count, details.formats.GetData());
    }

    u32 present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.GetNativePhysicalDevice(), m_surface, &present_mode_count, nullptr);
    if (present_mode_count != 0)
    {
        details.present_modes.Resize(present_mode_count);
        const VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device.GetNativePhysicalDevice(), m_surface, &present_mode_count,
                                                                          details.present_modes.GetData());
        if (result != VK_SUCCESS)
        {
            throw Opal::Exception("Failed to get present modes!");
        }
    }
    return details;
}

Rndr::AdvancedSwapChain::AdvancedSwapChain(const AdvancedDevice& device, const AdvancedSurface& surface, const AdvancedSwapChainDesc& desc)
{
const AdvancedSwapChainSupportDetails swap_chain_support = surface.GetSwapChainSupportDetails(device.GetPhysicalDevice());
    bool is_supported = false;
    for (auto available_format : swap_chain_support.formats)
    {
        if (available_format.format == ToVkFormat(desc.pixel_format) && available_format.colorSpace == desc.color_space)
        {
            is_supported = true;
            break;
        }
    }
    if (!is_supported)
    {
        throw Opal::Exception("Swap chain format not supported!");
    }

    is_supported = false;
    for (auto available_present_mode : swap_chain_support.present_modes)
    {
        if (available_present_mode == desc.present_mode)
        {
            is_supported = true;
            break;
        }
    }
    if (!is_supported)
    {
        throw Opal::Exception("Swap chain present mode not supported!");
    }

    VkExtent2D extent = {desc.width, desc.height};
    extent.width = Opal::Clamp(extent.width, swap_chain_support.capabilities.minImageExtent.width,
                               swap_chain_support.capabilities.maxImageExtent.width);
    extent.height = Opal::Clamp(extent.height, swap_chain_support.capabilities.minImageExtent.height,
                                swap_chain_support.capabilities.maxImageExtent.height);
    RNDR_LOG_INFO("Requested swap chain extent: (%d, %d)", desc.width, desc.height);
    RNDR_LOG_INFO("Swap chain extent: (%d, %d)", extent.width, extent.height);

    u32 image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
    {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface.GetNativeSurface();
    create_info.minImageCount = image_count;
    create_info.imageFormat = ToVkFormat(desc.pixel_format);
    create_info.imageColorSpace = desc.color_space;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto graphics_queue = device.GetQueue(QueueFamily::Graphics);
    auto present_queue = device.GetQueue(QueueFamily::Present);

    if (graphics_queue != present_queue)
    {
        // If graphics and present queues are different, we use VK_SHARING_MODE_CONCURRENT
        // to allow concurrent access to the resources from different queues
        Opal::InPlaceArray<u32, 2> indices = {graphics_queue->GetQueueFamilyIndex(), present_queue->GetQueueFamilyIndex()};
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = static_cast<u32>(indices.GetSize());
        create_info.pQueueFamilyIndices = indices.GetData();
    }
    else
    {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;  // swap_chain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = desc.present_mode;
    // If set to VK_TRUE it means that we don't care about the color of the pixels if they are occluded by other window.
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(device.GetNativeDevice(), &create_info, nullptr, &m_swap_chain);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create swap chain!");
    }

    result = vkGetSwapchainImagesKHR(device.GetNativeDevice(), m_swap_chain, &image_count, nullptr);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to get number of swap chain images!");
    }

    m_images.Resize(image_count);
    result = vkGetSwapchainImagesKHR(device.GetNativeDevice(), m_swap_chain, &image_count, m_images.GetData());
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to get swap chain images!");
    }

    m_image_views.Resize(m_images.GetSize());
    for (u32 i = 0; i < m_image_views.GetSize(); ++i)
    {
        VkImageViewCreateInfo image_view_create_info{};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = m_images[i];
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = ToVkFormat(desc.pixel_format);
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(device.GetNativeDevice(), &image_view_create_info, nullptr, &m_image_views[i]);
        if (result != VK_SUCCESS)
        {
            throw Opal::Exception("Failed to create image view!");
        }
    }

    m_desc = desc;
    m_extent = extent;
    m_device = device;
}

Rndr::AdvancedSwapChain::~AdvancedSwapChain()
{
    Destroy();
}

Rndr::AdvancedSwapChain::AdvancedSwapChain(AdvancedSwapChain&& other) noexcept
    : m_swap_chain(other.m_swap_chain),
      m_device(Opal::Move(other.m_device)),
      m_images(Opal::Move(other.m_images)),
      m_image_views(Opal::Move(other.m_image_views)),
      m_desc(other.m_desc),
      m_extent(other.m_extent)
{
    other.m_swap_chain = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_images.Clear();
    other.m_image_views.Clear();
    other.m_desc = {};
    other.m_extent = {};
}

Rndr::AdvancedSwapChain& Rndr::AdvancedSwapChain::operator=(AdvancedSwapChain&& other) noexcept
{
    Destroy();

    m_swap_chain = other.m_swap_chain;
    m_device = Opal::Move(other.m_device);
    m_images = Opal::Move(other.m_images);
    m_image_views = Opal::Move(other.m_image_views);
    m_desc = other.m_desc;
    m_extent = other.m_extent;

    other.m_swap_chain = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_images.Clear();
    other.m_image_views.Clear();
    other.m_desc = {};
    other.m_extent = {};

    return *this;
}

void Rndr::AdvancedSwapChain::Destroy()
{
    for (const VkImageView& image_view : m_image_views)
    {
        vkDestroyImageView(m_device->GetNativeDevice(), image_view, nullptr);
    }
    m_image_views.Clear();
    m_images.Clear();
    if (m_swap_chain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device->GetNativeDevice(), m_swap_chain, nullptr);
        m_swap_chain = VK_NULL_HANDLE;
    }
    m_desc = {};
    m_extent = {};
    m_device = nullptr;
}
