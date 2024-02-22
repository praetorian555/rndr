#include <filesystem>

#include <gli/gli.hpp>

#include "rndr/core/base.h"
#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/file.h"
#include "rndr/core/input.h"
#include "rndr/core/render-api.h"
#include "rndr/core/renderer-base.h"  // TODO: Rename renderer-base.h to renderer-manager.h
#include "rndr/core/time.h"
#include "rndr/core/window.h"
#include "rndr/utility/cube-map.h"
#include "rndr/utility/fly-camera.h"
#include "rndr/utility/input-layout-builder.h"
#include "rndr/utility/mesh.h"
#include "rndr/utility/material.h"

void Run(const Rndr::String& asset_path);

int main(int argc, char* argv[])
{
    Rndr::Init({.enable_input_system = true});
    Run(GLTF_SAMPLE_ASSETS_DIR "DamagedHelmet/glTF/");
    Rndr::Destroy();
    return 0;
}

struct InstanceData
{
    Rndr::Matrix4x4f model;
    Rndr::Matrix4x4f normal;
};

struct PerFrameData
{
    Rndr::Matrix4x4f view_projection;
    Rndr::Point3f camera_position;
};

class PbrRenderer : public Rndr::RendererBase
{
public:
    PbrRenderer(const Rndr::String& name, const Rndr::RendererBaseDesc& desc, const Rndr::String& asset_path)
        : Rndr::RendererBase(name, desc), m_asset_path(asset_path)
    {
        using namespace Rndr;
        const String vertex_shader_code = Rndr::File::ReadShader(ASSETS_DIR, "material-pbr.vert");
        const String fragment_shader_code = Rndr::File::ReadShader(ASSETS_DIR, "material-pbr.frag");
        m_vertex_shader = Shader(desc.graphics_context, {.type = ShaderType::Vertex, .source = vertex_shader_code});
        RNDR_ASSERT(m_vertex_shader.IsValid());
        m_fragment_shader = Shader(desc.graphics_context, {.type = ShaderType::Fragment, .source = fragment_shader_code});
        RNDR_ASSERT(m_fragment_shader.IsValid());

        const Rndr::String mesh_path = m_asset_path + "DamagedHelmet.rndrmesh";
        if (!Rndr::Mesh::ReadOptimizedData(m_mesh_data, mesh_path))
        {
            RNDR_LOG_ERROR("Failed to load mesh data from file: %s", mesh_path.c_str());
            exit(1);
        }
        const Rndr::String material_path = m_asset_path + "DamagedHelmet.rndrmat";
        Rndr::Array<Rndr::MaterialDescription> material_descriptions;
        Rndr::Array<Rndr::String> texture_paths;
        if (!Rndr::Material::ReadOptimizedData(material_descriptions, texture_paths, material_path))
        {
            RNDR_LOG_ERROR("Failed to load material data from file: %s", material_path.c_str());
            exit(1);
        }
        m_material_data = material_descriptions[0];
        // TODO: Implement this function
        if (!Rndr::Material::SetupMaterial(m_material_data, m_material_textures, m_desc.graphics_context, texture_paths))
        {
            RNDR_LOG_ERROR("Failed to setup material data!");
            exit(1);
        }

        m_vertex_buffer = Rndr::Buffer(desc.graphics_context,
                                       {.type = Rndr::BufferType::ShaderStorage,
                                        .usage = Rndr::Usage::Default,
                                        .size = static_cast<uint32_t>(m_mesh_data.vertex_buffer_data.size())},
                                       m_mesh_data.vertex_buffer_data);
        RNDR_ASSERT(m_vertex_buffer.IsValid());
        m_index_buffer = Rndr::Buffer(desc.graphics_context,
                                      {.type = Rndr::BufferType::Index,
                                       .usage = Rndr::Usage::Default,
                                       .size = static_cast<uint32_t>(m_mesh_data.index_buffer_data.size()),
                                       .stride = sizeof(uint32_t)},
                                      m_mesh_data.index_buffer_data);
        RNDR_ASSERT(m_index_buffer.IsValid());

        m_instance_buffer = Rndr::Buffer(
            desc.graphics_context, {.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Dynamic, .size = sizeof(InstanceData)});
        RNDR_ASSERT(m_instance_buffer.IsValid());
        Rndr::Matrix4x4f model_transform = Math::Translate(Rndr::Vector3f(0.0f, 0.0f, 0.0f)) * Math::RotateX(90.0f) * Math::Scale(1.0f);
        model_transform = Math::Transpose(model_transform);
        InstanceData instance_data = {.model = model_transform, .normal = model_transform};
        m_desc.graphics_context->Update(m_instance_buffer, Rndr::ToByteSpan(instance_data));

        m_material_buffer = Rndr::Buffer(desc.graphics_context, {.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Dynamic, .size = sizeof(MaterialDescription)});
        RNDR_ASSERT(m_material_buffer.IsValid());
        m_desc.graphics_context->Update(m_material_buffer, Rndr::ToByteSpan(m_material_data));

        m_per_frame_buffer = Rndr::Buffer(
            desc.graphics_context, {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = sizeof(PerFrameData)});
        RNDR_ASSERT(m_per_frame_buffer.IsValid());

        const Rndr::String env_map_image_path = ASSETS_DIR "/piazza_bologni_1k.hdr";
        m_env_map_image = LoadImage(Rndr::ImageType::CubeMap, env_map_image_path);
        RNDR_ASSERT(m_env_map_image.IsValid());

        const Rndr::String irradiance_map_image_path = ASSETS_DIR "/piazza_bologni_1k_irradience.hdr";
        m_irradiance_map_image = LoadImage(Rndr::ImageType::CubeMap, irradiance_map_image_path);
        RNDR_ASSERT(m_irradiance_map_image.IsValid());

        const Rndr::String brdf_lut_image_path = ASSETS_DIR "/brdf-lut.ktx";
        m_brdf_lut_image = LoadImage(Rndr::ImageType::Image2D, brdf_lut_image_path);

        const Rndr::InputLayoutDesc input_layout_desc = Rndr::InputLayoutBuilder()
                                                            .AddVertexBuffer(m_vertex_buffer, 1, Rndr::DataRepetition::PerVertex)
                                                            .AddVertexBuffer(m_instance_buffer, 2, Rndr::DataRepetition::PerInstance, 1)
                                                            .AddIndexBuffer(m_index_buffer)
                                                            .Build();
        m_pipeline = Rndr::Pipeline(desc.graphics_context, {.vertex_shader = &m_vertex_shader,
                                                            .pixel_shader = &m_fragment_shader,
                                                            .input_layout = input_layout_desc,
                                                            .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                            .depth_stencil = {.is_depth_enabled = true}});
        RNDR_ASSERT(m_pipeline.IsValid());

        m_command_list = Rndr::CommandList(desc.graphics_context);
        m_command_list.BindConstantBuffer(m_per_frame_buffer, 0);
        m_command_list.Bind(m_env_map_image, 5);
        m_command_list.Bind(m_irradiance_map_image, 6);
        m_command_list.Bind(m_brdf_lut_image, 7);
        m_command_list.Bind(m_pipeline);
        const MeshDescription mesh_desc = m_mesh_data.meshes[0];
        m_command_list.DrawIndices(Rndr::PrimitiveTopology::Triangle, mesh_desc.GetLodIndicesCount(0), 1, mesh_desc.index_offset);
    }

    bool Render() override
    {
        const Rndr::Matrix4x4f view_projection_transform = Math::Transpose(m_camera_transform);
        PerFrameData per_frame_data = {.view_projection = view_projection_transform, .camera_position = m_camera_position};
        m_desc.graphics_context->Update(m_per_frame_buffer, Rndr::ToByteSpan(per_frame_data));

        m_desc.graphics_context->Bind(m_material_buffer, 3);
        m_command_list.Submit();
        return true;
    }

    void SetCameraInfo(const Rndr::Matrix4x4f& transform, const Rndr::Point3f& position)
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
    Rndr::String m_asset_path;

    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_fragment_shader;

    Rndr::MeshData m_mesh_data;
    Rndr::MaterialDescription m_material_data;

    Rndr::Buffer m_vertex_buffer;
    Rndr::Buffer m_index_buffer;
    Rndr::Buffer m_instance_buffer;
    Rndr::Buffer m_material_buffer;
    Rndr::Buffer m_per_frame_buffer;
    Rndr::Array<Rndr::Image> m_material_textures;

    Rndr::Image m_env_map_image;
    Rndr::Image m_irradiance_map_image;
    Rndr::Image m_brdf_lut_image;

    Rndr::Pipeline m_pipeline;
    Rndr::CommandList m_command_list;

    Rndr::Matrix4x4f m_camera_transform;
    Rndr::Point3f m_camera_position;
};

void Run(const Rndr::String& asset_path)
{
    using namespace Rndr;
    Window window({.width = 1920, .height = 1080, .name = "Material System Example"});
    GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});

    FlyCamera fly_camera(&window, &InputSystem::GetCurrentContext(),
                         {.start_position = Point3f(0.0f, 0.0f, 5.0f),
                          .movement_speed = 10,
                          .rotation_speed = 100,
                          .projection_desc = {.near = 0.05f, .far = 5000.0f}});

    SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight()});
    const RendererBaseDesc renderer_desc{.graphics_context = Ref{graphics_context}, .swap_chain = Ref{swap_chain}};
    RendererManager renderer_manager;
    const ScopePtr<RendererBase> clear_renderer = MakeScoped<ClearRenderer>(String("Clear"), renderer_desc, Colors::k_white);
    const ScopePtr<PbrRenderer> pbr_renderer = MakeScoped<PbrRenderer>(String("PBR"), renderer_desc, asset_path);
    const ScopePtr<RendererBase> present_renderer = MakeScoped<PresentRenderer>(String("Present"), renderer_desc);
    renderer_manager.AddRenderer(clear_renderer.get());
    renderer_manager.AddRenderer(pbr_renderer.get());
    renderer_manager.AddRenderer(present_renderer.get());

    float delta_seconds = 1 / 60.0f;
    while (!window.IsClosed())
    {
        const Timestamp start_time = GetTimestamp();

        window.ProcessEvents();
        InputSystem::ProcessEvents(delta_seconds);

        fly_camera.Update(delta_seconds);
        pbr_renderer->SetCameraInfo(fly_camera.FromWorldToNDC(), fly_camera.GetPosition());

        renderer_manager.Render();

        const Timestamp end_time = GetTimestamp();
        delta_seconds = static_cast<float>(GetDuration(start_time, end_time));
    }
}