#include "opal/container/dynamic-array.h"
#include "opal/container/in-place-array.h"
#include "opal/paths.h"

#include "rndr/advanced/advanced-buffer.hpp"
#include "rndr/advanced/advanced-descriptor-set.hpp"
#include "rndr/advanced/advanced-texture.hpp"
#include "rndr/advanced/command-buffer.hpp"
#include "rndr/advanced/device.hpp"
#include "rndr/advanced/graphics-context.hpp"
#include "rndr/advanced/physical-device.hpp"
#include "rndr/advanced/swap-chain.hpp"
#include "rndr/advanced/synchronization.hpp"
#include "rndr/application.hpp"
#include "rndr/file.hpp"
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
    auto graphics_queue = device.GetQueue(Rndr::QueueFamily::Graphics);
    auto present_queue = device.GetQueue(Rndr::QueueFamily::Present);

    Rndr::AdvancedSwapChain swap_chain(device, surface, {});

    const VkExtent2D extent = swap_chain.GetExtent();
    Rndr::AdvancedTexture texture(device, {.image_type = VK_IMAGE_TYPE_2D,
                                           .format = Rndr::PixelFormat::D32_SFLOAT,
                                           .width = extent.width,
                                           .height = extent.height,
                                           .sample_count = VK_SAMPLE_COUNT_1_BIT,
                                           .image_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                           .view_type = VK_IMAGE_VIEW_TYPE_2D,
                                           .subresource_range = {.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT}});
    // TODO: Flip y for everything in mesh for Vulkan
    Rndr::Mesh mesh;
    Rndr::MaterialDesc material_desc;
    const Opal::StringUtf8 mesh_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "sample-models", "Suzanne", "glTF", "Suzanne.gltf");
    Rndr::File::LoadMeshAndMaterialDescription(mesh_path, mesh, material_desc);
    Opal::DynamicArray<Rndr::u8> combined_vertex_index_data(Opal::GetScratchAllocator());
    combined_vertex_index_data.Append(mesh.vertices);
    combined_vertex_index_data.Append(mesh.indices);
    Rndr::AdvancedBuffer mesh_buffer(device,
                                     {.size = combined_vertex_index_data.GetSize(),
                                      .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                      .keep_memory_mapped = false},
                                     combined_vertex_index_data);

    Opal::InPlaceArray<Rndr::AdvancedBuffer, k_frames_in_flight> m_shader_buffers;
    for (i32 i = 0; i < k_frames_in_flight; i++)
    {
        m_shader_buffers[i] = Rndr::AdvancedBuffer(
            device, {.size = sizeof(ShaderData), .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, .keep_memory_mapped = true});
    }

    // Create fences and semaphores
    Opal::DynamicArray<Rndr::AdvancedFence> fences;
    Opal::DynamicArray<Rndr::AdvancedSemaphore> present_semaphores;
    for (i32 i = 0; i < k_frames_in_flight; ++i)
    {
        fences.EmplaceBack(device, true);
        present_semaphores.EmplaceBack(device);
    }
    Opal::DynamicArray<Rndr::AdvancedSemaphore> render_semaphores;
    for (u32 i = 0; i < swap_chain.GetImageViews().GetSize(); ++i)
    {
        render_semaphores.EmplaceBack(device);
    }

    Opal::DynamicArray<Rndr::AdvancedCommandBuffer> command_buffers;
    for (Rndr::i32 i = 0; i < k_frames_in_flight; ++i)
    {
        command_buffers.EmplaceBack(device, device.GetQueue(Rndr::QueueFamily::Graphics).Get());
    }

    const Rndr::Bitmap albedo_bitmap = Rndr::File::LoadImage(material_desc.albedo_texture_path, true, true);
    const Rndr::Bitmap mr_bitmap = Rndr::File::LoadImage(material_desc.metallic_roughness_texture_path, true, true);
    Rndr::AdvancedTexture albedo_texture(device, device.GetQueue(Rndr::QueueFamily::Graphics), albedo_bitmap);
    Rndr::AdvancedTexture mr_texture(device, device.GetQueue(Rndr::QueueFamily::Graphics), mr_bitmap);
    Rndr::AdvancedSampler albedo_sampler(device, {.max_anisotropy = 8.0f, .max_lod = static_cast<f32>(albedo_bitmap.GetMipCount())});
    Rndr::AdvancedSampler mr_sampler(device, {.max_anisotropy = 8.0f, .max_lod = static_cast<f32>(mr_bitmap.GetMipCount())});

    // Setup descriptor pool
    Rndr::AdvancedDescriptorPoolDesc descriptor_pool_desc;
    descriptor_pool_desc.Add(Rndr::AdvancedDescriptorType::CombinedImageSampler, 100);
    descriptor_pool_desc.max_sets = k_frames_in_flight;
    Rndr::AdvancedDescriptorPool descriptor_pool(device, descriptor_pool_desc);

    // Setup the descriptor set layout. It has two bindings and both are images with samplers.
    Rndr::AdvancedDescriptorSetLayoutDesc layout_desc;
    layout_desc.AddBinding(Rndr::AdvancedDescriptorType::CombinedImageSampler, 1, Rndr::ShaderTypeBits::Fragment);
    layout_desc.AddBinding(Rndr::AdvancedDescriptorType::CombinedImageSampler, 1, Rndr::ShaderTypeBits::Fragment);
    Rndr::AdvancedDescriptorSetLayout descriptor_set_layout(device, layout_desc);

    // Allocate descriptor set from the descriptor pool and fill it with concrete data.
    Rndr::AdvancedDescriptorSet descriptor_set(descriptor_pool, descriptor_set_layout);
    Opal::DynamicArray<Rndr::AdvancedDescriptorSetUpdateBinding> update_bindings;
    Rndr::AdvancedDescriptorSetUpdateBinding binding{
        .descriptor_type = Rndr::AdvancedDescriptorType::CombinedImageSampler,
        .binding = 0,
        .image_info = {.sampler = albedo_sampler, .image = albedo_texture, .image_layout = Rndr::ImageLayout::ShaderReadOnly}};
    update_bindings.PushBack(binding);
    binding.binding = 1;
    binding.image_info = {.sampler = mr_sampler, .image = mr_texture, .image_layout = Rndr::ImageLayout::ShaderReadOnly};
    update_bindings.PushBack(binding);
    descriptor_set.UpdateDescriptorSets(update_bindings);
}