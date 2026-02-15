#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"

#include "rndr/advanced/advanced-texture.hpp"
#include "rndr/advanced/graphics-context.hpp"
#include "rndr/pixel-format.hpp"
#include "rndr/types.hpp"

namespace Rndr
{
class AdvancedSemaphore;

struct AdvancedSwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    Opal::DynamicArray<VkSurfaceFormatKHR> formats;
    Opal::DynamicArray<VkPresentModeKHR> present_modes;
};

struct AdvancedSwapChainDesc
{
    bool use_depth = true;
    PixelFormat depth_pixel_format;
    PixelFormat pixel_format = PixelFormat::B8G8R8A8_SRGB;
    VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
};

class AdvancedSurface
{
public:
    AdvancedSurface() = default;
    explicit AdvancedSurface(const AdvancedGraphicsContext& context, Opal::Ref<const class GenericWindow> window);
    ~AdvancedSurface();
    AdvancedSurface(const AdvancedSurface&) = delete;
    AdvancedSurface& operator=(const AdvancedSurface&) = delete;
    AdvancedSurface(AdvancedSurface&& other) noexcept;
    AdvancedSurface& operator=(AdvancedSurface&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_surface != VK_NULL_HANDLE; }
    [[nodiscard]] VkSurfaceKHR GetNativeSurface() const { return m_surface; }
    [[nodiscard]] AdvancedSwapChainSupportDetails GetSwapChainSupportDetails(const class AdvancedPhysicalDevice& device) const;
    [[nodiscard]] const GenericWindow& GetWindow() const { return *m_window; }

private:
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    Opal::Ref<const GenericWindow> m_window;
    Opal::Ref<const AdvancedGraphicsContext> m_context;
};

class AdvancedSwapChain
{
public:
    AdvancedSwapChain() = default;
    AdvancedSwapChain(const class AdvancedDevice& device, const AdvancedSurface& surface, const AdvancedSwapChainDesc& desc = {});
    ~AdvancedSwapChain();
    AdvancedSwapChain(const AdvancedSwapChain&) = delete;
    AdvancedSwapChain& operator=(const AdvancedSwapChain&) = delete;
    AdvancedSwapChain(AdvancedSwapChain&& other) noexcept;
    AdvancedSwapChain& operator=(AdvancedSwapChain&& other) noexcept;

    void Recreate();
    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_swap_chain != VK_NULL_HANDLE; }
    [[nodiscard]] VkSwapchainKHR GetNativeSwapChain() const { return m_swap_chain; }
    [[nodiscard]] const AdvancedSwapChainDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] const VkExtent2D& GetExtent() const { return m_extent; }
    [[nodiscard]] const AdvancedTexture& GetColorImage(u32 idx) const { return m_color_textures[idx]; }
    [[nodiscard]] VkImageView GetColorImageView(u32 idx) const { return m_color_textures[idx].GetNativeImageView(); }
    [[nodiscard]] u32 GetColorImageCount() const { return static_cast<u32>(m_color_textures.GetSize()); }
    [[nodiscard]] const AdvancedTexture& GetDepthImage() const { return m_depth_texture; }
    [[nodiscard]] VkImageView GetDepthImageView() const { return m_depth_texture.GetNativeImageView(); }

    u32 AcquireImage(const Opal::Ref<AdvancedSemaphore>& semaphore);
    void Present(u32 image_index, Opal::Ref<AdvancedDeviceQueue> queue, Opal::Ref<AdvancedSemaphore> semaphore);

private:
    AdvancedSwapChainDesc m_desc;
    VkSwapchainKHR m_swap_chain = VK_NULL_HANDLE;
    VkExtent2D m_extent = {};
    Opal::Ref<const class AdvancedDevice> m_device;
    Opal::Ref<const AdvancedSurface> m_surface;
    Opal::DynamicArray<AdvancedTexture> m_color_textures;
    AdvancedTexture m_depth_texture;
};

}  // namespace Rndr