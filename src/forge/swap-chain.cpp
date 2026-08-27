#include "rndr/forge/swap-chain.hpp"

#include "opal/math-base.h"

#if defined(OPAL_PLATFORM_WINDOWS)
#include "rndr/platform/windows-header.hpp"
#elif defined(OPAL_PLATFORM_LINUX)
#include <xcb/xcb.h>
#endif

#include "opal/container/in-place-array.h"

#include "rndr/forge/device.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/log.hpp"
#include "rndr/pixel-format.hpp"

static Opal::Optional<VkPresentModeKHR> ToVkPresentMode(Rndr::Forge::PresentMode present_mode)
{
    switch (present_mode)
    {
        case Rndr::Forge::PresentMode::Immediate:
            return Opal::Optional<VkPresentModeKHR>(VK_PRESENT_MODE_IMMEDIATE_KHR);
        case Rndr::Forge::PresentMode::Mailbox:
            return Opal::Optional<VkPresentModeKHR>(VK_PRESENT_MODE_MAILBOX_KHR);
        case Rndr::Forge::PresentMode::Fifo:
            return Opal::Optional<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR);
        case Rndr::Forge::PresentMode::FifoRelaxed:
            return Opal::Optional<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_RELAXED_KHR);
    }
    return {};
}

static Opal::Optional<VkColorSpaceKHR> ToVkColorSpace(Rndr::Forge::ColorSpace color_space)
{
    switch (color_space)
    {
        case Rndr::Forge::ColorSpace::SrgbNonlinear:
            return Opal::Optional<VkColorSpaceKHR>(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
        case Rndr::Forge::ColorSpace::ExtendedSrgbLinear:
            return Opal::Optional<VkColorSpaceKHR>(VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT);
        case Rndr::Forge::ColorSpace::Hdr10St2084:
            return Opal::Optional<VkColorSpaceKHR>(VK_COLOR_SPACE_HDR10_ST2084_EXT);
        case Rndr::Forge::ColorSpace::DisplayP3Nonlinear:
            return Opal::Optional<VkColorSpaceKHR>(VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT);
    }
    return {};
}

Opal::Expected<Rndr::Forge::Surface, Rndr::ErrorCode> Rndr::Forge::Surface::Create(const GraphicsContext& context,
                                                                                   const GenericWindow& window)
{
    using Result = Opal::Expected<Surface, ErrorCode>;

#if defined(OPAL_PLATFORM_WINDOWS)
    Surface surface;
    surface.m_window = window;
    VkWin32SurfaceCreateInfoKHR surface_create_info{};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hwnd = reinterpret_cast<HWND>(window.GetNativeHandle());
    surface_create_info.hinstance = GetModuleHandle(nullptr);
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateWin32SurfaceKHR(context.GetInstance(), &surface_create_info, nullptr, &surface.m_surface),
                                 "vkCreateWin32SurfaceKHR", Result);
    surface.m_context = &context;
    return Result(std::move(surface));
#elif defined(OPAL_PLATFORM_LINUX)
    Surface surface;
    surface.m_window = window;
    VkXcbSurfaceCreateInfoKHR surface_create_info{};
    surface_create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surface_create_info.connection = reinterpret_cast<xcb_connection_t*>(window.GetNativeDisplayHandle());
    surface_create_info.window = static_cast<xcb_window_t>(reinterpret_cast<uintptr_t>(window.GetNativeHandle()));
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateXcbSurfaceKHR(context.GetInstance(), &surface_create_info, nullptr, &surface.m_surface),
                                 "vkCreateXcbSurfaceKHR", Result);
    surface.m_context = &context;
    return Result(std::move(surface));
#else
#error "Platform not supported!"
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

Opal::Expected<Rndr::Forge::SwapChainSupportDetails, Rndr::ErrorCode> Rndr::Forge::Surface::GetSwapChainSupportDetails(
    const PhysicalDevice& device) const
{
    using Result = Opal::Expected<SwapChainSupportDetails, ErrorCode>;

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
        RNDR_FORGE_VK_CHECK_EXPECTED(vkGetPhysicalDeviceSurfacePresentModesKHR(device.GetNativePhysicalDevice(), m_surface,
                                                                               &present_mode_count, details.present_modes.GetData()),
                                     "vkGetPhysicalDeviceSurfacePresentModesKHR", Result);
    }
    return Result(std::move(details));
}

Opal::Expected<Rndr::Forge::SwapChain, Rndr::ErrorCode> Rndr::Forge::SwapChain::Create(const Device& device, const Surface& surface,
                                                                                       const SwapChainDesc& desc)
{
    using Result = Opal::Expected<SwapChain, ErrorCode>;

    // Recreate holds the swap chain and its textures the moment each is made, and a later step of it can
    // still give up. Both belong to this local, so what it got that far is released by the destructor.
    SwapChain swap_chain;
    swap_chain.m_desc = desc;
    swap_chain.m_device = device;
    swap_chain.m_surface = surface;
    RNDR_FORGE_CHECK_EXPECTED(swap_chain.Recreate(), Result);
    return Result(std::move(swap_chain));
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
      m_current_texture_index(other.m_current_texture_index)
{
    other.m_swap_chain = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_surface = nullptr;
    other.m_color_textures.Clear();
    other.m_desc = {};
    other.m_extent = {};
    other.m_current_texture_index = k_invalid_texture_index;
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
    m_current_texture_index = other.m_current_texture_index;

    other.m_swap_chain = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_surface = nullptr;
    other.m_color_textures.Clear();
    other.m_desc = {};
    other.m_extent = {};
    other.m_current_texture_index = k_invalid_texture_index;

    return *this;
}

void Rndr::Forge::SwapChain::DestroyTextures()
{
    for (Texture& color_texture : m_color_textures)
    {
        color_texture.Destroy();
    }
    m_color_textures.Clear();
    m_depth_texture.Destroy();
    // The index pointed into the array that just went away, so nothing is acquired any more.
    m_current_texture_index = k_invalid_texture_index;
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
        // The textures may still be in use by frames that were submitted but have not finished yet. A wait
        // that fails has already logged why, and there is nothing else teardown can do about it.
        (void)m_device->WaitForAll();
    }
    DestroyTextures();
    if (m_device != nullptr)
    {
        DestroySwapChain();
    }
    m_surface = nullptr;
    m_device = nullptr;
    m_desc = {};
    m_extent = {};
}

Opal::Expected<const Rndr::Forge::Texture&, Rndr::ErrorCode> Rndr::Forge::SwapChain::GetCurrentColorTexture() const
{
    using Result = Opal::Expected<const Texture&, ErrorCode>;

    if (!HasAcquiredTexture())
    {
        RNDR_LOG_ERROR("Forge: there is no acquired texture - AcquireTexture has to come first");
        return Result(ErrorCode::InvalidArgument);
    }
    return Result(m_color_textures[static_cast<i32>(m_current_texture_index)]);
}

Opal::Expected<Rndr::Forge::Texture&, Rndr::ErrorCode> Rndr::Forge::SwapChain::GetCurrentColorTexture()
{
    using Result = Opal::Expected<Texture&, ErrorCode>;

    if (!HasAcquiredTexture())
    {
        RNDR_LOG_ERROR("Forge: there is no acquired texture - AcquireTexture has to come first");
        return Result(ErrorCode::InvalidArgument);
    }
    return Result(m_color_textures[static_cast<i32>(m_current_texture_index)]);
}

Opal::Expected<Rndr::Forge::AcquiredTexture, Rndr::ErrorCode> Rndr::Forge::SwapChain::AcquireTexture(const Semaphore& semaphore)
{
    using Result = Opal::Expected<AcquiredTexture, ErrorCode>;

    // Above the early return below, so that a window with no client area does not hide the mistake.
    if (semaphore.IsTimeline())
    {
        RNDR_LOG_ERROR("Forge: acquiring a texture needs a binary semaphore - presentation cannot signal a timeline");
        return Result(ErrorCode::InvalidArgument);
    }
    if (m_swap_chain == VK_NULL_HANDLE)
    {
        // No swap chain because the window had no client area the last time we tried. Try again for the next frame.
        RNDR_FORGE_CHECK_EXPECTED(Recreate(), Result);
        return Result(AcquiredTexture{});
    }

    u32 texture_index = k_invalid_texture_index;
    const VkResult result = vkAcquireNextImageKHR(m_device->GetNativeDevice(), m_swap_chain, UINT64_MAX, semaphore.GetNativeSemaphore(),
                                                  VK_NULL_HANDLE, &texture_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RNDR_FORGE_CHECK_EXPECTED(Recreate(), Result);
        return Result(AcquiredTexture{});
    }
    // A suboptimal swap chain still hands out a usable texture and signals the semaphore, so the frame is rendered with
    // it and the recreation happens after the matching present reports the same thing.
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        RNDR_LOG_ERROR("Forge: {} failed: {}", "vkAcquireNextImageKHR", VkResultToString(result));
        return Result(VkResultToErrorCode(result));
    }
    m_current_texture_index = texture_index;
    // The presentation engine is the one writer of a layout that Forge cannot observe, and the specification
    // says the contents of a re-acquired texture are undefined. Undefined is also the cheapest thing to
    // transition out of, which is what a swap chain texture cleared at the top of every frame wants.
    // Undefined covers every subresource of a swap chain texture, so this range check cannot fail.
    (void)m_color_textures[static_cast<i32>(texture_index)].SetCurrentLayout({}, ImageLayout::Undefined);
    return Result(AcquiredTexture{SwapChainStatus::Success, texture_index});
}

Opal::Expected<Rndr::Forge::SwapChainStatus, Rndr::ErrorCode> Rndr::Forge::SwapChain::Present(DeviceQueue& queue,
                                                                                              const Semaphore& semaphore)
{
    using Result = Opal::Expected<SwapChainStatus, ErrorCode>;

    if (semaphore.IsTimeline())
    {
        RNDR_LOG_ERROR("Forge: presenting needs a binary semaphore - presentation cannot wait on a timeline");
        return Result(ErrorCode::InvalidArgument);
    }
    if (!HasAcquiredTexture())
    {
        RNDR_LOG_ERROR("Forge: there is no acquired texture to present - AcquireTexture has to come first");
        return Result(ErrorCode::InvalidArgument);
    }
    // Cleared before the call rather than after: a present that comes back out of date recreates the swap
    // chain underneath us, and the index would then point into textures that no longer exist.
    const u32 texture_index = m_current_texture_index;
    m_current_texture_index = k_invalid_texture_index;

    const VkSemaphore wait_semaphore = semaphore.GetNativeSemaphore();
    const VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &wait_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swap_chain,
        .pImageIndices = &texture_index,
    };
    const VkResult result = vkQueuePresentKHR(queue.GetNativeQueue(), &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        RNDR_FORGE_CHECK_EXPECTED(Recreate(), Result);
        return Result(SwapChainStatus::OutOfDate);
    }
    if (result != VK_SUCCESS)
    {
        RNDR_LOG_ERROR("Forge: {} failed: {}", "vkQueuePresentKHR", VkResultToString(result));
        return Result(VkResultToErrorCode(result));
    }
    return Result(SwapChainStatus::Success);
}

namespace
{
/**
 * Pick the extent of the textures of the swap chain. Surfaces that dictate their own size report it in currentExtent,
 * the rest are driven by the size of the window. A zero extent means that the window has no client area, as happens
 * while it is minimized, and that no swap chain can be created for it.
 */
VkExtent2D SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Rndr::GenericWindow& window)
{
    // The window says whether it is minimized, not the surface: Windows reports a zero currentExtent for a
    // minimized window on its own, but an iconified X11 window keeps its last geometry and the surface
    // keeps reporting it, so presenting would quietly continue into a window nobody can see.
    if (window.IsMinimized())
    {
        return {.width = 0, .height = 0};
    }
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
    return {
        .width = Opal::Clamp(static_cast<Rndr::u32>(window_size.x), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        .height =
            Opal::Clamp(static_cast<Rndr::u32>(window_size.y), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}
}  // namespace

Rndr::ErrorCode Rndr::Forge::SwapChain::Recreate()
{
    // Frames that were submitted earlier can still be reading from the textures that are about to be released.
    RNDR_FORGE_CHECK(m_device->WaitForAll());
    DestroyTextures();

    Opal::Expected<SwapChainSupportDetails, ErrorCode> support_details =
        m_surface->GetSwapChainSupportDetails(m_device->GetPhysicalDevice());
    if (!support_details.HasValue())
    {
        return support_details.GetError();
    }
    const SwapChainSupportDetails& swap_chain_support = support_details.GetValue();
    RNDR_FORGE_TRANSLATE(color_space, ToVkColorSpace(m_desc.color_space), "SwapChainDesc::color_space");
    RNDR_FORGE_TRANSLATE(present_mode, ToVkPresentMode(m_desc.present_mode), "SwapChainDesc::present_mode");
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
        RNDR_LOG_ERROR("Forge: this surface does not offer the swap chain format and colour space the desc asks for");
        return ErrorCode::FeatureNotSupported;
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
        RNDR_LOG_ERROR("Forge: this surface does not offer the swap chain present mode the desc asks for");
        return ErrorCode::FeatureNotSupported;
    }

    const VkExtent2D extent = SelectExtent(swap_chain_support.capabilities, m_surface->GetWindow());
    if (extent.width == 0 || extent.height == 0)
    {
        // The window has no client area, so there is nothing to present to. Release the swap chain and let the next
        // AcquireTexture try again once the window is back.
        DestroySwapChain();
        m_extent = {};
        return ErrorCode::Success;
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
    // A presented frame is only observable when it can be the source of a copy, and that is a usage the
    // surface has to offer. Asked for and missing is refused, rather than handing back textures whose desc
    // says they can be read and whose images cannot.
    TextureUsageBits color_texture_usage = TextureUsageBits::ColorAttachment;
    if (m_desc.allow_readback)
    {
        if ((swap_chain_support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0)
        {
            RNDR_LOG_ERROR(
                "Forge: this surface does not offer swap chain textures that can be copied from, so allow_readback cannot be "
                "honoured");
            return ErrorCode::FeatureNotSupported;
        }
        create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        color_texture_usage |= TextureUsageBits::TransferSource;
    }

    Opal::Expected<const DeviceQueue&, ErrorCode> graphics_queue_result = m_device->GetQueue(QueueFamily::Graphics);
    Opal::Expected<const DeviceQueue&, ErrorCode> present_queue_result = m_device->GetQueue(QueueFamily::Present);
    if (!graphics_queue_result.HasValue() || !present_queue_result.HasValue())
    {
        RNDR_LOG_ERROR("Forge: a swap chain needs a device created with both a graphics and a present queue");
        return ErrorCode::InvalidArgument;
    }
    const DeviceQueue& graphics_queue = graphics_queue_result.GetValue();
    const DeviceQueue& present_queue = present_queue_result.GetValue();

    // A device created against a surface proved this at creation; one created with
    // enable_presentation picked its family without a surface to ask, so the promise is checked
    // here, against the surface actually being presented to.
    VkBool32 present_supported = VK_FALSE;
    RNDR_FORGE_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_device->GetNativePhysicalDevice(), present_queue.GetQueueFamilyIndex(),
                                                             m_surface->GetNativeSurface(), &present_supported),
                        "vkGetPhysicalDeviceSurfaceSupportKHR");
    if (present_supported != VK_TRUE)
    {
        RNDR_LOG_ERROR("Forge: the device's present queue family cannot present to this surface");
        return ErrorCode::FeatureNotSupported;
    }

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
        RNDR_LOG_ERROR("Forge: {} failed: {}", "vkCreateSwapchainKHR", VkResultToString(result));
        return VkResultToErrorCode(result);
    }
    m_swap_chain = new_swap_chain;
    m_extent = extent;

    result = vkGetSwapchainImagesKHR(m_device->GetNativeDevice(), m_swap_chain, &image_count, nullptr);
    if (result != VK_SUCCESS)
    {
        RNDR_LOG_ERROR("Forge: {} failed: {}", "vkGetSwapchainImagesKHR", VkResultToString(result));
        return VkResultToErrorCode(result);
    }

    Opal::DynamicArray<VkImage> images;
    images.Resize(image_count);
    result = vkGetSwapchainImagesKHR(m_device->GetNativeDevice(), m_swap_chain, &image_count, images.GetData());
    if (result != VK_SUCCESS)
    {
        RNDR_LOG_ERROR("Forge: {} failed: {}", "vkGetSwapchainImagesKHR", VkResultToString(result));
        return VkResultToErrorCode(result);
    }
    for (VkImage image : images)
    {
        Opal::Expected<Texture, ErrorCode> texture = Texture::Create(
            m_device, image,
            TextureDesc{.format = m_desc.pixel_format, .width = extent.width, .height = extent.height, .usage = color_texture_usage});
        if (!texture.HasValue())
        {
            return texture.GetError();
        }
        m_color_textures.PushBack(std::move(texture.GetValue()));
    }
    if (m_desc.use_depth)
    {
        Opal::Expected<Texture, ErrorCode> depth_texture = Texture::Create(m_device, {.dimension = TextureDimension::Texture2D,
                                                                                      .format = m_desc.depth_pixel_format,
                                                                                      .width = extent.width,
                                                                                      .height = extent.height,
                                                                                      .sample_count = SampleCount::Count1,
                                                                                      .usage = TextureUsageBits::DepthStencilAttachment,
                                                                                      .view_type = TextureViewType::Texture2D});
        if (!depth_texture.HasValue())
        {
            return depth_texture.GetError();
        }
        m_depth_texture = std::move(depth_texture.GetValue());
    }
    return ErrorCode::Success;
}
