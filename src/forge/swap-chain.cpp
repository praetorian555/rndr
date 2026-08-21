#include "rndr/forge/swap-chain.hpp"

#include "opal/math-base.h"

#if defined(OPAL_PLATFORM_WINDOWS)
#include "rndr/platform/windows-header.hpp"
#endif

#include "opal/container/in-place-array.h"

#include "rndr/forge/device.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/vulkan-exception.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/log.hpp"
#include "rndr/pixel-format.hpp"

static VkPresentModeKHR ToVkPresentMode(Rndr::Forge::PresentMode present_mode)
{
    switch (present_mode)
    {
        case Rndr::Forge::PresentMode::Immediate:
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case Rndr::Forge::PresentMode::Mailbox:
            return VK_PRESENT_MODE_MAILBOX_KHR;
        case Rndr::Forge::PresentMode::Fifo:
            return VK_PRESENT_MODE_FIFO_KHR;
        case Rndr::Forge::PresentMode::FifoRelaxed:
            return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    }
    throw Opal::Exception("Unknown present mode!");
}

static VkColorSpaceKHR ToVkColorSpace(Rndr::Forge::ColorSpace color_space)
{
    switch (color_space)
    {
        case Rndr::Forge::ColorSpace::SrgbNonlinear:
            return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        case Rndr::Forge::ColorSpace::ExtendedSrgbLinear:
            return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
        case Rndr::Forge::ColorSpace::Hdr10St2084:
            return VK_COLOR_SPACE_HDR10_ST2084_EXT;
        case Rndr::Forge::ColorSpace::DisplayP3Nonlinear:
            return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
    }
    throw Opal::Exception("Unknown color space!");
}

Rndr::Forge::Surface::Surface(const GraphicsContext& context, const GenericWindow& window) : m_window(window)
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

Rndr::Forge::Surface::~Surface()
{
    Destroy();
}

Rndr::Forge::Surface::Surface(Surface&& other) noexcept
    : m_surface(other.m_surface), m_window(std::move(other.m_window)), m_context(std::move(other.m_context))
{
    other.m_surface = VK_NULL_HANDLE;
    other.m_window = nullptr;
    other.m_context = nullptr;
}

Rndr::Forge::Surface& Rndr::Forge::Surface::operator=(Surface&& other) noexcept
{
    Destroy();
    m_surface = other.m_surface;
    m_window = std::move(other.m_window);
    m_context = std::move(other.m_context);
    other.m_surface = VK_NULL_HANDLE;
    other.m_window = nullptr;
    other.m_context = nullptr;
    return *this;
}

void Rndr::Forge::Surface::Destroy()
{
    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_context->GetInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    m_window = nullptr;
    m_context = nullptr;
}

Rndr::Forge::SwapChainSupportDetails Rndr::Forge::Surface::GetSwapChainSupportDetails(const PhysicalDevice& device) const
{
    SwapChainSupportDetails details;

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

Rndr::Forge::SwapChain::SwapChain(const Device& device, const Surface& surface, const SwapChainDesc& desc)
    : m_desc(desc), m_device(device), m_surface(surface)
{
    Recreate();
}

Rndr::Forge::SwapChain::~SwapChain()
{
    Destroy();
}

Rndr::Forge::SwapChain::SwapChain(SwapChain&& other) noexcept
    : m_desc(other.m_desc),
      m_swap_chain(other.m_swap_chain),
      m_extent(other.m_extent),
      m_device(std::move(other.m_device)),
      m_surface(std::move(other.m_surface)),
      m_color_textures(std::move(other.m_color_textures)),
      m_depth_texture(std::move(other.m_depth_texture)),
      m_current_image_index(other.m_current_image_index)
{
    other.m_swap_chain = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_surface = nullptr;
    other.m_color_textures.Clear();
    other.m_desc = {};
    other.m_extent = {};
    other.m_current_image_index = k_invalid_image_index;
}

Rndr::Forge::SwapChain& Rndr::Forge::SwapChain::operator=(SwapChain&& other) noexcept
{
    Destroy();

    m_swap_chain = other.m_swap_chain;
    m_device = std::move(other.m_device);
    m_surface = std::move(other.m_surface);
    m_color_textures = std::move(other.m_color_textures);
    m_depth_texture = std::move(other.m_depth_texture);
    m_desc = other.m_desc;
    m_extent = other.m_extent;
    m_current_image_index = other.m_current_image_index;

    other.m_swap_chain = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_surface = nullptr;
    other.m_color_textures.Clear();
    other.m_desc = {};
    other.m_extent = {};
    other.m_current_image_index = k_invalid_image_index;

    return *this;
}

void Rndr::Forge::SwapChain::DestroyImages()
{
    for (Texture& color_texture : m_color_textures)
    {
        color_texture.Destroy();
    }
    m_color_textures.Clear();
    m_depth_texture.Destroy();
    // The index pointed into the array that just went away, so nothing is acquired any more.
    m_current_image_index = k_invalid_image_index;
}

void Rndr::Forge::SwapChain::DestroySwapChain()
{
    if (m_swap_chain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device->GetNativeDevice(), m_swap_chain, nullptr);
        m_swap_chain = VK_NULL_HANDLE;
    }
}

void Rndr::Forge::SwapChain::Destroy()
{
    if (m_device != nullptr && m_swap_chain != VK_NULL_HANDLE)
    {
        // The images may still be in use by frames that were submitted but have not finished yet.
        m_device->WaitForAll();
    }
    DestroyImages();
    if (m_device != nullptr)
    {
        DestroySwapChain();
    }
    m_surface = nullptr;
    m_device = nullptr;
    m_desc = {};
    m_extent = {};
}

const Rndr::Forge::Texture& Rndr::Forge::SwapChain::GetCurrentColorImage() const
{
    if (!HasAcquiredImage())
    {
        throw Opal::Exception("There is no acquired image - AcquireImage has to come first!");
    }
    return m_color_textures[static_cast<i32>(m_current_image_index)];
}

Rndr::Forge::Texture& Rndr::Forge::SwapChain::GetCurrentColorImage()
{
    if (!HasAcquiredImage())
    {
        throw Opal::Exception("There is no acquired image - AcquireImage has to come first!");
    }
    return m_color_textures[static_cast<i32>(m_current_image_index)];
}

VkImageView Rndr::Forge::SwapChain::GetCurrentColorImageView() const
{
    return GetCurrentColorImage().GetNativeImageView();
}

Rndr::Forge::AcquiredImage Rndr::Forge::SwapChain::AcquireImage(const Semaphore& semaphore)
{
    // Above the early return below, so that a window with no client area does not hide the mistake.
    if (semaphore.IsTimeline())
    {
        throw Opal::Exception("Acquiring an image needs a binary semaphore - presentation cannot signal a timeline!");
    }
    if (m_swap_chain == VK_NULL_HANDLE)
    {
        // No swap chain because the window had no client area the last time we tried. Try again for the next frame.
        Recreate();
        return {};
    }

    u32 image_index = k_invalid_image_index;
    const VkResult result = vkAcquireNextImageKHR(m_device->GetNativeDevice(), m_swap_chain, UINT64_MAX,
                                                  semaphore.GetNativeSemaphore(), VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        Recreate();
        return {};
    }
    // A suboptimal swap chain still hands out a usable image and signals the semaphore, so the frame is rendered with
    // it and the recreation happens after the matching present reports the same thing.
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw VulkanException(result, "vkAcquireNextImageKHR");
    }
    m_current_image_index = image_index;
    // The presentation engine is the one writer of a layout that Forge cannot observe, and the specification
    // says the contents of a re-acquired image are undefined. Undefined is also the cheapest thing to
    // transition out of, which is what a swap chain image cleared at the top of every frame wants.
    m_color_textures[static_cast<i32>(image_index)].SetCurrentLayout({}, ImageLayout::Undefined);
    return {SwapChainStatus::Success, image_index};
}

Rndr::Forge::SwapChainStatus Rndr::Forge::SwapChain::Present(DeviceQueue& queue, const Semaphore& semaphore)
{
    if (semaphore.IsTimeline())
    {
        throw Opal::Exception("Presenting needs a binary semaphore - presentation cannot wait on a timeline!");
    }
    if (!HasAcquiredImage())
    {
        throw Opal::Exception("There is no acquired image to present - AcquireImage has to come first!");
    }
    // Cleared before the call rather than after: a present that comes back out of date recreates the swap
    // chain underneath us, and the index would then point into images that no longer exist.
    const u32 image_index = m_current_image_index;
    m_current_image_index = k_invalid_image_index;

    const VkSemaphore wait_semaphore = semaphore.GetNativeSemaphore();
    const VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &wait_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swap_chain,
        .pImageIndices = &image_index,
    };
    const VkResult result = vkQueuePresentKHR(queue.GetNativeQueue(), &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        Recreate();
        return SwapChainStatus::OutOfDate;
    }
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkQueuePresentKHR");
    }
    return SwapChainStatus::Success;
}

namespace
{
/**
 * Pick the extent of the images of the swap chain. Surfaces that dictate their own size report it in currentExtent,
 * the rest are driven by the size of the window. A zero extent means that the window has no client area, as happens
 * while it is minimized, and that no swap chain can be created for it.
 */
VkExtent2D SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Rndr::GenericWindow& window)
{
    constexpr Rndr::u32 k_extent_driven_by_window = 0xFFFFFFFF;
    if (capabilities.currentExtent.width != k_extent_driven_by_window)
    {
        return capabilities.currentExtent;
    }

    const Rndr::Vector2i window_size = window.GetSize();
    if (window_size.x <= 0 || window_size.y <= 0)
    {
        return {.width = 0, .height = 0};
    }
    return {.width = Opal::Clamp(static_cast<Rndr::u32>(window_size.x), capabilities.minImageExtent.width,
                                 capabilities.maxImageExtent.width),
            .height = Opal::Clamp(static_cast<Rndr::u32>(window_size.y), capabilities.minImageExtent.height,
                                  capabilities.maxImageExtent.height)};
}
}  // namespace

void Rndr::Forge::SwapChain::Recreate()
{
    // Frames that were submitted earlier can still be reading from the images that are about to be released.
    m_device->WaitForAll();
    DestroyImages();

    const SwapChainSupportDetails swap_chain_support = m_surface->GetSwapChainSupportDetails(m_device->GetPhysicalDevice());
    const VkColorSpaceKHR color_space = ToVkColorSpace(m_desc.color_space);
    const VkPresentModeKHR present_mode = ToVkPresentMode(m_desc.present_mode);
    bool is_supported = false;
    for (auto available_format : swap_chain_support.formats)
    {
        if (available_format.format == ToVkFormat(m_desc.pixel_format) && available_format.colorSpace == color_space)
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
        if (available_present_mode == present_mode)
        {
            is_supported = true;
            break;
        }
    }
    if (!is_supported)
    {
        throw Opal::Exception("Swap chain present mode not supported!");
    }

    const VkExtent2D extent = SelectExtent(swap_chain_support.capabilities, m_surface->GetWindow());
    if (extent.width == 0 || extent.height == 0)
    {
        // The window has no client area, so there is nothing to present to. Release the swap chain and let the next
        // AcquireImage try again once the window is back.
        DestroySwapChain();
        m_extent = {};
        return;
    }
    RNDR_LOG_INFO("Swap chain extent: ({}, {})", extent.width, extent.height);

    u32 image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
    {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_surface->GetNativeSurface();
    create_info.minImageCount = image_count;
    create_info.imageFormat = ToVkFormat(m_desc.pixel_format);
    create_info.imageColorSpace = color_space;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const DeviceQueue& graphics_queue = m_device->GetQueue(QueueFamily::Graphics);
    const DeviceQueue& present_queue = m_device->GetQueue(QueueFamily::Present);

    // Has to outlive the create info, since the create info only points to it.
    Opal::InPlaceArray<u32, 2> indices;
    if (&graphics_queue != &present_queue)
    {
        // If graphics and present queues are different, we use VK_SHARING_MODE_CONCURRENT
        // to allow concurrent access to the resources from different queues
        indices = {graphics_queue.GetQueueFamilyIndex(), present_queue.GetQueueFamilyIndex()};
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
    create_info.presentMode = present_mode;
    // If set to VK_TRUE it means that we don't care about the color of the pixels if they are occluded by other window.
    create_info.clipped = VK_TRUE;
    // Letting the driver reuse the resources of the swap chain we are replacing.
    create_info.oldSwapchain = m_swap_chain;

    VkSwapchainKHR new_swap_chain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(m_device->GetNativeDevice(), &create_info, nullptr, &new_swap_chain);
    // The old swap chain is retired by the call above, whether it succeeded or not, so it goes away either way.
    DestroySwapChain();
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateSwapchainKHR");
    }
    m_swap_chain = new_swap_chain;
    m_extent = extent;

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
        Texture texture(m_device, image, TextureDesc{
            .format = m_desc.pixel_format,
            .width = extent.width,
            .height = extent.height,
            .usage = TextureUsageBits::ColorAttachment
        });
        m_color_textures.PushBack(std::move(texture));
    }
    if (m_desc.use_depth)
    {
        m_depth_texture = Texture{m_device,
                                          {.dimension = TextureDimension::Texture2D,
                                           .format = m_desc.depth_pixel_format,
                                           .width = extent.width,
                                           .height = extent.height,
                                           .sample_count = SampleCount::Count1,
                                           .usage = TextureUsageBits::DepthStencilAttachment,
                                           .view_type = TextureViewType::Texture2D}};
    }
}
