#include "../../build/opengl-msvc-opt-debug/_deps/opal-src/include/opal/container/in-place-array.h"
#include "../../build/opengl-msvc-opt-debug/_deps/opal-src/include/opal/paths.h"
#include "opal/container/dynamic-array.h"
#include "rndr/application.hpp"
#include "rndr/file.hpp"

#include "vma/vk_mem_alloc.h"
#include "volk/volk.h"

#include "rndr/advanced/advanced-buffer.hpp"
#include "rndr/advanced/advanced-texture.hpp"
#include "rndr/advanced/device.hpp"
#include "rndr/advanced/graphics-context.hpp"
#include "rndr/advanced/physical-device.hpp"
#include "rndr/advanced/swap-chain.hpp"
#include "rndr/types.hpp"

using i32 = Rndr::i32;
using u32 = Rndr::u32;
using f32 = Rndr::f32;
using u8 = Rndr::u8;

struct ShaderData
{
    Rndr::Matrix4x4f projection;
    Rndr::Matrix4x4f view;
    Rndr::Matrix4x4f model[3];
    Rndr::Vector4f light_position{0, -1, 10, 0};
    u32 selected = 1;
};

int main()
{
    constexpr i32 k_frames_in_flight = 2;

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
    Opal::DynamicArray<Rndr::u8> combined_vertex_index_data(Opal::GetScratchAllocator());
    combined_vertex_index_data.Append(mesh.vertices);
    combined_vertex_index_data.Append(mesh.indices);
    Rndr::AdvancedBuffer mesh_buffer(
        device,
        {.size = combined_vertex_index_data.GetSize(), .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, .keep_memory_mapped = false},
        combined_vertex_index_data);

    Opal::InPlaceArray<Rndr::AdvancedBuffer, k_frames_in_flight> m_shader_buffers;
    for (i32 i = 0; i < k_frames_in_flight; i++)
    {
        m_shader_buffers[i] = Rndr::AdvancedBuffer(
            device, {.size = sizeof(ShaderData), .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, .keep_memory_mapped = true});
    }

    // Create fences and semaphores
    Opal::DynamicArray<VkFence> fences(k_frames_in_flight);
    Opal::DynamicArray<VkSemaphore> present_semaphores(k_frames_in_flight);
    for (i32 i = 0; i < k_frames_in_flight; ++i)
    {
        const VkFenceCreateInfo fence_create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        VkResult result = vkCreateFence(device.GetNativeDevice(), &fence_create_info, nullptr, &fences[i]);
        if (result != VK_SUCCESS)
        {
            throw Opal::Exception("Failed to create a fence!");
        }
        const VkSemaphoreCreateInfo semaphore_create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        result = vkCreateSemaphore(device.GetNativeDevice(), &semaphore_create_info, nullptr, &present_semaphores[i]);
        if (result != VK_SUCCESS)
        {
            throw Opal::Exception("Failed to create a present semaphore!");
        }
    }
    Opal::DynamicArray<VkSemaphore> render_semaphores(swap_chain.GetImageViews().GetSize());
    for (auto& semaphore : render_semaphores)
    {
        const VkSemaphoreCreateInfo semaphore_create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        const VkResult result = vkCreateSemaphore(device.GetNativeDevice(), &semaphore_create_info, nullptr, &semaphore);
        if (result != VK_SUCCESS)
        {
            throw Opal::Exception("Failed to create a render semaphore!");
        }
    }

    auto command_buffers = m_graphics_queue.CreateCommandBuffers(k_frames_in_flight);
}