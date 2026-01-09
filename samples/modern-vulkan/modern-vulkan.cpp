#include "../../build/opengl-msvc-opt-debug/_deps/opal-src/include/opal/paths.h"
#include "opal/container/dynamic-array.h"
#include "rndr/application.hpp"
#include "rndr/file.hpp"

#include "vma/vk_mem_alloc.h"
#include "volk/volk.h"

#include "rndr/advanced/advanced-texture.hpp"
#include "rndr/advanced/device.hpp"
#include "rndr/advanced/graphics-context.hpp"
#include "rndr/advanced/physical-device.hpp"
#include "rndr/advanced/swap-chain.hpp"
#include "rndr/types.hpp"

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
    Rndr::AdvancedDevice device(std::move(physical_devices[0]), graphics_context, {.surface = Opal::Ref{surface}});
    Rndr::AdvancedDeviceQueue m_graphics_queue(device, Rndr::AdvancedDeviceQueueFamilyFlags::Graphics);
    Rndr::AdvancedDeviceQueue m_present_queue(device, Rndr::AdvancedDeviceQueueFamilyFlags::Present);

    Rndr::AdvancedSwapChain swap_chain(device, surface, {});

    const VkExtent2D extent = swap_chain.GetExtent();
    Rndr::AdvancedTexture texture(device, {.image_type = VK_IMAGE_TYPE_2D,
                                           .format = VK_FORMAT_D32_SFLOAT,
                                           .width = extent.width,
                                           .height = extent.height,
                                           .depth = 1,
                                           .mip_levels = 1,
                                           .array_layers = 1,
                                           .sample_count = VK_SAMPLE_COUNT_1_BIT,
                                           .image_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                           .view_type = VK_IMAGE_VIEW_TYPE_2D,
                                           .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                           .mip_map_level_count = 1,
                                           .layer_count = 1});
    // TODO: Flip y for everything in mesh for Vulkan
    Rndr::Mesh mesh;
    Rndr::MaterialDesc material_desc;
    const Opal::StringUtf8 mesh_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "sample-models", "Suzanne", "glTF", "Suzanne.gltf");
    Rndr::File::LoadMeshAndMaterialDescription(mesh_path, mesh, material_desc);

}