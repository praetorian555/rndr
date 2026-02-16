#include "example-controller.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/in-place-array.h"
#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/advanced/advanced-buffer.hpp"
#include "rndr/advanced/advanced-descriptor-set.hpp"
#include "rndr/advanced/advanced-pipeline.hpp"
#include "rndr/advanced/advanced-shader.hpp"
#include "rndr/advanced/advanced-texture.hpp"
#include "rndr/advanced/command-buffer.hpp"
#include "rndr/advanced/device.hpp"
#include "rndr/advanced/graphics-context.hpp"
#include "rndr/advanced/physical-device.hpp"
#include "rndr/advanced/swap-chain.hpp"
#include "rndr/advanced/synchronization.hpp"
#include "rndr/application.hpp"
#include "rndr/file.hpp"
#include "rndr/fly-camera.hpp"
#include "rndr/projections.hpp"
#include "rndr/types.hpp"

using i32 = Rndr::i32;
using u32 = Rndr::u32;
using f32 = Rndr::f32;
using u8 = Rndr::u8;

struct ShaderData
{
    Rndr::Matrix4x4f projection;
    Rndr::Matrix4x4f view;
    Opal::InPlaceArray<Rndr::Matrix4x4f, 3> models;
    Rndr::Vector4f light_position{0, -1, 10, 0};
    u32 selected = 1;
};

void Run();

int main()
{
    try
    {
        Run();
    } catch (const Opal::Exception& e)
    {
        printf("%s", *e.What());
        return 1;
    }
    return 0;
}

void Run()
{
    constexpr i32 k_frames_in_flight = 2;

    auto rndr_app = Rndr::Application::Create({.enable_input_system = true});
    Rndr::GenericWindow* window = rndr_app->CreateGenericWindow();
    rndr_app->EnableHighPrecisionCursorMode(true, *window);
    rndr_app->ShowCursor(false);
    rndr_app->SetCursorPositionMode(Rndr::CursorPositionMode::ResetToCenter);

    Rndr::AdvancedGraphicsContext graphics_context{{.collect_debug_messages = true}};
    Rndr::AdvancedSurface surface(graphics_context, window);

    auto physical_devices = graphics_context.EnumeratePhysicalDevices();
    Rndr::AdvancedDevice device(std::move(physical_devices[0]), graphics_context, {.surface = Opal::Ref{surface}});
    auto graphics_queue = device.GetQueue(Rndr::QueueFamily::Graphics);
    auto present_queue = device.GetQueue(Rndr::QueueFamily::Present);

    Rndr::AdvancedSwapChain swap_chain(device, surface, {.use_depth = true, .depth_pixel_format = Rndr::PixelFormat::D32_SFLOAT});

    // TODO: Flip y for everything in mesh for Vulkan
    Rndr::Mesh mesh;
    Rndr::MaterialDesc material_desc;
    const Opal::StringUtf8 mesh_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "sample-models", "Suzanne", "glTF", "Suzanne.gltf");
    Rndr::File::LoadMeshAndMaterialDescription(mesh_path, mesh, material_desc);
    Opal::DynamicArray<Rndr::u8> combined_vertex_index_data;
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
            device, {.size = sizeof(ShaderData), .usage = 0, .keep_memory_mapped = true, .use_device_address = true});
    }

    // Create fences and semaphores
    Opal::DynamicArray<Rndr::AdvancedFence> fences;
    Opal::DynamicArray<Rndr::AdvancedSemaphore> present_semaphores;
    for (i32 i = 0; i < k_frames_in_flight; ++i)
    {
        constexpr bool k_start_signaled = true;
        fences.EmplaceBack(device, k_start_signaled);
        present_semaphores.EmplaceBack(device);
    }
    Opal::DynamicArray<Rndr::AdvancedSemaphore> render_semaphores;
    for (u32 i = 0; i < swap_chain.GetColorImageCount(); ++i)
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
    const Rndr::AdvancedDescriptorPool descriptor_pool(device, descriptor_pool_desc);

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

    const Opal::StringUtf8 shader_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "shaders", "modern-vulkan.spv");
    Opal::DynamicArray<u8> shader_contents = Rndr::File::ReadEntireFile(shader_path);
    Rndr::AdvancedShader vertex_shader(device, shader_contents, {.entry_point = "main_vertex"});
    Rndr::AdvancedShader fragment_shader(device, shader_contents, {.entry_point = "main_fragment"});

    Rndr::AdvancedVertexInputDesc vertex_input_desc;
    vertex_input_desc.AddBinding(0, mesh.vertex_size, Rndr::DataRepetition::PerVertex);
    vertex_input_desc.AddAttribute(0, 0, Rndr::PixelFormat::R32G32B32_SFLOAT, 0);
    vertex_input_desc.AddAttribute(0, 1, Rndr::PixelFormat::R32G32B32_SFLOAT, sizeof(Rndr::Vector3f));
    vertex_input_desc.AddAttribute(0, 2, Rndr::PixelFormat::R32G32_SFLOAT, 2 * sizeof(Rndr::Vector3f));

    Rndr::AdvancedPushConstantRange push_constant_range{
        .shader_stages = Rndr::ShaderTypeBits::Vertex,
        .size = sizeof(VkDeviceAddress),
    };

    Rndr::AdvancedColorBlendDesc color_blend_desc;
    const Rndr::AdvancedGraphicsPipelineDesc pipeline_desc{
        .vertex_input = vertex_input_desc,
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .descriptor_set_layouts = {descriptor_set_layout},
        .push_constant_ranges = {push_constant_range},
        .depth_stencil = {.depth_test_enabled = true, .depth_write_enabled = true, .depth_comparator = Rndr::Comparator::LessEqual},
        .color_blend_attachments = {color_blend_desc},
        .color_attachment_formats = {swap_chain.GetDesc().pixel_format},
        .depth_attachment_format = swap_chain.GetDesc().depth_pixel_format};
    Rndr::AdvancedPipeline pipeline(device, pipeline_desc);

    rndr_app->GetInputSystemChecked().GetCurrentContext().AddAction(
    "Exit",
    {Rndr::InputBinding::CreateKeyboardButtonBinding(Rndr::InputPrimitive::Escape, Rndr::InputTrigger::ButtonPressed,
                                                     [window](Rndr::InputPrimitive, Rndr::InputTrigger, Rndr::f32, bool)
                                                     { window->ForceClose(); })});

    Rndr::Vector2i window_size = window->GetSize().GetValue();
    f32 window_width = window_size.x;
    f32 window_height = window_size.y;
    const Rndr::FlyCameraDesc fly_camera_desc{.start_position = {0.0f, 1.0f, 0.0f},
                                              .start_yaw_radians = 0,
                                              .projection_desc = {.near = 0.1f, .far = 32.0f, .complexity = Rndr::ApiComplexity::Advanced}};
    ExampleController controller(*rndr_app, window_width, window_height, fly_camera_desc, 10.0f, 0.005f, 0.005f);
    controller.Enable(true);

    Rndr::f32 delta_seconds = 0.016;
    Rndr::u32 frame_index = 0;
    while (!window->IsClosed())
    {
        Opal::GetScratchAllocator()->Reset();
        auto start_time = Opal::GetSeconds();

        window_size = window->GetSize().GetValue();
        f32 window_width = window_size.x;
        f32 window_height = window_size.y;

        // Acquire next swap chain image to render to
        fences[frame_index].Wait();
        fences[frame_index].Reset();
        u32 image_index = swap_chain.AcquireImage(present_semaphores[frame_index]);

        rndr_app->ProcessSystemEvents(delta_seconds);
        controller.Tick(delta_seconds);

        // Update shader data
        ShaderData shader_data;
        shader_data.projection = controller.GetProjectionTransform();
        shader_data.view = controller.GetViewTransform();
        for (i32 i = 0; i < 3; i++)
        {
            shader_data.models[i] = Opal::Translate(Rndr::Point3f{(static_cast<f32>(i) - 1) * 3.0f, 0.0f, 0.0f});
        }
        m_shader_buffers[frame_index].Update(Opal::AsBytes(shader_data));

        // Start recording rendering commands
        auto& command_buffer = command_buffers[frame_index];
        command_buffer.Reset();
        command_buffer.Begin();

        // Make sure our color and depth attachment are ready and in proper layout
        Opal::InPlaceArray<Rndr::AdvancedImageBarrier, 2> barriers{
            {.stages_must_finish = Rndr::PipelineStageBits::ColorAttachmentOutput,
             .stages_must_finish_access = Rndr::PipelineStageAccessBits::None,
             .before_stages_start = Rndr::PipelineStageBits::ColorAttachmentOutput,
             .before_stages_start_access = Rndr::PipelineStageAccessBits::Read | Rndr::PipelineStageAccessBits::Write,
             .old_layout = Rndr::ImageLayout::Undefined,
             .new_layout = Rndr::ImageLayout::ColorAttachment,
             .image = swap_chain.GetColorImage(static_cast<i32>(image_index))},
            {.stages_must_finish = Rndr::PipelineStageBits::EarlyFragmentTests | Rndr::PipelineStageBits::LateFragmentTests,
             .stages_must_finish_access = Rndr::PipelineStageAccessBits::Write,
             .before_stages_start = Rndr::PipelineStageBits::EarlyFragmentTests | Rndr::PipelineStageBits::LateFragmentTests,
             .before_stages_start_access = Rndr::PipelineStageAccessBits::Write,
             .old_layout = Rndr::ImageLayout::Undefined,
             .new_layout = Rndr::ImageLayout::DepthStencilAttachment,
             .image = swap_chain.GetDepthImage(),
             .subresource_range = {
                 .aspect_mask = Rndr::ImageAspectBits::Depth,
             }}};
        command_buffer.CmdImageBarriers(barriers);

        // Configure attachments, what happens when they are loaded and how they are stored after rendering
        // Do the actual draw calls
        const Rndr::AdvancedRenderingDesc rendering_desc{
            .render_area_extent = window_size,
            .color_attachments = {Rndr::AdvancedRenderingAttachmentDesc{.image_view = swap_chain.GetColorImageView(image_index),
                                                                        .image_layout = Rndr::ImageLayout::ColorAttachment,
                                                                        .load_operation = Rndr::AttachmentLoadOperation::Clear,
                                                                        .store_operation = Rndr::AttachmentStoreOperation::Store,
                                                                        .clear_value = {.color = {0.0f, 0.0f, 0.2f, 1.0f}}}},
            .depth_attachment = {.image_view = swap_chain.GetDepthImageView(),
                                 .image_layout = Rndr::ImageLayout::DepthStencilAttachment,
                                 .load_operation = Rndr::AttachmentLoadOperation::Clear,
                                 .store_operation = Rndr::AttachmentStoreOperation::DontCare,
                                 .clear_value = {.depth_stencil = {.depth = 1.0f, .stencil = 0}}}};
        command_buffer.CmdBeginRendering(rendering_desc);
        command_buffer.CmdSetViewport(Rndr::Vector2f::Zero(), {window_width, window_height});
        command_buffer.CmdSetScissor(Rndr::Vector2i::Zero(), window_size);
        command_buffer.CmdBindVertexBuffer(mesh_buffer, 0);
        command_buffer.CmdBindIndexBuffer(mesh_buffer, mesh.vertex_count * mesh.vertex_size, Rndr::IndexSize::uint32);
        command_buffer.CmdBindPipeline(pipeline);
        command_buffer.CmdBindDescriptorSet(pipeline, descriptor_set);
        VkDeviceAddress device_address = m_shader_buffers[frame_index].GetNativeDeviceAddress();
        command_buffer.CmdPushConstants(pipeline, Rndr::ShaderTypeBits::Vertex, Opal::AsBytes(device_address));
        command_buffer.CmdDrawIndexed(mesh.index_count, 3);
        command_buffer.CmdEndRendering();

        command_buffer.CmdImageBarrier({.stages_must_finish = Rndr::PipelineStageBits::ColorAttachmentOutput,
                                        .stages_must_finish_access = Rndr::PipelineStageAccessBits::Write,
                                        .before_stages_start = Rndr::PipelineStageBits::ColorAttachmentOutput,
                                        .old_layout = Rndr::ImageLayout::ColorAttachment,
                                        .new_layout = Rndr::ImageLayout::Present,
                                        .image = swap_chain.GetColorImage(static_cast<i32>(image_index))});
        command_buffer.End();

        graphics_queue->Submit(command_buffer, present_semaphores[frame_index], Rndr::PipelineStageBits::ColorAttachmentOutput,
                               render_semaphores[image_index], fences[frame_index]);
        frame_index = (frame_index + 1) % k_frames_in_flight;
        swap_chain.Present(image_index, present_queue, render_semaphores[image_index]);

        auto end_time = Opal::GetSeconds();
        delta_seconds = static_cast<f32>(end_time - start_time);
    }

    device.WaitForAll();
}