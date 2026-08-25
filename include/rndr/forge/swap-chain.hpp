#pragma once

#include "volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"

#include "rndr/error-codes.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/forge/texture.hpp"
#include "rndr/forge/types.hpp"
#include "rndr/pixel-format.hpp"
#include "rndr/types.hpp"

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
    /**
     * Give the color textures TransferSource as well, so that a presented frame can be copied back and
     * looked at. Off by default, since a frame that only presents does not need it and the extra usage can
     * cost the driver a compression path. A surface that does not offer the usage is refused rather than
     * dropping it, so a caller that asked to read the frame back is never quietly given a texture it cannot.
     */
    bool allow_readback = false;
};

/** Outcome of acquiring or presenting a swap chain texture. */
enum class SwapChainStatus : u8
{
    /** The operation succeeded and the frame can continue. */
    Success,
    /**
     * The swap chain no longer matched the surface and was recreated. The current frame has to be skipped and
     * anything the caller cached about the swap chain - its textures, their count, the extent - is stale.
     */
    OutOfDate
};

/** Index of no texture, returned by AcquireTexture when the swap chain went out of date. */
static constexpr u32 k_invalid_texture_index = 0xFFFFFFFF;

struct AcquiredTexture
{
    SwapChainStatus status = SwapChainStatus::OutOfDate;
    u32 texture_index = k_invalid_texture_index;
};

class Surface
{
public:
    Surface() = default;

    /**
     * @param context Instance the surface belongs to. Has to outlive it.
     * @param window Window to present to. Has to outlive it as well.
     * @return The surface, or whatever the failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<Surface, ErrorCode> Create(const GraphicsContext& context, const GenericWindow& window);
    ~Surface();
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    Surface(Surface&& other) noexcept;
    Surface& operator=(Surface&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_surface != VK_NULL_HANDLE; }
    [[nodiscard]] VkSurfaceKHR GetNativeSurface() const { return m_surface; }
    /**
     * What this surface offers on that device: its capabilities, its formats and its present modes.
     * @return The details, or whatever the failing query maps to.
     */
    [[nodiscard]] Opal::Expected<SwapChainSupportDetails, ErrorCode> GetSwapChainSupportDetails(const PhysicalDevice& device) const;
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
    ~SwapChain();

    /**
     * @param device Device to create the swap chain on. Has to outlive it, as does the surface.
     * @param desc Format, colour space, present mode and whether to carry a depth texture.
     * @return The swap chain, ErrorCode::FeatureNotSupported when the surface does not offer what the desc
     *         asks for, ErrorCode::InvalidArgument when the device has no graphics or present queue, or
     *         whatever the failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<SwapChain, ErrorCode> Create(const Device& device, const Surface& surface,
                                                                     const SwapChainDesc& desc = {});
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;
    SwapChain(SwapChain&& other) noexcept;
    SwapChain& operator=(SwapChain&& other) noexcept;

    /**
     * Rebuild the swap chain and its textures for the current size of the window, keeping the device, the surface
     * and the desc this swap chain was created with. Waits for the device to go idle first, so it is safe to call
     * while previous frames are still in flight.
     *
     * When the window has no client area, as happens while it is minimized, the textures are released and no new swap
     * chain is created. The object stays usable and the next AcquireTexture tries again.
     * @return ErrorCode::Success, or the first code the wait or the rebuild reported.
     */
    [[nodiscard]] ErrorCode Recreate();

    /** Release everything, including the references to the device and the surface. The object is empty afterwards. */
    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_swap_chain != VK_NULL_HANDLE; }
    [[nodiscard]] VkSwapchainKHR GetNativeSwapChain() const { return m_swap_chain; }
    [[nodiscard]] const SwapChainDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] const VkExtent2D& GetExtent() const { return m_extent; }
    [[nodiscard]] const Texture& GetColorTexture(u32 idx) const { return m_color_textures[idx]; }
    /** The same texture, mutable, since a barrier on it moves the layout it tracks. */
    [[nodiscard]] Texture& GetColorTexture(u32 idx) { return m_color_textures[idx]; }
    [[nodiscard]] u32 GetColorTextureCount() const { return static_cast<u32>(m_color_textures.GetSize()); }
    /**
     * Whether this swap chain was created with a depth texture. A swap chain made with
     * SwapChainDesc::use_depth off has none, and GetDepthTexture then hands back an empty texture. Ask this
     * rather than testing the texture, and leave RenderingDesc::depth_attachment absent when it answers false.
     */
    [[nodiscard]] bool HasDepth() const { return m_depth_texture.IsValid(); }
    [[nodiscard]] const Texture& GetDepthTexture() const { return m_depth_texture; }
    [[nodiscard]] Texture& GetDepthTexture() { return m_depth_texture; }

    /**
     * Acquire the next texture to render into. The semaphore is signaled once it is ready to be written to.
     *
     * On SwapChainStatus::OutOfDate the swap chain has been recreated, no texture was acquired and the semaphore was
     * not signaled, so the caller has to skip the frame. Because nothing was submitted for that frame, the caller
     * must not reset its per-frame fence before this call returns Success, otherwise the next wait on that fence
     * never completes.
     */
    [[nodiscard]] Opal::Expected<AcquiredTexture, ErrorCode> AcquireTexture(const Semaphore& semaphore);

    /**
     * Whether a texture is acquired right now, which is true between an AcquireTexture that returned Success
     * and the Present that hands it back.
     */
    [[nodiscard]] bool HasAcquiredTexture() const { return m_current_texture_index != k_invalid_texture_index; }

    /** Index the last AcquireTexture handed out, or k_invalid_texture_index when none is acquired. */
    [[nodiscard]] u32 GetCurrentTextureIndex() const { return m_current_texture_index; }

    /**
     * The acquired texture, which is the one to render into this frame. Throws when none is acquired, since
     * there is no texture to hand back rather than a wrong one.
     */
    [[nodiscard]] Opal::Expected<const Texture&, ErrorCode> GetCurrentColorTexture() const;
    [[nodiscard]] Opal::Expected<Texture&, ErrorCode> GetCurrentColorTexture();

    /**
     * Present the acquired texture once the given semaphore is signaled. Which texture that is the swap chain
     * remembers from AcquireTexture, so nothing has to be threaded through the frame; presenting with none
     * acquired is refused. The texture is no longer acquired afterwards, whatever the outcome.
     *
     * Returns SwapChainStatus::OutOfDate when the swap chain stopped matching the surface, in which case it has
     * already been recreated and the caller has to refresh anything it cached about it.
     */
    [[nodiscard]] Opal::Expected<SwapChainStatus, ErrorCode> Present(DeviceQueue& queue, const Semaphore& semaphore);

private:
    /** Destroy the color textures, their views and the depth texture, leaving the swap chain handle alone. */
    void DestroyTextures();

    /** Destroy the swap chain handle, leaving the textures alone. Does nothing when there is no swap chain. */
    void DestroySwapChain();

    SwapChainDesc m_desc;
    VkSwapchainKHR m_swap_chain = VK_NULL_HANDLE;
    VkExtent2D m_extent = {};
    Opal::Ref<const Device> m_device;
    Opal::Ref<const Surface> m_surface;
    Opal::DynamicArray<Texture> m_color_textures;
    Texture m_depth_texture;
    /** The texture AcquireTexture handed out, until Present gives it back. */
    u32 m_current_texture_index = k_invalid_texture_index;
};

}  // namespace Rndr::Forge