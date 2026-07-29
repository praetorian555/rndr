#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"

#include "rndr/forge/texture.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/pixel-format.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"

namespace Rndr
{
class GenericWindow;
}

namespace Rndr::Forge
{

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    Opal::DynamicArray<VkSurfaceFormatKHR> formats;
    Opal::DynamicArray<VkPresentModeKHR> present_modes;
};

struct SwapChainDesc
{
    bool use_depth = true;
    PixelFormat depth_pixel_format;
    PixelFormat pixel_format = PixelFormat::B8G8R8A8_SRGB;
    VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
};

class Surface
{
public:
    Surface() = default;
    explicit Surface(const GraphicsContext& context, Opal::Ref<const GenericWindow> window);
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

    void Recreate();
    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_swap_chain != VK_NULL_HANDLE; }
    [[nodiscard]] VkSwapchainKHR GetNativeSwapChain() const { return m_swap_chain; }
    [[nodiscard]] const SwapChainDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] const VkExtent2D& GetExtent() const { return m_extent; }
    [[nodiscard]] const Texture& GetColorImage(u32 idx) const { return m_color_textures[idx]; }
    [[nodiscard]] VkImageView GetColorImageView(u32 idx) const { return m_color_textures[idx].GetNativeImageView(); }
    [[nodiscard]] u32 GetColorImageCount() const { return static_cast<u32>(m_color_textures.GetSize()); }
    [[nodiscard]] const Texture& GetDepthImage() const { return m_depth_texture; }
    [[nodiscard]] VkImageView GetDepthImageView() const { return m_depth_texture.GetNativeImageView(); }

    u32 AcquireImage(const Opal::Ref<Semaphore>& semaphore);
    void Present(u32 image_index, Opal::Ref<DeviceQueue> queue, Opal::Ref<Semaphore> semaphore);

private:
    SwapChainDesc m_desc;
    VkSwapchainKHR m_swap_chain = VK_NULL_HANDLE;
    VkExtent2D m_extent = {};
    Opal::Ref<const Device> m_device;
    Opal::Ref<const Surface> m_surface;
    Opal::DynamicArray<Texture> m_color_textures;
    Texture m_depth_texture;
};

}  // namespace Rndr::Forge