#include "rndr/forge/swap-chain.hpp"

#include "opal/math-base.h"
#include "opal/time.h"

#if RNDR_WINDOWS
#include "rndr/platform/windows-header.hpp"
#elif RNDR_LINUX
#include <xcb/xcb.h>
#endif

#include "opal/container/in-place-array.h"
#include "opal/math/transform.h"

#include "rndr/forge/device.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/log.hpp"
#include "rndr/pixel-format.hpp"

namespace
{
/** How many presents the presentation engine keeps a report for until it is taken. */
constexpr Rndr::u32 k_timing_queue_size = 16;
}  // namespace

namespace
{
struct SurfaceGeometry
{
    VkExtent2D extent = {};
    Rndr::Forge::SurfaceRotation rotation = Rndr::Forge::SurfaceRotation::None;
};

SurfaceGeometry SelectGeometry(const VkSurfaceCapabilitiesKHR& capabilities, const Rndr::GenericWindow& window);
}  // namespace

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

#if RNDR_WINDOWS
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
#elif RNDR_ANDROID
    // The activity has no window while it is in the background. A surface over nothing is not something to
    // retry from in here; the application builds it again when the window comes back.
    if (window.GetNativeHandle() == nullptr)
    {
        RNDR_LOG_ERROR("Forge: the activity has no native window to create a surface over, it is in the background");
        return Result(ErrorCode::PlatformError);
    }
    Surface surface;
    surface.m_window = window;
    VkAndroidSurfaceCreateInfoKHR surface_create_info{};
    surface_create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surface_create_info.window = reinterpret_cast<ANativeWindow*>(window.GetNativeHandle());
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateAndroidSurfaceKHR(context.GetInstance(), &surface_create_info, nullptr, &surface.m_surface),
                                 "vkCreateAndroidSurfaceKHR", Result);
    surface.m_context = &context;
    return Result(std::move(surface));
#elif RNDR_LINUX
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
      m_rotation(other.m_rotation),
      m_device(std::move(other.m_device)),
      m_surface(std::move(other.m_surface)),
      m_color_textures(std::move(other.m_color_textures)),
      m_depth_texture(std::move(other.m_depth_texture)),
      m_current_texture_index(other.m_current_texture_index),
      m_timing(std::move(other.m_timing))
{
    other.m_timing = {};
    other.m_swap_chain = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_surface = nullptr;
    other.m_color_textures.Clear();
    other.m_desc = {};
    other.m_extent = {};
    other.m_rotation = SurfaceRotation::None;
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
    m_rotation = other.m_rotation;
    m_current_texture_index = other.m_current_texture_index;
    m_timing = std::move(other.m_timing);

    other.m_timing = {};
    other.m_swap_chain = VK_NULL_HANDLE;
    other.m_device = nullptr;
    other.m_surface = nullptr;
    other.m_color_textures.Clear();
    other.m_desc = {};
    other.m_extent = {};
    other.m_rotation = SurfaceRotation::None;
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
    // Present timing is the swap chain's; the next one sets it up again.
    m_timing.is_on = false;
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
    m_rotation = SurfaceRotation::None;
    m_timing = {};
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
#if RNDR_ANDROID
    // Android reports a turn of the screen as a suboptimal present, but not the size that goes with it, and for a moment
    // the surface can report the new rotation with the old size. A swap chain made in that moment is the wrong shape and
    // nothing says so again, so the surface is asked every frame and a settled change makes the swap chain again.
    VkSurfaceCapabilitiesKHR capabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device->GetPhysicalDevice().GetNativePhysicalDevice(), m_surface->GetNativeSurface(),
                                                  &capabilities) == VK_SUCCESS)
    {
        const SurfaceGeometry geometry = SelectGeometry(capabilities, m_surface->GetWindow());
        if (geometry.extent.width != m_extent.width || geometry.extent.height != m_extent.height || geometry.rotation != m_rotation)
        {
            RNDR_FORGE_CHECK_EXPECTED(Recreate(), Result);
            return Result(AcquiredTexture{});
        }
    }
#endif

    u32 texture_index = k_invalid_texture_index;
    const VkResult result = vkAcquireNextImageKHR(m_device->GetNativeDevice(), m_swap_chain, m_desc.acquire_timeout,
                                                  semaphore.GetNativeSemaphore(), VK_NULL_HANDLE, &texture_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RNDR_FORGE_CHECK_EXPECTED(Recreate(), Result);
        return Result(AcquiredTexture{});
    }
    // Both are the timeout running out: NOT_READY when it was zero, TIMEOUT otherwise. Neither acquired
    // anything or signals the semaphore, and neither is a failure - the timeout exists to be run out.
    if (result == VK_TIMEOUT || result == VK_NOT_READY)
    {
        return Result(AcquiredTexture{.status = SwapChainStatus::NotReady});
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
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &wait_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swap_chain,
        .pImageIndices = &texture_index,
    };
    // A timed present asks when its first pixel goes out. What earlier ones reported is taken first, so the queue the
    // presentation engine reports into has room.
    const uint64_t present_id = m_timing.next_present_id;
    const VkPresentId2KHR present_id_info{.sType = VK_STRUCTURE_TYPE_PRESENT_ID_2_KHR, .swapchainCount = 1, .pPresentIds = &present_id};
    const VkPresentTimingInfoEXT timing_info{.sType = VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT,
                                             .timeDomainId = m_timing.time_domain_id,
                                             .presentStageQueries = m_timing.stage};
    const VkPresentTimingsInfoEXT timings_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_TIMINGS_INFO_EXT, .pNext = &present_id_info, .swapchainCount = 1, .pTimingInfos = &timing_info};
    if (m_timing.is_on)
    {
        RNDR_FORGE_CHECK_EXPECTED(CollectPresentTimes(), Result);
        // A present that is never shown, as in a window kept hidden, is never reported and keeps its place, so one
        // that would fill the queue goes untimed.
        if (m_timing.waiting < k_timing_queue_size)
        {
            present_info.pNext = &timings_info;
            ++m_timing.waiting;
        }
        else
        {
            present_info.pNext = &present_id_info;
        }
        ++m_timing.next_present_id;
    }
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
/** The stage nearest the screen that the surface reports present times for, or 0 when it reports none or the device cannot ask. */
VkPresentStageFlagsEXT SelectPresentStage(const Rndr::Forge::Device& device, const Rndr::Forge::Surface& surface)
{
    if (!device.GetEnabledFeatures().present_timing)
    {
        return 0;
    }
    VkPresentTimingSurfaceCapabilitiesEXT timing{.sType = VK_STRUCTURE_TYPE_PRESENT_TIMING_SURFACE_CAPABILITIES_EXT};
    VkSurfaceCapabilitiesPresentId2KHR present_id2{.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_ID_2_KHR, .pNext = &timing};
    VkSurfaceCapabilities2KHR capabilities{.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, .pNext = &present_id2};
    const VkPhysicalDeviceSurfaceInfo2KHR surface_info{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
                                                       .surface = surface.GetNativeSurface()};
    if (vkGetPhysicalDeviceSurfaceCapabilities2KHR(device.GetPhysicalDevice().GetNativePhysicalDevice(), &surface_info, &capabilities) !=
            VK_SUCCESS ||
        timing.presentTimingSupported == VK_FALSE || present_id2.presentId2Supported == VK_FALSE)
    {
        return 0;
    }
    if ((timing.presentStageQueries & VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT) != 0)
    {
        return VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT;
    }
    if ((timing.presentStageQueries & VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT) != 0)
    {
        return VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT;
    }
    return 0;
}

/** The time domain Opal::GetSeconds reads. */
#if RNDR_WINDOWS
constexpr VkTimeDomainKHR k_host_time_domain = VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR;
#else
constexpr VkTimeDomainKHR k_host_time_domain = VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR;
#endif

/** What one unit of a time domain is in seconds, for a domain Opal::GetSeconds reads; 0 for any other. */
Rndr::f64 GetSecondsPerTick(VkTimeDomainKHR time_domain)
{
#if RNDR_WINDOWS
    if (time_domain == VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR)
    {
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart > 0 ? 1.0 / static_cast<Rndr::f64>(frequency.QuadPart) : 0.0;
    }
#else
    if (time_domain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR)
    {
        return 1e-9;
    }
#endif
    return 0.0;
}
}  // namespace

Rndr::ErrorCode Rndr::Forge::SwapChain::TakePresentTimes(Opal::DynamicArray<PresentTime>& out_times)
{
    if (!m_timing.is_on)
    {
        return ErrorCode::Success;
    }
    RNDR_FORGE_CHECK(CollectPresentTimes());
    for (const PresentTime& time : m_timing.reported)
    {
        out_times.PushBack(time);
    }
    m_timing.reported.Clear();
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::SwapChain::SetUpPresentTiming()
{
    m_timing.waiting = 0;
    RNDR_FORGE_VK_CHECK(vkSetSwapchainPresentTimingQueueSizeEXT(m_device->GetNativeDevice(), m_swap_chain, k_timing_queue_size),
                        "vkSetSwapchainPresentTimingQueueSizeEXT");
    return SelectTimeDomain();
}

Rndr::ErrorCode Rndr::Forge::SwapChain::SelectTimeDomain()
{
    // A few at most: the host's clocks and the swap chain's own; the first that many are enough to choose from.
    constexpr u32 k_max_domains = 8;
    VkTimeDomainKHR domains[k_max_domains] = {};
    uint64_t ids[k_max_domains] = {};
    VkSwapchainTimeDomainPropertiesEXT properties{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_TIME_DOMAIN_PROPERTIES_EXT,
                                                  .timeDomainCount = k_max_domains,
                                                  .pTimeDomains = domains,
                                                  .pTimeDomainIds = ids};
    uint64_t counter = 0;
    const VkResult result = vkGetSwapchainTimeDomainPropertiesEXT(m_device->GetNativeDevice(), m_swap_chain, &properties, &counter);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE)
    {
        RNDR_LOG_ERROR("Forge: {} failed: {}", "vkGetSwapchainTimeDomainPropertiesEXT", VkResultToString(result));
        return VkResultToErrorCode(result);
    }
    m_timing.is_on = false;
    m_timing.time_domains_counter = counter;
    properties.timeDomainCount = Opal::Min(properties.timeDomainCount, k_max_domains);
    for (u32 i = 0; i < properties.timeDomainCount && !m_timing.is_on; ++i)
    {
        const f64 seconds_per_tick = GetSecondsPerTick(domains[i]);
        m_timing.is_on = seconds_per_tick > 0.0;
        m_timing.is_calibrated = false;
        m_timing.time_domain = domains[i];
        m_timing.time_domain_id = ids[i];
        m_timing.seconds_per_tick = seconds_per_tick;
    }
    // A domain of the swap chain's own, in nanoseconds, read against the host's clock.
    for (u32 i = 0; i < properties.timeDomainCount && !m_timing.is_on; ++i)
    {
        if (domains[i] == VK_TIME_DOMAIN_SWAPCHAIN_LOCAL_EXT || domains[i] == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT)
        {
            m_timing.is_calibrated = true;
            m_timing.time_domain = domains[i];
            m_timing.time_domain_id = ids[i];
            m_timing.is_on = Calibrate();
        }
    }
    if (!m_timing.is_on)
    {
        RNDR_LOG_WARNING("Forge: the swap chain reports present times on no clock the host reads, so presents are not timed");
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::SwapChain::CollectPresentTimes()
{
    constexpr u32 k_batch = 16;
    // What is kept for a caller that does not take it, past which the oldest go.
    constexpr u64 k_max_reported = 256;
    while (true)
    {
        VkPresentStageTimeEXT stages[k_batch] = {};
        VkPastPresentationTimingEXT timings[k_batch] = {};
        for (u32 i = 0; i < k_batch; ++i)
        {
            stages[i].stage = m_timing.stage;
            timings[i] = {.sType = VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_EXT, .presentStageCount = 1, .pPresentStages = &stages[i]};
        }
        const VkPastPresentationTimingInfoEXT info{.sType = VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_INFO_EXT, .swapchain = m_swap_chain};
        VkPastPresentationTimingPropertiesEXT properties{.sType = VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_PROPERTIES_EXT,
                                                        .presentationTimingCount = k_batch,
                                                        .pPresentationTimings = timings};
        const VkResult result = vkGetPastPresentationTimingEXT(m_device->GetNativeDevice(), &info, &properties);
        if (result != VK_SUCCESS && result != VK_INCOMPLETE)
        {
            RNDR_LOG_ERROR("Forge: {} failed: {}", "vkGetPastPresentationTimingEXT", VkResultToString(result));
            return VkResultToErrorCode(result);
        }
        for (u32 i = 0; i < properties.presentationTimingCount && i < k_batch; ++i)
        {
            const VkPastPresentationTimingEXT& timing = timings[i];
            m_timing.waiting -= m_timing.waiting > 0 ? 1 : 0;
            if (timing.reportComplete == VK_FALSE || timing.timeDomainId != m_timing.time_domain_id || timing.presentStageCount == 0 ||
                stages[i].time == 0)
            {
                continue;
            }
            if (m_timing.reported.GetSize() >= k_max_reported)
            {
                m_timing.reported.Erase(m_timing.reported.begin());
            }
            m_timing.reported.PushBack({.present_id = timing.presentId, .first_pixel_time = ToHostSeconds(stages[i].time)});
        }
        // The swap chain's time domains changed: what comes from here is read against the new choice.
        if (properties.timeDomainsCounter != m_timing.time_domains_counter)
        {
            RNDR_FORGE_CHECK(SelectTimeDomain());
            if (!m_timing.is_on)
            {
                return ErrorCode::Success;
            }
        }
        if (result != VK_INCOMPLETE)
        {
            break;
        }
    }
    // The two clocks drift apart, so the moment they are read together is taken again once a second.
    constexpr f64 k_calibration_interval = 1.0;
    if (m_timing.is_calibrated && Opal::GetSeconds() - m_timing.calibrated_seconds > k_calibration_interval)
    {
        m_timing.is_on = Calibrate();
    }
    return ErrorCode::Success;
}

bool Rndr::Forge::SwapChain::Calibrate()
{
    const VkSwapchainCalibratedTimestampInfoEXT swap_chain_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CALIBRATED_TIMESTAMP_INFO_EXT,
        .swapchain = m_swap_chain,
        .presentStage = m_timing.time_domain == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT ? m_timing.stage : 0,
        .timeDomainId = m_timing.time_domain_id};
    const VkCalibratedTimestampInfoKHR infos[2] = {
        {.sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR, .pNext = &swap_chain_info, .timeDomain = m_timing.time_domain},
        {.sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR, .timeDomain = k_host_time_domain}};
    uint64_t timestamps[2] = {};
    uint64_t max_deviation = 0;
    const VkResult result = vkGetCalibratedTimestampsKHR(m_device->GetNativeDevice(), 2, infos, timestamps, &max_deviation);
    if (result != VK_SUCCESS)
    {
        RNDR_LOG_WARNING("Forge: the swap chain's clock cannot be read against the host's, so presents are not timed: {}",
                         VkResultToString(result));
        return false;
    }
    m_timing.calibrated_time = timestamps[0];
    m_timing.calibrated_seconds = static_cast<f64>(timestamps[1]) * GetSecondsPerTick(k_host_time_domain);
    return true;
}

Rndr::f64 Rndr::Forge::SwapChain::ToHostSeconds(u64 time) const
{
    if (!m_timing.is_calibrated)
    {
        return static_cast<f64>(time) * m_timing.seconds_per_tick;
    }
    const i64 since = static_cast<i64>(time - m_timing.calibrated_time);
    return m_timing.calibrated_seconds + static_cast<f64>(since) * 1e-9;
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

/**
 * The rotation the surface is shown at, when it is one a swap chain can be created pre-rotated by. A mirrored or an
 * inherited transform is left to the presentation engine, which is where it was before pre-rotation existed.
 */
Rndr::Forge::SurfaceRotation SelectRotation(const VkSurfaceCapabilitiesKHR& capabilities)
{
    const VkSurfaceTransformFlagBitsKHR current = capabilities.currentTransform;
    if ((capabilities.supportedTransforms & current) == 0)
    {
        return Rndr::Forge::SurfaceRotation::None;
    }
    switch (current)
    {
        case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
            return Rndr::Forge::SurfaceRotation::Clockwise90;
        case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
            return Rndr::Forge::SurfaceRotation::Clockwise180;
        case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
            return Rndr::Forge::SurfaceRotation::Clockwise270;
        default:
            return Rndr::Forge::SurfaceRotation::None;
    }
}

VkSurfaceTransformFlagBitsKHR ToVkSurfaceTransform(Rndr::Forge::SurfaceRotation rotation)
{
    switch (rotation)
    {
        case Rndr::Forge::SurfaceRotation::Clockwise90:
            return VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
        case Rndr::Forge::SurfaceRotation::Clockwise180:
            return VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR;
        case Rndr::Forge::SurfaceRotation::Clockwise270:
            return VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR;
        default:
            return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
}

/**
 * The extent and rotation a swap chain for this surface is created with. The extent is in the display's natural
 * orientation: created that way and turned by the presentation engine's own transform, which costs nothing, rather than
 * in the window's and turned by a composition pass every frame. Android reports a surface that is not pre-rotated as
 * suboptimal, so without this every present on a turned phone recreates it. A zero extent means no swap chain.
 */
SurfaceGeometry SelectGeometry(const VkSurfaceCapabilitiesKHR& capabilities, const Rndr::GenericWindow& window)
{
    SurfaceGeometry geometry{.extent = SelectExtent(capabilities, window)};
    if (geometry.extent.width == 0 || geometry.extent.height == 0)
    {
        return geometry;
    }
    geometry.rotation = SelectRotation(capabilities);
    if (geometry.rotation == Rndr::Forge::SurfaceRotation::Clockwise90 || geometry.rotation == Rndr::Forge::SurfaceRotation::Clockwise270)
    {
        geometry.extent = {.width = geometry.extent.height, .height = geometry.extent.width};
    }
    return geometry;
}
}  // namespace

Rndr::Matrix4x4f Rndr::Forge::SwapChain::GetPreRotation() const
{
    // Counter-clockwise in clip space by the angle the display is turned clockwise, written out rather than built
    // from a sine and a cosine so that the zeros are zeros.
    Matrix4x4f result = Opal::Identity<f32>();
    switch (m_rotation)
    {
        case SurfaceRotation::Clockwise90:
            result.elements[0][0] = 0.0f;
            result.elements[0][1] = -1.0f;
            result.elements[1][0] = 1.0f;
            result.elements[1][1] = 0.0f;
            break;
        case SurfaceRotation::Clockwise180:
            result.elements[0][0] = -1.0f;
            result.elements[1][1] = -1.0f;
            break;
        case SurfaceRotation::Clockwise270:
            result.elements[0][0] = 0.0f;
            result.elements[0][1] = 1.0f;
            result.elements[1][0] = -1.0f;
            result.elements[1][1] = 0.0f;
            break;
        default:
            break;
    }
    return result;
}

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

    const SurfaceGeometry geometry = SelectGeometry(swap_chain_support.capabilities, m_surface->GetWindow());
    const VkExtent2D extent = geometry.extent;
    const SurfaceRotation rotation = geometry.rotation;
    if (extent.width == 0 || extent.height == 0)
    {
        // The window has no client area, so there is nothing to present to. Release the swap chain and let the next
        // AcquireTexture try again once the window is back.
        DestroySwapChain();
        m_extent = {};
        m_rotation = SurfaceRotation::None;
        return ErrorCode::Success;
    }
    RNDR_LOG_INFO("Swap chain extent: ({}, {}), rotated {} degrees", extent.width, extent.height, static_cast<i32>(rotation) * 90);

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
    Opal::Expected<bool, ErrorCode> can_present = m_device->CanPresentTo(*m_surface);
    if (!can_present.HasValue())
    {
        return can_present.GetError();
    }
    if (!can_present.GetValue())
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

    create_info.preTransform = ToVkSurfaceTransform(rotation);
    // Opaque where the surface offers it. Android's surfaces offer only Inherit, which leaves it to the window, and
    // that window is opaque unless the app asks otherwise.
    const VkCompositeAlphaFlagsKHR supported_composite_alpha = swap_chain_support.capabilities.supportedCompositeAlpha;
    if ((supported_composite_alpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0)
    {
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    else if ((supported_composite_alpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) != 0)
    {
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }
    else
    {
        RNDR_LOG_ERROR("Forge: this surface offers neither opaque nor inherited composite alpha");
        return ErrorCode::FeatureNotSupported;
    }
    create_info.presentMode = present_mode;
    // If set to VK_TRUE it means that we don't care about the color of the pixels if they are occluded by other window.
    create_info.clipped = VK_TRUE;
    // Letting the driver reuse the resources of the swap chain we are replacing.
    create_info.oldSwapchain = m_swap_chain;
    m_timing.is_on = false;
    m_timing.stage = SelectPresentStage(*m_device, *m_surface);
    if (m_timing.stage != 0)
    {
        create_info.flags |= VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT | VK_SWAPCHAIN_CREATE_PRESENT_ID_2_BIT_KHR;
    }

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
    m_rotation = rotation;
    m_timing.reported.Clear();
    if (m_timing.stage != 0)
    {
        RNDR_FORGE_CHECK(SetUpPresentTiming());
    }

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
