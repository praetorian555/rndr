#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"

#include "rndr/forge/texture.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/pixel-format.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"

namespace Rndr
{
class GenericWindow;
}

namespace Rndr::Forge
{

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities = {};
    Opal::DynamicArray<VkSurfaceFormatKHR> formats;
    Opal::DynamicArray<VkPresentModeKHR> present_modes;
};

struct SwapChainDesc
{
    bool use_depth = true;
    PixelFormat depth_pixel_format = PixelFormat::D32_SFLOAT;
    PixelFormat pixel_format = PixelFormat::B8G8R8A8_SRGB;
    ColorSpace color_space = ColorSpace::SrgbNonlinear;
    PresentMode present_mode = PresentMode::Fifo;
};

/** Outcome of acquiring or presenting a swap chain image. */
enum class SwapChainStatus : u8
{
    /** The operation succeeded and the frame can continue. */
    Success,
    /**
     * The swap chain no longer matched the surface and was recreated. The current frame has to be skipped and
     * anything the caller cached about the swap chain - image views, image count, extent - is stale.
     */
    OutOfDate
};

/** Index of no image, returned by AcquireImage when the swap chain went out of date. */
static constexpr u32 k_invalid_image_index = 0xFFFFFFFF;

struct AcquiredImage
{
    SwapChainStatus status = SwapChainStatus::OutOfDate;
    u32 image_index = k_invalid_image_index;
};

class Surface
{
public:
    Surface() = default;
    explicit Surface(const GraphicsContext& context, const GenericWindow& window);
    ~Surface();
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    Surface(Surface&& other) noexcept;
    Surface& operator=(Surface&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_surface != VK_NULL_HANDLE; }
    [[nodiscard]] VkSurfaceKHR GetNativeSurface() const { return m_surface; }
    [[nodiscard]] SwapChainSupportDetails GetSwapChainSupportDetails(const PhysicalDevice& device) const;
    [[nodiscard]] const GenericWindow& GetWindow() const { return *m_window; }

private:
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    Opal::Ref<const GenericWindow> m_window;
    Opal::Ref<const GraphicsContext> m_context;
};

class SwapChain
{
public:
    SwapChain() = default;
    SwapChain(const Device& device, const Surface& surface, const SwapChainDesc& desc = {});
    ~SwapChain();
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;
    SwapChain(SwapChain&& other) noexcept;
    SwapChain& operator=(SwapChain&& other) noexcept;

    /**
     * Rebuild the swap chain and its images for the current size of the window, keeping the device, the surface
     * and the desc this swap chain was created with. Waits for the device to go idle first, so it is safe to call
     * while previous frames are still in flight.
     *
     * When the window has no client area, as happens while it is minimized, the images are released and no new swap
     * chain is created. The object stays usable and the next AcquireImage tries again.
     */
    void Recreate();

    /** Release everything, including the references to the device and the surface. The object is empty afterwards. */
    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_swap_chain != VK_NULL_HANDLE; }
    [[nodiscard]] VkSwapchainKHR GetNativeSwapChain() const { return m_swap_chain; }
    [[nodiscard]] const SwapChainDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] const VkExtent2D& GetExtent() const { return m_extent; }
    [[nodiscard]] const Texture& GetColorImage(u32 idx) const { return m_color_textures[idx]; }
    [[nodiscard]] VkImageView GetColorImageView(u32 idx) const { return m_color_textures[idx].GetNativeImageView(); }
    [[nodiscard]] u32 GetColorImageCount() const { return static_cast<u32>(m_color_textures.GetSize()); }
    /**
     * Whether this swap chain was created with a depth image. A swap chain made with
     * SwapChainDesc::use_depth off has none, and GetDepthImage and GetDepthImageView then hand back an empty
     * texture and a null view. Ask this rather than comparing the view against VK_NULL_HANDLE, and leave
     * RenderingDesc::depth_attachment absent when it answers false.
     */
    [[nodiscard]] bool HasDepth() const { return m_depth_texture.IsValid(); }
    [[nodiscard]] const Texture& GetDepthImage() const { return m_depth_texture; }
    [[nodiscard]] VkImageView GetDepthImageView() const { return m_depth_texture.GetNativeImageView(); }

    /**
     * Acquire the next image to render into. The semaphore is signaled once the image is ready to be written to.
     *
     * On SwapChainStatus::OutOfDate the swap chain has been recreated, no image was acquired and the semaphore was
     * not signaled, so the caller has to skip the frame. Because nothing was submitted for that frame, the caller
     * must not reset its per-frame fence before this call returns Success, otherwise the next wait on that fence
     * never completes.
     */
    AcquiredImage AcquireImage(const Semaphore& semaphore);

    /**
     * Whether an image is acquired right now, which is true between an AcquireImage that returned Success
     * and the Present that hands it back.
     */
    [[nodiscard]] bool HasAcquiredImage() const { return m_current_image_index != k_invalid_image_index; }

    /** Index the last AcquireImage handed out, or k_invalid_image_index when none is acquired. */
    [[nodiscard]] u32 GetCurrentImageIndex() const { return m_current_image_index; }

    /**
     * The acquired image, which is the one to render into this frame. Throws when none is acquired, since
     * there is no image to hand back rather than a wrong one.
     */
    [[nodiscard]] const Texture& GetCurrentColorImage() const;
    [[nodiscard]] VkImageView GetCurrentColorImageView() const;

    /**
     * Present the acquired image once the given semaphore is signaled. Which image that is the swap chain
     * remembers from AcquireImage, so nothing has to be threaded through the frame; presenting with none
     * acquired throws. The image is no longer acquired afterwards, whatever the outcome.
     *
     * Returns SwapChainStatus::OutOfDate when the swap chain stopped matching the surface, in which case it has
     * already been recreated and the caller has to refresh anything it cached about it.
     */
    SwapChainStatus Present(DeviceQueue& queue, const Semaphore& semaphore);

private:
    /** Destroy the color images, their views and the depth image, leaving the swap chain handle alone. */
    void DestroyImages();

    /** Destroy the swap chain handle, leaving the images alone. Does nothing when there is no swap chain. */
    void DestroySwapChain();

    SwapChainDesc m_desc;
    VkSwapchainKHR m_swap_chain = VK_NULL_HANDLE;
    VkExtent2D m_extent = {};
    Opal::Ref<const Device> m_device;
    Opal::Ref<const Surface> m_surface;
    Opal::DynamicArray<Texture> m_color_textures;
    Texture m_depth_texture;
    /** The image AcquireImage handed out, until Present gives it back. */
    u32 m_current_image_index = k_invalid_image_index;
};

}  // namespace Rndr::Forge