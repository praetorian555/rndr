#include <filesystem>

#include <gli/gli.hpp>

#include "rndr/rndr.h"

void Run();

int main()
{
    Rndr::Init({.enable_input_system = true, .enable_cpu_tracer = true});
    Run();
    Rndr::Destroy();
    return 0;
}

struct PerFrameData
{
    Rndr::Matrix4x4f view_projection;
    Rndr::Point3f camera_position_world;
};

struct ModelData
{
    Rndr::Matrix4x4f model_transform;
    Rndr::Matrix4x4f normal_transform;
};

class MeshRenderer : public Rndr::RendererBase
{
public:
    MeshRenderer(const Rndr::String& name, const Rndr::RendererBaseDesc& desc) : Rndr::RendererBase(name, desc)
    {
        using namespace Rndr;

        const Rndr::String k_asset_path = GLTF_SAMPLE_ASSETS_DIR "Bistro/Exterior/exterior";
        const Rndr::String k_scene_path = k_asset_path + ".rndrscene";
        const Rndr::String k_mesh_path = k_asset_path + ".rndrmesh";
        const Rndr::String k_mat_path = k_asset_path + ".rndrmat";
        const bool is_data_loaded = Rndr::Scene::ReadScene(m_scene_data, k_scene_path, k_mesh_path, k_mat_path, desc.graphics_context);
        if (!is_data_loaded)
        {
            RNDR_HALT("Failed to load mesh data from file!");
            return;
        }

        // Setup shaders
        const String vertex_shader_code = Rndr::File::ReadShader(ASSETS_DIR, "material-pbr.vert");
        const String fragment_shader_code = Rndr::File::ReadShader(ASSETS_DIR, "material-pbr.frag");
        m_vertex_shader = Shader(desc.graphics_context, {.type = ShaderType::Vertex, .source = vertex_shader_code});
        RNDR_ASSERT(m_vertex_shader.IsValid());
        m_pixel_shader =
            Shader(desc.graphics_context, {.type = ShaderType::Fragment, .source = fragment_shader_code, .defines = {"USE_PBR"}});
        RNDR_ASSERT(m_pixel_shader.IsValid());

        // Setup vertex buffer
        m_vertex_buffer = Rndr::Buffer(desc.graphics_context,
                                       {.type = Rndr::BufferType::ShaderStorage,
                                        .usage = Rndr::Usage::Default,
                                        .size = static_cast<uint32_t>(m_scene_data.mesh_data.vertex_buffer_data.size())},
                                       m_scene_data.mesh_data.vertex_buffer_data);
        RNDR_ASSERT(m_vertex_buffer.IsValid());

        // Setup index buffer
        m_index_buffer = Rndr::Buffer(desc.graphics_context,
                                      {.type = Rndr::BufferType::Index,
                                       .usage = Rndr::Usage::Default,
                                       .size = static_cast<uint32_t>(m_scene_data.mesh_data.index_buffer_data.size()),
                                       .stride = sizeof(uint32_t)},
                                      m_scene_data.mesh_data.index_buffer_data);
        RNDR_ASSERT(m_index_buffer.IsValid());

        // Setup model transforms buffer
        const uint32_t model_transforms_buffer_size = static_cast<uint32_t>(m_scene_data.shapes.size() * sizeof(ModelData));
        Array<ModelData> model_transforms_data(m_scene_data.shapes.size());
        for (int i = 0; i < m_scene_data.shapes.size(); i++)
        {
            const MeshDrawData& shape = m_scene_data.shapes[i];
            const Rndr::Matrix4x4f model_transform = m_scene_data.scene_description.world_transforms[shape.transform_index];
            const Rndr::Matrix4x4f normal_transform = Math::Transpose(Math::Inverse(model_transform));
            model_transforms_data[i] = {.model_transform = model_transform, .normal_transform = normal_transform};
        }
        m_model_transforms_buffer =
            Rndr::Buffer(desc.graphics_context,
                         {.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Dynamic, .size = model_transforms_buffer_size},
                         Rndr::ToByteSpan(model_transforms_data));
        RNDR_ASSERT(m_model_transforms_buffer.IsValid());

        m_material_buffer = Rndr::Buffer(desc.graphics_context,
                                         {.type = Rndr::BufferType::ShaderStorage,
                                          .usage = Rndr::Usage::Dynamic,
                                          .size = static_cast<uint32_t>(m_scene_data.materials.size() * sizeof(MaterialDescription))});
        RNDR_ASSERT(m_material_buffer.IsValid());
        m_desc.graphics_context->Update(m_material_buffer, Rndr::ToByteSpan(m_scene_data.materials));

        // Setup buffer that will be updated every frame with camera info
        constexpr size_t k_per_frame_size = sizeof(PerFrameData);
        m_per_frame_buffer = Rndr::Buffer(
            m_desc.graphics_context,
            {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size, .stride = k_per_frame_size});
        RNDR_ASSERT(m_per_frame_buffer.IsValid());

        // Describe what buffers are bound to what slots. No need to describe data layout since we are using vertex pulling.
        const Rndr::InputLayoutDesc input_layout_desc = Rndr::InputLayoutBuilder()
                                                            .AddShaderStorage(m_vertex_buffer, 1)
                                                            .AddShaderStorage(m_model_transforms_buffer, 2)
                                                            .AddShaderStorage(m_material_buffer, 3)
                                                            .AddIndexBuffer(m_index_buffer)
                                                            .Build();

        // Setup pipeline object.
        m_pipeline = Rndr::Pipeline(desc.graphics_context, {.vertex_shader = &m_vertex_shader,
                                                            .pixel_shader = &m_pixel_shader,
                                                            .input_layout = input_layout_desc,
                                                            .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                            .depth_stencil = {.is_depth_enabled = true}});
        RNDR_ASSERT(m_pipeline.IsValid());

        const Rndr::String env_map_image_path = ASSETS_DIR "/piazza_bologni_1k.hdr";
        m_env_map_image = LoadImage(Rndr::ImageType::CubeMap, env_map_image_path);
        RNDR_ASSERT(m_env_map_image.IsValid());

        const Rndr::String irradiance_map_image_path = ASSETS_DIR "/piazza_bologni_1k_irradience.hdr";
        m_irradiance_map_image = LoadImage(Rndr::ImageType::CubeMap, irradiance_map_image_path);
        RNDR_ASSERT(m_irradiance_map_image.IsValid());

        const Rndr::String brdf_lut_image_path = ASSETS_DIR "/brdf-lut.ktx";
        m_brdf_lut_image = LoadImage(Rndr::ImageType::Image2D, brdf_lut_image_path);

        // Setup draw commands based on the mesh data
        Array<DrawIndicesData> draw_commands;
        if (!Mesh::GetDrawCommands(draw_commands, m_scene_data.shapes, m_scene_data.mesh_data))
        {
            RNDR_HALT("Failed to get draw commands from mesh data!");
            return;
        }
        const Span<DrawIndicesData> draw_commands_span(draw_commands);

        // Create a command list
        m_command_list = CommandList(m_desc.graphics_context);
        m_command_list.Bind(*m_desc.swap_chain);
        m_command_list.Bind(m_pipeline);
        m_command_list.BindConstantBuffer(m_per_frame_buffer, 0);
        m_command_list.Bind(m_env_map_image, 5);
        m_command_list.Bind(m_irradiance_map_image, 6);
        m_command_list.Bind(m_brdf_lut_image, 7);
        m_command_list.DrawIndicesMulti(m_pipeline, PrimitiveTopology::Triangle, draw_commands_span);
    }

    bool Render() override
    {
        RNDR_TRACE_SCOPED(Mesh rendering);

        // Rotate the mesh
        const Rndr::Matrix4x4f t = Math::Scale(0.1f);
        Rndr::Matrix4x4f mvp = m_camera_transform * t;
        mvp = Math::Transpose(mvp);
        PerFrameData per_frame_data = {.view_projection = mvp, .camera_position_world = m_camera_position};
        m_desc.graphics_context->Update(m_per_frame_buffer, Rndr::ToByteSpan(per_frame_data));

        m_command_list.Submit();

        return true;
    }

    void SetCameraTransform(const Rndr::Matrix4x4f& transform, const Rndr::Point3f& position)
    {
        m_camera_transform = transform;
        m_camera_position = position;
    }

    Rndr::Image LoadImage(Rndr::ImageType image_type, const Rndr::String& image_path)
    {
        using namespace Rndr;
        constexpr bool k_flip_vertically = true;

        const std::filesystem::path path(image_path);
        const bool is_ktx = path.extension() == ".ktx";

        if (is_ktx)
        {
            gli::texture texture = gli::load_ktx(image_path);
            const ImageDesc image_desc{.width = texture.extent().x,
                                       .height = texture.extent().y,
                                       .array_size = 1,
                                       .type = image_type,
                                       .pixel_format = Rndr::PixelFormat::R16G16_FLOAT,  // TODO: Fix this!
                                       .use_mips = true,
                                       .sampler = {.max_anisotropy = 16.0f,
                                                   .address_mode_u = ImageAddressMode::Clamp,
                                                   .address_mode_v = ImageAddressMode::Clamp,
                                                   .address_mode_w = ImageAddressMode::Clamp}};
            const ConstByteSpan texture_data{static_cast<uint8_t*>(texture.data(0, 0, 0)), texture.size()};
            return {m_desc.graphics_context, image_desc, texture_data};
        }
        if (image_type == ImageType::Image2D)
        {
            Bitmap bitmap = Rndr::File::ReadEntireImage(image_path, PixelFormat::R8G8B8A8_UNORM, k_flip_vertically);
            RNDR_ASSERT(bitmap.IsValid());
            const ImageDesc image_desc{.width = bitmap.GetWidth(),
                                       .height = bitmap.GetHeight(),
                                       .array_size = 1,
                                       .type = image_type,
                                       .pixel_format = bitmap.GetPixelFormat(),
                                       .use_mips = true,
                                       .sampler = {.max_anisotropy = 16.0f}};
            const ConstByteSpan bitmap_data{bitmap.GetData(), bitmap.GetSize3D()};
            return {m_desc.graphics_context, image_desc, bitmap_data};
        }
        if (image_type == ImageType::CubeMap)
        {
            const Bitmap equirectangular_bitmap = Rndr::File::ReadEntireImage(image_path, PixelFormat::R32G32B32_FLOAT);
            RNDR_ASSERT(equirectangular_bitmap.IsValid());
            const bool is_equirectangular = equirectangular_bitmap.GetWidth() == 2 * equirectangular_bitmap.GetHeight();
            Bitmap vertical_cross_bitmap;
            if (is_equirectangular)
            {
                if (!Rndr::CubeMap::ConvertEquirectangularMapToVerticalCross(equirectangular_bitmap, vertical_cross_bitmap))
                {
                    RNDR_HALT("Failed to convert equirectangular map to vertical cross!");
                    return {};
                }
            }
            else
            {
                vertical_cross_bitmap = equirectangular_bitmap;
            }
            Bitmap cube_map_bitmap;
            if (!Rndr::CubeMap::ConvertVerticalCrossToCubeMapFaces(vertical_cross_bitmap, cube_map_bitmap))
            {
                RNDR_HALT("Failed to convert vertical cross to cube map faces!");
                return {};
            }
            const ImageDesc image_desc{.width = cube_map_bitmap.GetWidth(),
                                       .height = cube_map_bitmap.GetHeight(),
                                       .array_size = cube_map_bitmap.GetDepth(),
                                       .type = image_type,
                                       .pixel_format = cube_map_bitmap.GetPixelFormat(),
                                       .use_mips = true,
                                       .sampler = {.address_mode_u = ImageAddressMode::Clamp,
                                                   .address_mode_v = ImageAddressMode::Clamp,
                                                   .address_mode_w = ImageAddressMode::Clamp}};
            const ConstByteSpan bitmap_data{cube_map_bitmap.GetData(), cube_map_bitmap.GetSize3D()};
            return {m_desc.graphics_context, image_desc, bitmap_data};
        }
        return {};
    }

private:
    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_pixel_shader;

    Rndr::Buffer m_vertex_buffer;
    Rndr::Buffer m_index_buffer;
    Rndr::Buffer m_model_transforms_buffer;
    Rndr::Buffer m_material_buffer;

    Rndr::Image m_env_map_image;
    Rndr::Image m_irradiance_map_image;
    Rndr::Image m_brdf_lut_image;

    Rndr::Buffer m_per_frame_buffer;
    Rndr::Pipeline m_pipeline;
    Rndr::CommandList m_command_list;

    Rndr::SceneDrawData m_scene_data;
    Rndr::Matrix4x4f m_camera_transform;
    Rndr::Point3f m_camera_position;
};

void Run()
{
    Rndr::Window window({.width = 1600, .height = 1200, .name = "Scene Renderer Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight(), .enable_vsync = false});
    RNDR_ASSERT(swap_chain.IsValid());

    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height) { swap_chain.SetSize(width, height); });

    Rndr::Array<Rndr::InputBinding> exit_bindings;
    exit_bindings.push_back({Rndr::InputPrimitive::Keyboard_Esc, Rndr::InputTrigger::ButtonReleased});
    Rndr::InputSystem::GetCurrentContext().AddAction(
        Rndr::InputAction("Exit"),
        Rndr::InputActionData{.callback = [&window](Rndr::InputPrimitive, Rndr::InputTrigger, float) { window.Close(); },
                              .native_window = window.GetNativeWindowHandle(),
                              .bindings = exit_bindings});

    const Rndr::RendererBaseDesc renderer_desc = {.graphics_context = Rndr::Ref{graphics_context}, .swap_chain = Rndr::Ref{swap_chain}};

    constexpr Rndr::Vector4f k_clear_color = Rndr::Colors::k_white;
    const Rndr::ScopePtr<Rndr::RendererBase> clear_renderer =
        RNDR_MAKE_SCOPED(Rndr::ClearRenderer, "Clear the screen", renderer_desc, k_clear_color);
    const Rndr::ScopePtr<Rndr::RendererBase> present_renderer =
        RNDR_MAKE_SCOPED(Rndr::PresentRenderer, "Present the back buffer", renderer_desc);
    const Rndr::ScopePtr<MeshRenderer> mesh_renderer = RNDR_MAKE_SCOPED(MeshRenderer, "Render a mesh", renderer_desc);

    Rndr::FlyCamera fly_camera(&window, &Rndr::InputSystem::GetCurrentContext(),
                               {.start_position = Rndr::Point3f(-20.0f, 15.0f, 20.0f),
                                .movement_speed = 100,
                                .rotation_speed = 200,
                                .projection_desc = {.near = 0.5f, .far = 5000.0f}});

    Rndr::RendererManager renderer_manager;
    renderer_manager.AddRenderer(clear_renderer.get());
    renderer_manager.AddRenderer(mesh_renderer.get());
    renderer_manager.AddRenderer(present_renderer.get());

    Rndr::FramesPerSecondCounter fps_counter(0.1f);
    float delta_seconds = 0.033f;
    while (!window.IsClosed())
    {
        RNDR_TRACE_SCOPED(Frame);

        const Rndr::Timestamp start_time = Rndr::GetTimestamp();

        fps_counter.Update(delta_seconds);

        window.ProcessEvents();
        Rndr::InputSystem::ProcessEvents(delta_seconds);

        fly_camera.Update(delta_seconds);
        mesh_renderer->SetCameraTransform(fly_camera.FromWorldToNDC(), fly_camera.GetPosition());

        renderer_manager.Render();

        const Rndr::Timestamp end_time = Rndr::GetTimestamp();
        delta_seconds = static_cast<float>(Rndr::GetDuration(start_time, end_time));
    }
}