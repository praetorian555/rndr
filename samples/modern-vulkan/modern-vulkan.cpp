#include "opal/container/dynamic-array.h"
#include "rndr/application.hpp"

#include "vma/vk_mem_alloc.h"
#include "volk/volk.h"

#include "rndr/types.hpp"
#include "rndr/advanced/graphics-context.hpp"
#include "rndr/advanced/physical-device.hpp"
#include "rndr/advanced/device.hpp"
#include "rndr/advanced/swap-chain.hpp"

using i32 = Rndr::i32;
using u32 = Rndr::u32;
using f32 = Rndr::f32;

#define VK_CHECK(expr)                                         \
    do                                                         \
    {                                                          \
        [[maybe_unused]] const VkResult result_ = expr;        \
        if (result_ != VK_SUCCESS)                             \
            throw Opal::Exception("Vulkan operation failed!"); \
    } while (0)

Opal::DynamicArray<const char*> GetRequiredInstanceExtensions()
{
    Opal::DynamicArray<const char*> required_extension_names;

    // We need this extension if we want to display the image to the display
    required_extension_names.PushBack(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(OPAL_PLATFORM_WINDOWS)
    // We need it if we want to display the image to the display on Windows
    required_extension_names.PushBack(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
    return required_extension_names;
}

VmaAllocator vma_allocator;

int main()
{
    auto rndr_app = Rndr::Application::Create();
    Rndr::GenericWindow* window = rndr_app->CreateGenericWindow();
    Rndr::AdvancedGraphicsContext graphics_context{{.collect_debug_messages = true}};
    Rndr::AdvancedSurface surface(graphics_context, window->GetNativeHandle());

    auto physical_devices = graphics_context.EnumeratePhysicalDevices();
    Rndr::AdvancedDevice device(std::move(physical_devices[0]), {.surface = Opal::Ref{surface}});
    Rndr::AdvancedDeviceQueue m_graphics_queue(device, Rndr::AdvancedDeviceQueueFamilyFlags::Graphics);
    Rndr::AdvancedDeviceQueue m_present_queue(device, Rndr::AdvancedDeviceQueueFamilyFlags::Present);

    Rndr::AdvancedSwapChain swap_chain(device, surface, {});

    // Setup GPU allocator
    const VmaVulkanFunctions vk_functions {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkCreateImage = vkCreateImage,
    };
    const VmaAllocatorCreateInfo vma_alloc_create_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = device.GetNativePhysicalDevice(),
        .device = device.GetNativeDevice(),
        .pVulkanFunctions = &vk_functions,
        .instance = graphics_context.GetInstance(),
    };
    VK_CHECK(vmaCreateAllocator(&vma_alloc_create_info, &vma_allocator));

}