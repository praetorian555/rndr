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
#include "rndr/forge/debug.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/frame-context.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/query.hpp"
#include "rndr/forge/swap-chain.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/transfer.hpp"
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
using f64 = Rndr::f64;
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
    /** How often the window title is rewritten. A figure that changes every frame cannot be read. */
    constexpr f64 k_title_update_period_seconds = 0.25;

    auto rndr_app = Rndr::Application::Create({.enable_input_system = true});
    auto window = rndr_app->CreateGenericWindow({});
    window->EnableHighPrecisionCursorMode(true);
    rndr_app->ShowCursor(false);
    window->SetCursorPositionMode(Rndr::CursorPositionMode::ResetToCenter);

    Rndr::Forge::GraphicsContext graphics_context{{.collect_debug_messages = true}};
    Rndr::Forge::Surface surface(graphics_context, *window);

    // Picks the best device that can do everything the desc asks for, rather than whichever one the driver
    // listed first, which on a laptop is as likely to be the integrated GPU as the discrete one.
    const Rndr::Forge::DeviceDesc device_desc{.surface = surface};
    auto physical_devices = graphics_context.EnumeratePhysicalDevices();
    Rndr::Forge::Device device(Rndr::Forge::SelectPhysicalDevice(physical_devices, device_desc), graphics_context, device_desc);
    Rndr::Forge::DeviceQueue& graphics_queue = device.GetQueue(Rndr::Forge::QueueFamily::Graphics);
    Rndr::Forge::DeviceQueue& present_queue = device.GetQueue(Rndr::Forge::QueueFamily::Present);

    Rndr::Forge::SwapChain swap_chain(device, surface, {.use_depth = true, .depth_pixel_format = Rndr::PixelFormat::D32_SFLOAT});

    const Opal::StringUtf8 mesh_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "sample-models", "Suzanne", "glTF", "Suzanne.gltf").GetValue();
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

    // Owns the frames in flight: a fence, a command buffer and the semaphores on both sides of the swap chain
    // for each, plus the order acquire, submit and present have to happen in.
    Rndr::Forge::FrameContext frame_context(device, swap_chain, graphics_queue, present_queue, {.frames_in_flight = k_frames_in_flight});

    // One pool per frame in flight, two timestamps each: one at the top of the frame and one at the bottom.
    // Per frame rather than shared, because the frame that reads a result must not be the frame writing it.
    Opal::InPlaceArray<Rndr::Forge::TimestampQueryPool, k_frames_in_flight> gpu_timers;
    for (i32 i = 0; i < k_frames_in_flight; i++)
    {
        gpu_timers[i] = Rndr::Forge::TimestampQueryPool(device, {.query_count = 2});
    }
    // A pool holds undefined values until it is reset, and reading one that never was is undefined rather
    // than empty. Reset every pool once here so the first frames can ask for a result and be told there is
    // none yet; from then on the reset each frame records is what keeps them readable.
    Rndr::Forge::ImmediateSubmit(device, graphics_queue,
                                 [&](Rndr::Forge::CommandBuffer& command_buffer)
                                 {
                                     for (i32 i = 0; i < k_frames_in_flight; i++)
                                     {
                                         command_buffer.CmdResetQueryPool(gpu_timers[i]);
                                     }
                                 });

    const Opal::StringUtf8 albedo_texture_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "sample-models", "Suzanne", "glTF", "Suzanne_BaseColor.png").GetValue();
    const Opal::StringUtf8 metallic_roughness_texture_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "sample-models", "Suzanne", "glTF", "Suzanne_MetallicRoughness.png").GetValue();
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
    layout_desc.AddBinding(0, Rndr::Forge::DescriptorType::CombinedImageSampler, 1, Rndr::ShaderTypeBits::Fragment);
    layout_desc.AddBinding(1, Rndr::Forge::DescriptorType::CombinedImageSampler, 1, Rndr::ShaderTypeBits::Fragment);
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

    const Opal::StringUtf8 shader_path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "shaders", "modern-vulkan.slang").GetValue();
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

    // Names show up in validation messages and in a capture, and cost nothing in a build without debug utils.
    Rndr::Forge::SetDebugName(device, mesh_buffer, "suzanne mesh");
    Rndr::Forge::SetDebugName(device, albedo_texture, "suzanne albedo");
    Rndr::Forge::SetDebugName(device, mr_texture, "suzanne metallic roughness");
    Rndr::Forge::SetDebugName(device, albedo_sampler, "albedo sampler");
    Rndr::Forge::SetDebugName(device, mr_sampler, "metallic roughness sampler");
    Rndr::Forge::SetDebugName(device, descriptor_set_layout, "material layout");
    Rndr::Forge::SetDebugName(device, descriptor_set, "material set");
    Rndr::Forge::SetDebugName(device, pipeline, "forward pipeline");
    Rndr::Forge::SetDebugName(device, swap_chain, "swap chain");
    Rndr::Forge::SetDebugName(device, frame_context, "frame");
    for (i32 i = 0; i < k_frames_in_flight; ++i)
    {
        Rndr::Forge::SetDebugName(device, m_shader_buffers[i], "per frame shader data");
        Rndr::Forge::SetDebugName(device, gpu_timers[i], "frame timing");
    }

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
    // What the last completed frame took on the device, and when the title last said so. The title is only
    // rewritten a few times a second: a figure that changes every frame is unreadable, and formatting one
    // every frame is work the frame loop does not need to do.
    f64 gpu_milliseconds = 0.0;
    f64 last_title_update_seconds = 0.0;
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

        // Waits for this frame's slot, acquires an image, and begins its command buffer.
        if (frame_context.BeginFrame() == Rndr::Forge::SwapChainStatus::OutOfDate)
        {
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
        const u32 frame_index = frame_context.GetFrameIndex();

        // BeginFrame has already waited on the fence of this slot, so the timestamps this pool holds are the
        // ones written k_frames_in_flight frames ago and are there to read without stalling. Read them
        // before the reset below throws them away. The first frames have nothing in the pool yet and say so.
        Rndr::Forge::TimestampQueryPool& gpu_timer = gpu_timers[frame_index];
        f64 measured_gpu_ms = 0.0;
        if (gpu_timer.TryGetElapsedMilliseconds(0, 1, measured_gpu_ms))
        {
            gpu_milliseconds = measured_gpu_ms;
        }

        // Everything is rendered at the size of the swap chain, which can lag a frame behind the size of the window.
        const Rndr::Vector2i render_size = frame_context.GetRenderSize();
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

        auto& command_buffer = frame_context.GetCommandBuffer();

        // A pool holds undefined values until it is reset, so the reset comes first every frame, and the two
        // timestamps around the frame's work measure the span between them: from the moment the device
        // reaches the top of this command buffer to the moment everything in it has finished.
        command_buffer.CmdResetQueryPool(gpu_timer);
        command_buffer.CmdWriteTimestamp(gpu_timer, 0, Rndr::Forge::PipelineStageBits::PipelineStart);

        // Make sure our color and depth attachment are ready and in proper layout. Both are cleared when the
        // render pass starts, so neither has to preserve what it holds.
        Opal::InPlaceArray<Rndr::Forge::ImageBarrier, 2> barriers{
            Rndr::Forge::ImageBarrier::ToColorAttachment(frame_context.GetColorImage()),
            Rndr::Forge::ImageBarrier::ToDepthStencilAttachment(swap_chain.GetDepthImage())};
        command_buffer.CmdImageBarriers(barriers);

        // Configure attachments, what happens when they are loaded and how they are stored after rendering
        // Do the actual draw calls
        const Rndr::Forge::RenderingDesc rendering_desc{
            .render_area_extent = render_size,
            .color_attachments = {Rndr::Forge::RenderingAttachmentDesc{.image_view = frame_context.GetColorImageView(),
                                                                        .image_layout = Rndr::Forge::ImageLayout::ColorAttachment,
                                                                        .load_operation = Rndr::Forge::AttachmentLoadOperation::Clear,
                                                                        .store_operation = Rndr::Forge::AttachmentStoreOperation::Store,
                                                                        .clear_value = {.color = {0.0f, 0.0f, 0.2f, 1.0f}}}},
            .depth_attachment = {.image_view = swap_chain.GetDepthImageView(),
                                 .image_layout = Rndr::Forge::ImageLayout::DepthStencilAttachment,
                                 .load_operation = Rndr::Forge::AttachmentLoadOperation::Clear,
                                 .store_operation = Rndr::Forge::AttachmentStoreOperation::DontCare,
                                 .clear_value = {.depth_stencil = {.depth = 1.0f, .stencil = 0}}}};
        {
            // A capture shows everything between the two braces as one collapsible "forward pass" instead of
            // as a run of loose draws. The guard closes the region even if something below throws.
            const Rndr::Forge::ScopedDebugLabel forward_pass(command_buffer, "forward pass", {0.2f, 0.6f, 1.0f, 1.0f});
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
        }

        // The closing half of the span. PipelineEnd waits for everything recorded above to finish, so the
        // difference from the first timestamp covers the whole frame rather than a slice of it.
        command_buffer.CmdWriteTimestamp(gpu_timer, 1, Rndr::Forge::PipelineStageBits::PipelineEnd);

        // Transitions the image to Present, ends the command buffer, submits it and presents.
        frame_context.EndFrame();

        auto end_time = Opal::GetSeconds();
        delta_seconds = static_cast<f32>(end_time - start_time);

        if (end_time - last_title_update_seconds > k_title_update_period_seconds)
        {
            last_title_update_seconds = end_time;
            char title[128] = {};
            snprintf(title, sizeof(title), "Forge - modern-vulkan - CPU %.2f ms - GPU %.3f ms",
                     static_cast<f64>(delta_seconds) * 1000.0, gpu_milliseconds);
            window->SetTitle(Opal::StringUtf8(title));
        }
    }

    device.WaitForAll();
}