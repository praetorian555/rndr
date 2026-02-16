#include "rndr/advanced/swap-chain.hpp"

#include "opal/math-base.h"

#if defined(OPAL_PLATFORM_WINDOWS)
#include "rndr/platform/windows-header.hpp"
#endif

#include "opal/container/in-place-array.h"

#include "rndr/advanced/device.hpp"
#include "rndr/advanced/physical-device.hpp"
#include "rndr/advanced/synchronization.hpp"
#include "rndr/advanced/vulkan-exception.hpp"
#include "rndr/log.hpp"
#include "rndr/pixel-format.hpp"

Rndr::AdvancedSurface::AdvancedSurface(const AdvancedGraphicsContext& context, Opal::Ref<const GenericWindow> window) : m_window(window)
{
#if defined(OPAL_PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR surface_create_info{};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hwnd = reinterpret_cast<HWND>(m_window->GetNativeHandle());
    surface_create_info.hinstance = GetModuleHandle(nullptr);
    const VkResult result = vkCreateWin32SurfaceKHR(context.GetInstance(), &surface_create_info, nullptr, &m_surface);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateWin32SurfaceKHR");
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

Rndr::AdvancedSurface::AdvancedSurface(AdvancedSurface&& other) noexcept : m_surface(other.m_surface), m_context(std::move(other.m_context))
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
            throw VulkanException(result, "vkGetPhysicalDeviceSurfacePresentModesKHR");
        }
    }
    return details;
}

Rndr::AdvancedSwapChain::AdvancedSwapChain(const AdvancedDevice& device, const AdvancedSurface& surface, const AdvancedSwapChainDesc& desc)
    : m_device(device), m_surface(surface), m_desc(desc)
{
    Recreate();
}

Rndr::AdvancedSwapChain::~AdvancedSwapChain()
{
    Destroy();
}

Rndr::AdvancedSwapChain::AdvancedSwapChain(AdvancedSwapChain&& other) noexcept
    : m_swap_chain(other.m_swap_chain),
      m_device(std::move(other.m_device)),
      m_surface(std::move(other.m_surface)),
      m_color_textures(std::move(other.m_color_textures)),
      m_depth_texture(std::move(other.m_depth_texture)),
      m_desc(other.m_desc),
      m_extent(other.m_extent)
{
    other.m_swap_chain = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_surface = nullptr;
    other.m_color_textures.Clear();
    other.m_desc = {};
    other.m_extent = {};
}

Rndr::AdvancedSwapChain& Rndr::AdvancedSwapChain::operator=(AdvancedSwapChain&& other) noexcept
{
    Destroy();

    m_swap_chain = other.m_swap_chain;
    m_device = std::move(other.m_device);
    m_surface = std::move(other.m_surface);
    m_color_textures = std::move(other.m_color_textures);
    m_depth_texture = std::move(other.m_depth_texture);
    m_desc = other.m_desc;
    m_extent = other.m_extent;

    other.m_swap_chain = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_surface = nullptr;
    other.m_color_textures.Clear();
    other.m_desc = {};
    other.m_extent = {};

    return *this;
}

void Rndr::AdvancedSwapChain::Destroy()
{
    for (AdvancedTexture& color_texture : m_color_textures)
    {
        color_texture.Destroy();
    }
    m_color_textures.Clear();
    m_depth_texture.Destroy();
    if (m_swap_chain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device->GetNativeDevice(), m_swap_chain, nullptr);
        m_swap_chain = VK_NULL_HANDLE;
    }
    m_surface = nullptr;
    m_device = nullptr;
    m_desc = {};
    m_extent = {};
}

Rndr::u32 Rndr::AdvancedSwapChain::AcquireImage(const Opal::Ref<AdvancedSemaphore>& semaphore)
{
    u32 image_index = UINT32_MAX;
    bool should_run_again = false;
    do
    {
        const VkResult result = vkAcquireNextImageKHR(m_device->GetNativeDevice(), m_swap_chain, UINT64_MAX,
                                                      semaphore->GetNativeSemaphore(), VK_NULL_HANDLE, &image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            Destroy();
            Recreate();
            should_run_again = true;
        }
        else if (result != VK_SUCCESS)
        {
            throw VulkanException(result, "vkAcquireNextImageKHR");
        }
    } while (should_run_again);
    return image_index;
}

void Rndr::AdvancedSwapChain::Present(u32 image_index, Opal::Ref<AdvancedDeviceQueue> queue, Opal::Ref<AdvancedSemaphore> semaphore)
{
    Opal::DynamicArray<VkSemaphore> wait_semaphores{semaphore->GetNativeSemaphore()};
    const VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores.GetData(),
        .swapchainCount = 1,
        .pSwapchains = &m_swap_chain,
        .pImageIndices = &image_index,
    };
    const VkResult result = vkQueuePresentKHR(queue->GetNativeQueue(), &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        Destroy();
        Recreate();
        return;
    }
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkQueuePresentKHR");
    }
}

void Rndr::AdvancedSwapChain::Recreate()
{
    const AdvancedSwapChainSupportDetails swap_chain_support = m_surface->GetSwapChainSupportDetails(m_device->GetPhysicalDevice());
    bool is_supported = false;
    for (auto available_format : swap_chain_support.formats)
    {
        if (available_format.format == ToVkFormat(m_desc.pixel_format) && available_format.colorSpace == m_desc.color_space)
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
        if (available_present_mode == m_desc.present_mode)
        {
            is_supported = true;
            break;
        }
    }
    if (!is_supported)
    {
        throw Opal::Exception("Swap chain present mode not supported!");
    }

    const GenericWindow& window = m_surface->GetWindow();
    const Vector2i window_size = window.GetSize().GetValue();
    VkExtent2D extent = {.width = static_cast<u32>(window_size.x), .height = static_cast<u32>(window_size.y)};
    extent.width = Opal::Clamp(extent.width, swap_chain_support.capabilities.minImageExtent.width,
                               swap_chain_support.capabilities.maxImageExtent.width);
    extent.height = Opal::Clamp(extent.height, swap_chain_support.capabilities.minImageExtent.height,
                                swap_chain_support.capabilities.maxImageExtent.height);
    RNDR_LOG_INFO("Requested swap chain extent: (%d, %d)", window_size.x, window_size.y);
    RNDR_LOG_INFO("Swap chain extent: (%d, %d)", extent.width, extent.height);

    u32 image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
    {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.oldSwapchain = m_swap_chain;
    create_info.surface = m_surface->GetNativeSurface();
    create_info.minImageCount = image_count;
    create_info.imageFormat = ToVkFormat(m_desc.pixel_format);
    create_info.imageColorSpace = m_desc.color_space;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto graphics_queue = m_device->GetQueue(QueueFamily::Graphics);
    auto present_queue = m_device->GetQueue(QueueFamily::Present);

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
    create_info.presentMode = m_desc.present_mode;
    // If set to VK_TRUE it means that we don't care about the color of the pixels if they are occluded by other window.
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(m_device->GetNativeDevice(), &create_info, nullptr, &m_swap_chain);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateSwapchainKHR");
    }

    result = vkGetSwapchainImagesKHR(m_device->GetNativeDevice(), m_swap_chain, &image_count, nullptr);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkGetSwapchainImagesKHR");
    }

    Opal::DynamicArray<VkImage> images;
    images.Resize(image_count);
    result = vkGetSwapchainImagesKHR(m_device->GetNativeDevice(), m_swap_chain, &image_count, images.GetData());
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkGetSwapchainImagesKHR");
    }
    for (VkImage image : images)
    {
        AdvancedTexture texture(m_device, image, AdvancedTextureDesc{
            .format = m_desc.pixel_format,
            .width = extent.width,
            .height = extent.height
        });
        m_color_textures.PushBack(std::move(texture));
    }
    if (m_desc.use_depth)
    {
        m_depth_texture = AdvancedTexture{m_device,
                                          {.image_type = VK_IMAGE_TYPE_2D,
                                           .format = m_desc.depth_pixel_format,
                                           .width = extent.width,
                                           .height = extent.height,
                                           .sample_count = VK_SAMPLE_COUNT_1_BIT,
                                           .image_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                           .view_type = VK_IMAGE_VIEW_TYPE_2D,
                                           .subresource_range = {.aspect_mask = ImageAspectBits::Depth}}};
    }
}
