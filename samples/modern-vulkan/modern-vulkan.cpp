#include <chrono>
#include <thread>

#include "example-controller.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/in-place-array.h"
#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/forge/buffer.hpp"
#include "rndr/forge/descriptor-set.hpp"
#include "rndr/forge/pipeline.hpp"
#include "rndr/forge/shader.hpp"
#include "rndr/forge/texture.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/swap-chain.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/mesh.hpp"
#include "rndr/application.hpp"
#include "rndr/file.hpp"
#include "rndr/fly-camera.hpp"
#include "rndr/generic-window.hpp"
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
    }
    catch (const Opal::Exception& e)
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
    auto window = rndr_app->CreateGenericWindow({});
    window->EnableHighPrecisionCursorMode(true);
    rndr_app->ShowCursor(false);
    window->SetCursorPositionMode(Rndr::CursorPositionMode::ResetToCenter);

    Rndr::Forge::GraphicsContext graphics_context{{.collect_debug_messages = true}};
    Rndr::Forge::Surface surface(graphics_context, *window);

    auto physical_devices = graphics_context.EnumeratePhysicalDevices();
    Rndr::Forge::Device device(std::move(physical_devices[0]), graphics_context, {.surface = surface});
    Rndr::Forge::DeviceQueue& graphics_queue = device.GetQueue(Rndr::Forge::QueueFamily::Graphics);
    Rndr::Forge::DeviceQueue& present_queue = device.GetQueue(Rndr::Forge::QueueFamily::Present);

    Rndr::Forge::SwapChain swap_chain(device, surface, {.use_depth = true, .depth_pixel_format = Rndr::PixelFormat::D32_SFLOAT});

    const Opal::StringUtf8 mesh_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "sample-models", "Suzanne", "glTF", "Suzanne.gltf");
    Rndr::Forge::Mesh mesh;
    Rndr::Forge::LoadMesh(mesh_path, mesh);
    Opal::DynamicArray<Rndr::u8> combined_vertex_index_data;
    combined_vertex_index_data.Append(mesh.vertices);
    combined_vertex_index_data.Append(mesh.indices);
    Rndr::Forge::Buffer mesh_buffer(device,
                                     {.size = combined_vertex_index_data.GetSize(),
                                      .usage = Rndr::Forge::BufferUsageBits::VertexBuffer | Rndr::Forge::BufferUsageBits::IndexBuffer,
                                      .keep_memory_mapped = false},
                                     combined_vertex_index_data);

    Opal::InPlaceArray<Rndr::Forge::Buffer, k_frames_in_flight> m_shader_buffers;
    for (i32 i = 0; i < k_frames_in_flight; i++)
    {
        m_shader_buffers[i] =
            Rndr::Forge::Buffer(device, {.size = sizeof(ShaderData),
                                         .usage = Rndr::Forge::BufferUsageBits::None,
                                         .keep_memory_mapped = true,
                                         .use_device_address = true});
    }

    // Create fences and semaphores
    Opal::DynamicArray<Rndr::Forge::Fence> fences;
    Opal::DynamicArray<Rndr::Forge::Semaphore> present_semaphores;
    for (i32 i = 0; i < k_frames_in_flight; ++i)
    {
        constexpr bool k_start_signaled = true;
        fences.EmplaceBack(device, k_start_signaled);
        present_semaphores.EmplaceBack(device);
    }
    // One semaphore per swap chain image, so they have to be rebuilt whenever the swap chain is.
    Opal::DynamicArray<Rndr::Forge::Semaphore> render_semaphores;
    auto match_render_semaphores_to_swap_chain = [&device, &render_semaphores, &swap_chain]
    {
        if (render_semaphores.GetSize() == swap_chain.GetColorImageCount())
        {
            return;
        }
        render_semaphores.Clear();
        for (u32 i = 0; i < swap_chain.GetColorImageCount(); ++i)
        {
            render_semaphores.EmplaceBack(device);
        }
    };
    match_render_semaphores_to_swap_chain();

    Opal::DynamicArray<Rndr::Forge::CommandBuffer> command_buffers;
    for (Rndr::i32 i = 0; i < k_frames_in_flight; ++i)
    {
        command_buffers.EmplaceBack(device, graphics_queue);
    }

    const Opal::StringUtf8 albedo_texture_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "sample-models", "Suzanne", "glTF", "Suzanne_BaseColor.png");
    const Opal::StringUtf8 metallic_roughness_texture_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "sample-models", "Suzanne", "glTF", "Suzanne_MetallicRoughness.png");
    const Rndr::Bitmap albedo_bitmap = Rndr::File::LoadImage(albedo_texture_path, true, true);
    const Rndr::Bitmap mr_bitmap = Rndr::File::LoadImage(metallic_roughness_texture_path, true, true);
    Rndr::Forge::Texture albedo_texture(device, graphics_queue, albedo_bitmap);
    Rndr::Forge::Texture mr_texture(device, graphics_queue, mr_bitmap);
    Rndr::Forge::Sampler albedo_sampler(device, {.max_anisotropy = 8.0f, .max_lod = static_cast<f32>(albedo_bitmap.GetMipCount())});
    Rndr::Forge::Sampler mr_sampler(device, {.max_anisotropy = 8.0f, .max_lod = static_cast<f32>(mr_bitmap.GetMipCount())});

    // Setup descriptor pool
    Rndr::Forge::DescriptorPoolDesc descriptor_pool_desc;
    descriptor_pool_desc.Add(Rndr::Forge::DescriptorType::CombinedImageSampler, 100);
    descriptor_pool_desc.max_sets = k_frames_in_flight;
    const Rndr::Forge::DescriptorPool descriptor_pool(device, descriptor_pool_desc);

    // Setup the descriptor set layout. It has two bindings and both are images with samplers.
    Rndr::Forge::DescriptorSetLayoutDesc layout_desc;
    layout_desc.AddBinding(Rndr::Forge::DescriptorType::CombinedImageSampler, 1, Rndr::ShaderTypeBits::Fragment);
    layout_desc.AddBinding(Rndr::Forge::DescriptorType::CombinedImageSampler, 1, Rndr::ShaderTypeBits::Fragment);
    Rndr::Forge::DescriptorSetLayout descriptor_set_layout(device, layout_desc);

    // Allocate descriptor set from the descriptor pool and fill it with concrete data.
    Rndr::Forge::DescriptorSet descriptor_set(descriptor_pool, descriptor_set_layout);
    Opal::DynamicArray<Rndr::Forge::DescriptorSetUpdateBinding> update_bindings;
    Rndr::Forge::DescriptorSetUpdateBinding binding1{
        .descriptor_type = Rndr::Forge::DescriptorType::CombinedImageSampler,
        .binding = 0,
        .resource_info = Rndr::Forge::DescriptorSetUpdateBinding::ImageInfo{
            .sampler = albedo_sampler, .image = albedo_texture, .image_layout = Rndr::Forge::ImageLayout::ShaderReadOnly}};
    update_bindings.PushBack(std::move(binding1));
    Rndr::Forge::DescriptorSetUpdateBinding binding2{
        .descriptor_type = Rndr::Forge::DescriptorType::CombinedImageSampler,
        .binding = 1,
        .resource_info = Rndr::Forge::DescriptorSetUpdateBinding::ImageInfo{
            .sampler = mr_sampler, .image = mr_texture, .image_layout = Rndr::Forge::ImageLayout::ShaderReadOnly}};
    update_bindings.PushBack(std::move(binding2));
    descriptor_set.UpdateDescriptorSets(update_bindings);

    const Opal::StringUtf8 shader_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "shaders", "modern-vulkan.slang");
    const Rndr::Forge::Shader vertex_shader = Rndr::Forge::Shader::FromSource(device, shader_path, {.entry_point = "main_vertex"});
    const Rndr::Forge::Shader fragment_shader = Rndr::Forge::Shader::FromSource(device, shader_path, {.entry_point = "main_fragment"});

    Rndr::Forge::VertexInputDesc vertex_input_desc;
    vertex_input_desc.AddBinding(0, mesh.vertex_size, Rndr::DataRepetition::PerVertex);
    vertex_input_desc.AddAttribute(0, 0, Rndr::PixelFormat::R32G32B32_SFLOAT, 0);
    vertex_input_desc.AddAttribute(0, 1, Rndr::PixelFormat::R32G32B32_SFLOAT, sizeof(Rndr::Vector3f));
    vertex_input_desc.AddAttribute(0, 2, Rndr::PixelFormat::R32G32_SFLOAT, 2 * sizeof(Rndr::Vector3f));

    Rndr::Forge::PushConstantRange push_constant_range{
        .shader_stages = Rndr::ShaderTypeBits::Vertex,
        .size = sizeof(VkDeviceAddress),
    };

    Rndr::Forge::ColorBlendDesc color_blend_desc;
    const Rndr::Forge::GraphicsPipelineDesc pipeline_desc{
        .vertex_input = std::move(vertex_input_desc),
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .descriptor_set_layouts = {descriptor_set_layout},
        .push_constant_ranges = {push_constant_range},
        .depth_stencil = {.depth_test_enabled = true, .depth_write_enabled = true, .depth_comparator = Rndr::Comparator::LessEqual},
        .color_blend_attachments = {color_blend_desc},
        .color_attachment_formats = {swap_chain.GetDesc().pixel_format},
        .depth_attachment_format = swap_chain.GetDesc().depth_pixel_format};
    Rndr::Forge::Pipeline pipeline(device, pipeline_desc);

    Rndr::Vector2i window_size = window->GetSize();
    f32 window_width = window_size.x;
    f32 window_height = window_size.y;

    rndr_app->GetInputSystemChecked()
        .GetCurrentContext()
        .AddAction("Exit")
        .Bind(Rndr::Key::Escape, Rndr::Trigger::Pressed)
        .OnButton([&window](Rndr::Trigger, bool) { window->RequestClose(); });
    const Rndr::FlyCameraDesc fly_camera_desc{.start_position = {0.0f, 1.0f, 10.0f},
                                              .start_yaw_radians = 0,
                                              .projection_desc = {.near = 0.1f, .far = 32.0f, .complexity = Rndr::ApiComplexity::Advanced}};
    ExampleController controller(*rndr_app, window_width, window_height, fly_camera_desc, 10.0f, 0.005f, 0.005f);
    controller.Enable(true);

    Rndr::f32 delta_seconds = 0.016;
    Rndr::u32 frame_index = 0;
    while (!window->IsClosed())
    {
        auto start_time = Opal::GetSeconds();

        rndr_app->ProcessSystemEvents();
        rndr_app->GetInputSystemChecked().ProcessSystemEvents(delta_seconds);

        const Rndr::Vector2i new_window_size = window->GetSize();
        if (new_window_size != window_size)
        {
            window_size = new_window_size;
            controller.SetScreenSize(window_size.x, window_size.y);
        }
        controller.Tick(delta_seconds);

        // Acquire next swap chain image to render to
        fences[frame_index].Wait();
        const Rndr::Forge::AcquiredImage acquired_image = swap_chain.AcquireImage(present_semaphores[frame_index]);
        if (acquired_image.status == Rndr::Forge::SwapChainStatus::OutOfDate)
        {
            // The swap chain was rebuilt, or the window is minimized and there is none. Nothing was submitted for this
            // frame, so the fence is left signaled and the frame index is not advanced.
            match_render_semaphores_to_swap_chain();
            if (!swap_chain.IsValid())
            {
                // The window has no client area, so there is nothing to render into. Idle instead of retrying as fast
                // as the loop can spin.
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
            const auto skipped_end_time = Opal::GetSeconds();
            delta_seconds = static_cast<f32>(skipped_end_time - start_time);
            continue;
        }
        const u32 image_index = acquired_image.image_index;
        fences[frame_index].Reset();

        // Everything is rendered at the size of the swap chain, which can lag a frame behind the size of the window.
        const VkExtent2D swap_chain_extent = swap_chain.GetExtent();
        const Rndr::Vector2i render_size{static_cast<i32>(swap_chain_extent.width), static_cast<i32>(swap_chain_extent.height)};
        const f32 render_width = static_cast<f32>(render_size.x);
        const f32 render_height = static_cast<f32>(render_size.y);

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
        Opal::InPlaceArray<Rndr::Forge::ImageBarrier, 2> barriers{
            {.stages_must_finish = Rndr::Forge::PipelineStageBits::ColorAttachmentOutput,
             .stages_must_finish_access = Rndr::Forge::PipelineStageAccessBits::None,
             .before_stages_start = Rndr::Forge::PipelineStageBits::ColorAttachmentOutput,
             .before_stages_start_access = Rndr::Forge::PipelineStageAccessBits::Read | Rndr::Forge::PipelineStageAccessBits::Write,
             .old_layout = Rndr::Forge::ImageLayout::Undefined,
             .new_layout = Rndr::Forge::ImageLayout::ColorAttachment,
             .image = swap_chain.GetColorImage(static_cast<i32>(image_index))},
            {.stages_must_finish = Rndr::Forge::PipelineStageBits::EarlyFragmentTests | Rndr::Forge::PipelineStageBits::LateFragmentTests,
             .stages_must_finish_access = Rndr::Forge::PipelineStageAccessBits::Write,
             .before_stages_start = Rndr::Forge::PipelineStageBits::EarlyFragmentTests | Rndr::Forge::PipelineStageBits::LateFragmentTests,
             .before_stages_start_access = Rndr::Forge::PipelineStageAccessBits::Write,
             .old_layout = Rndr::Forge::ImageLayout::Undefined,
             .new_layout = Rndr::Forge::ImageLayout::DepthStencilAttachment,
             .image = swap_chain.GetDepthImage(),
             .subresource_range = {
                 .aspect_mask = Rndr::Forge::ImageAspectBits::Depth,
             }}};
        command_buffer.CmdImageBarriers(barriers);

        // Configure attachments, what happens when they are loaded and how they are stored after rendering
        // Do the actual draw calls
        const Rndr::Forge::RenderingDesc rendering_desc{
            .render_area_extent = render_size,
            .color_attachments = {Rndr::Forge::RenderingAttachmentDesc{.image_view = swap_chain.GetColorImageView(image_index),
                                                                        .image_layout = Rndr::Forge::ImageLayout::ColorAttachment,
                                                                        .load_operation = Rndr::Forge::AttachmentLoadOperation::Clear,
                                                                        .store_operation = Rndr::Forge::AttachmentStoreOperation::Store,
                                                                        .clear_value = {.color = {0.0f, 0.0f, 0.2f, 1.0f}}}},
            .depth_attachment = {.image_view = swap_chain.GetDepthImageView(),
                                 .image_layout = Rndr::Forge::ImageLayout::DepthStencilAttachment,
                                 .load_operation = Rndr::Forge::AttachmentLoadOperation::Clear,
                                 .store_operation = Rndr::Forge::AttachmentStoreOperation::DontCare,
                                 .clear_value = {.depth_stencil = {.depth = 1.0f, .stencil = 0}}}};
        command_buffer.CmdBeginRendering(rendering_desc);
        command_buffer.CmdSetViewport(Rndr::Vector2f::Zero(), {render_width, render_height});
        command_buffer.CmdSetScissor(Rndr::Vector2i::Zero(), render_size);
        command_buffer.CmdBindVertexBuffer(mesh_buffer, 0);
        command_buffer.CmdBindIndexBuffer(mesh_buffer, mesh.vertex_count * mesh.vertex_size, Rndr::IndexSize::uint32);
        command_buffer.CmdBindPipeline(pipeline);
        command_buffer.CmdBindDescriptorSet(pipeline, descriptor_set);
        VkDeviceAddress device_address = m_shader_buffers[frame_index].GetNativeDeviceAddress();
        command_buffer.CmdPushConstants(pipeline, Rndr::ShaderTypeBits::Vertex, Opal::AsBytes(device_address));
        command_buffer.CmdDrawIndexed(mesh.index_count, 3);
        command_buffer.CmdEndRendering();

        command_buffer.CmdImageBarrier({.stages_must_finish = Rndr::Forge::PipelineStageBits::ColorAttachmentOutput,
                                        .stages_must_finish_access = Rndr::Forge::PipelineStageAccessBits::Write,
                                        .before_stages_start = Rndr::Forge::PipelineStageBits::ColorAttachmentOutput,
                                        .old_layout = Rndr::Forge::ImageLayout::ColorAttachment,
                                        .new_layout = Rndr::Forge::ImageLayout::Present,
                                        .image = swap_chain.GetColorImage(static_cast<i32>(image_index))});
        command_buffer.End();

        graphics_queue.Submit(command_buffer, present_semaphores[frame_index], Rndr::Forge::PipelineStageBits::ColorAttachmentOutput,
                              render_semaphores[image_index], fences[frame_index]);
        frame_index = (frame_index + 1) % k_frames_in_flight;
        if (swap_chain.Present(image_index, present_queue, render_semaphores[image_index]) ==
            Rndr::Forge::SwapChainStatus::OutOfDate)
        {
            match_render_semaphores_to_swap_chain();
        }

        auto end_time = Opal::GetSeconds();
        delta_seconds = static_cast<f32>(end_time - start_time);
    }

    device.WaitForAll();
}