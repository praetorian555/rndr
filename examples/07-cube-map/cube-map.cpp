#include "rndr/rndr.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <imgui.h>

void Run();

/**
 * In this example you will learn how to:
 *      1. ??
 */
int main()
{
    Rndr::Init();
    Run();
    Rndr::Destroy();
}

struct PerFrameData
{
    math::Matrix4x4 model;
    math::Matrix4x4 mvp;
    math::Vector4 camera_pos;
};
struct VertexData
{
    math::Point3 pos;
    math::Normal3 normal;
    math::Point2 tc;
};
constexpr size_t k_per_frame_size = sizeof(PerFrameData);

bool LoadMesh(const Rndr::String& file_path,
              Rndr::Array<VertexData>& vertices,
              Rndr::Array<uint32_t>& indices);

void Run()
{
    Rndr::Array<VertexData> vertices;
    Rndr::Array<uint32_t> indices;
    if (!LoadMesh(ASSETS_DIR "duck.gltf", vertices, indices))
    {
        return;
    }

    bool vertical_sync = false;

    Rndr::Window window({.width = 800, .height = 600, .name = "Cube Map Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    assert(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(
        graphics_context,
        {.width = window.GetWidth(), .height = window.GetHeight(), .enable_vsync = vertical_sync});
    assert(swap_chain.IsValid());

    // Read shaders for duck model from files.
    const Rndr::String model_vertex_shader_code =
        Rndr::File::ReadEntireTextFile(ASSETS_DIR "cube-map-duck.vert");
    const Rndr::String model_pixel_shader_code =
        Rndr::File::ReadEntireTextFile(ASSETS_DIR "cube-map-duck.frag");

    // Create shaders for a model.
    Rndr::Shader model_vertex_shader(
        graphics_context,
        {.type = Rndr::ShaderType::Vertex, .source = model_vertex_shader_code});
    assert(model_vertex_shader.IsValid());
    Rndr::Shader model_pixel_shader(
        graphics_context,
        {.type = Rndr::ShaderType::Fragment, .source = model_pixel_shader_code});
    assert(model_pixel_shader.IsValid());

    // Setup vertex buffer for the model.
    constexpr size_t k_stride = sizeof(VertexData);
    const Rndr::Buffer model_vertex_buffer(
        graphics_context,
        {.type = Rndr::BufferType::ShaderStorage,
         .size = static_cast<uint32_t>(k_stride * vertices.size()),
         .stride = k_stride},
        Rndr::ToByteSpan(vertices));
    assert(model_vertex_buffer.IsValid());

    // Setup index buffer.
    const Rndr::Buffer model_index_buffer(
        graphics_context,
        {.type = Rndr::BufferType::Index,
         .size = static_cast<uint32_t>(sizeof(uint32_t) * indices.size()),
         .stride = sizeof(uint32_t)},
        Rndr::ToByteSpan(indices));
    assert(model_index_buffer.IsValid());

    // Configure input layout.
    Rndr::InputLayoutBuilder builder;
    const Rndr::InputLayoutDesc input_layout_desc =
        builder.AddVertexBuffer(model_vertex_buffer, 1, Rndr::DataRepetition::PerVertex)
            .AddIndexBuffer(model_index_buffer)
            .Build();

    // Configure the pipeline.
    const Rndr::Pipeline model_pipeline(graphics_context,
                                        {.vertex_shader = &model_vertex_shader,
                                         .pixel_shader = &model_pixel_shader,
                                         .input_layout = input_layout_desc,
                                         .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                         .depth_stencil = {.is_depth_enabled = true}});
    assert(model_pipeline.IsValid());

    // Load model albedo texture.
    Rndr::CPUImage model_image = Rndr::File::ReadEntireImage(ASSETS_DIR "duck-base-color.png");
    assert(model_image.IsValid());
    constexpr bool k_use_mips = false;
    const Rndr::Image mesh_albedo(graphics_context, model_image, k_use_mips, {});
    assert(mesh_albedo.IsValid());

    // Create a buffer to store per-frame data.
    Rndr::Buffer per_frame_buffer(graphics_context,
                                  {.type = Rndr::BufferType::Constant,
                                   .usage = Rndr::Usage::Dynamic,
                                   .size = k_per_frame_size,
                                   .stride = k_per_frame_size});
    constexpr math::Vector4 k_clear_color{MATH_REALC(1.0),
                                          MATH_REALC(1.0),
                                          MATH_REALC(1.0),
                                          MATH_REALC(1.0)};

    // Handle window resizing.
    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height)
                          { swap_chain.SetSize(width, height); });

    // Bind stuff that stay the same across the frames.
    graphics_context.Bind(swap_chain);
    graphics_context.Bind(model_pipeline);
    graphics_context.BindUniform(per_frame_buffer, 0);
    graphics_context.Bind(mesh_albedo, 0);

    const int32_t index_count = static_cast<int32_t>(indices.size());
    while (!window.IsClosed())
    {
        window.ProcessEvents();

        // Clear the screen and draw the mesh.
        graphics_context.ClearColorAndDepth(k_clear_color, 1);

        // Setup transform that rotates the model around the Y axis.
        const float ratio = static_cast<Rndr::real>(window.GetWidth())
                            / static_cast<Rndr::real>(window.GetHeight());
        const float angle = std::fmod(10 * Rndr::GetSystemTime(), 360.0f);
        const math::Transform t = math::Translate(math::Vector3(0.0f, -0.5f, -1.5f))
                                  * math::Rotate(angle, math::Vector3(0.0f, 1.0f, 0.0f))
                                  * math::RotateX(-90);
        const math::Matrix4x4 p = math::Perspective_RH_N1(45.0f, ratio, 0.1f, 1000.0f);
        math::Matrix4x4 mvp = math::Multiply(p, t.GetMatrix());
        mvp = mvp.Transpose();
        PerFrameData per_frame_data = {.mvp = mvp};

        // Draw the model.
        graphics_context.Update(per_frame_buffer, Rndr::ToByteSpan(per_frame_data));
        graphics_context.DrawIndices(Rndr::PrimitiveTopology::Triangle, index_count);

        // TODO: Draw cube map.

        graphics_context.Present(swap_chain);
    }
}

bool LoadMesh(const Rndr::String& file_path,
              Rndr::Array<VertexData>& vertices,
              Rndr::Array<uint32_t>& indices)
{
    const aiScene* scene = aiImportFile(file_path.c_str(), aiProcess_Triangulate);
    if ((scene == nullptr) || !scene->HasMeshes())
    {
        RNDR_LOG_ERROR("Unable to load %s", file_path.c_str());
        return false;
    }

    const aiMesh* mesh = scene->mMeshes[0];
    for (unsigned i = 0; i != mesh->mNumVertices; i++)
    {
        const aiVector3D v = mesh->mVertices[i];
        const aiVector3D n = mesh->mNormals[i];
        const aiVector3D t = mesh->mTextureCoords[0][i];
        vertices.push_back({.pos = math::Point3(v.x, v.y, v.z),
                            .normal = math::Normal3(n.x, n.y, n.z),
                            .tc = math::Point2(t.x, t.y)});
    }
    for (unsigned i = 0; i != mesh->mNumFaces; i++)
    {
        for (unsigned j = 0; j != 3; j++)
        {
            indices.push_back(mesh->mFaces[i].mIndices[j]);
        }
    }
    aiReleaseImport(scene);
    return true;
}
