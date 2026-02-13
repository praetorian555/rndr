#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"

#include "rndr/pixel-format.hpp"
#include "rndr/types.hpp"
#include "rndr/advanced/graphics-context.hpp"

namespace Rndr
{

struct AdvancedSwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    Opal::DynamicArray<VkSurfaceFormatKHR> formats;
    Opal::DynamicArray<VkPresentModeKHR> present_modes;
};

struct AdvancedSwapChainDesc
{
    PixelFormat pixel_format = PixelFormat::B8G8R8A8_SRGB;
    VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    u32 width = 0;
    u32 height = 0;
};

class AdvancedSurface
{
public:
    AdvancedSurface() = default;
    explicit AdvancedSurface(const AdvancedGraphicsContext& context, NativeWindowHandle window_handle);
    ~AdvancedSurface();
    AdvancedSurface(const AdvancedSurface&) = delete;
    AdvancedSurface& operator=(const AdvancedSurface&) = delete;
    AdvancedSurface(AdvancedSurface&& other) noexcept;
    AdvancedSurface& operator=(AdvancedSurface&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_surface != VK_NULL_HANDLE; }
    [[nodiscard]] VkSurfaceKHR GetNativeSurface() const { return m_surface; }
    [[nodiscard]] AdvancedSwapChainSupportDetails GetSwapChainSupportDetails(const class AdvancedPhysicalDevice& device) const;

private:
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
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

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_swap_chain != VK_NULL_HANDLE; }
    [[nodiscard]] VkSwapchainKHR GetNativeSwapChain() const { return m_swap_chain; }
    [[nodiscard]] const AdvancedSwapChainDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] const VkExtent2D& GetExtent() const { return m_extent; }
    [[nodiscard]] const Opal::DynamicArray<VkImageView>& GetImageViews() const { return m_image_views; }

private:
    AdvancedSwapChainDesc m_desc;
    VkSwapchainKHR m_swap_chain = VK_NULL_HANDLE;
    VkExtent2D m_extent = {};
    Opal::DynamicArray<VkImage> m_images;
    Opal::DynamicArray<VkImageView> m_image_views;
    Opal::Ref<const class AdvancedDevice> m_device;
};

}  // namespace Rndr