#include "rndr/rndr.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>
#include <meshoptimizer.h>

void Run();

/**
 * In this example you can learn how to:
 * - Create a data buffer to change data on the GPU per frame.
 * - Use math transformations.
 * - Render 3D objects, both filled and using wireframe.
 * - Use mesh optimizer to optimize the mesh data.
 */
int main()
{
    Rndr::Init();
    Run();
    Rndr::Destroy();
}

const char* g_shader_code_vertex = R"(
#version 460 core
layout(std140, binding = 0) uniform PerFrameData
{
	uniform mat4 MVP;
};
layout (location=0) in vec3 pos;
layout (location=0) out vec3 color;
void main()
{
	gl_Position = MVP * vec4(pos, 1.0);
	color = pos.xyz;
}
)";

static const char* g_shader_code_geometry = R"(
#version 460 core
layout( triangles ) in;
layout( triangle_strip, max_vertices = 3 ) out;
layout (location=0) in vec3 color[];
layout (location=0) out vec3 colors;
layout (location=1) out vec3 barycoords;
void main()
{
	const vec3 bc[3] = vec3[]
	(
		vec3(1.0, 0.0, 0.0),
		vec3(0.0, 1.0, 0.0),
		vec3(0.0, 0.0, 1.0)
	);
	for ( int i = 0; i < 3; i++ )
	{
		gl_Position = gl_in[i].gl_Position;
		colors = color[i];
		barycoords = bc[i];
		EmitVertex();
	}
	EndPrimitive();
}
)";

const char* g_shader_code_fragment = R"(
#version 460 core
layout (location=0) in vec3 colors;
layout (location=1) in vec3 barycoords;
layout (location=0) out vec4 out_FragColor;
float edgeFactor(float thickness)
{
	vec3 a3 = smoothstep( vec3( 0.0 ), fwidth(barycoords) * thickness, barycoords);
	return min( min( a3.x, a3.y ), a3.z );
}
void main()
{
	out_FragColor = vec4( mix( vec3(0.0), colors, edgeFactor(1.0) ), 1.0 );
};
)";

struct PerFrameData
{
    math::Matrix4x4 mvp;
};
constexpr size_t k_per_frame_size = sizeof(PerFrameData);

void Run()
{
    const aiScene* scene = aiImportFile(ASSET_DIR "/scene.gltf", aiProcess_Triangulate);
    assert(scene != nullptr && scene->HasMeshes());
    const aiMesh* mesh = scene->mMeshes[0];
    Rndr::Array<math::Point3> positions;
    for (unsigned i = 0; i != mesh->mNumVertices; i++)
    {
        const aiVector3D v = mesh->mVertices[i];
        positions.emplace_back(v.x, v.y, v.z);
    }
    Rndr::Array<uint32_t> indices;
    for (uint32_t i = 0; i != mesh->mNumFaces; i++)
    {
        for (uint32_t j = 0; j != 3; j++)
        {
            indices.push_back(mesh->mFaces[i].mIndices[j]);
        }
    }
    aiReleaseImport(scene);

    Rndr::Array<uint32_t> indices_lod;
    {
        // Reindex the vertex buffer so that we remove redundant vertices.
        Rndr::Array<uint32_t> remap(indices.size());
        const size_t vertex_count = meshopt_generateVertexRemap(remap.data(),
                                                                indices.data(),
                                                                indices.size(),
                                                                positions.data(),
                                                                indices.size(),
                                                                sizeof(math::Point3));
        Rndr::Array<uint32_t> remapped_indices(indices.size());
        Rndr::Array<math::Point3> remapped_vertices(vertex_count);
        meshopt_remapIndexBuffer(remapped_indices.data(),
                                 indices.data(),
                                 indices.size(),
                                 remap.data());
        meshopt_remapVertexBuffer(remapped_vertices.data(),
                                  positions.data(),
                                  positions.size(),
                                  sizeof(math::Point3),
                                  remap.data());

        // Optimize the vertex cache by organizing the vertex data for same triangles to be close to
        // each other.
        meshopt_optimizeVertexCache(remapped_indices.data(),
                                    remapped_indices.data(),
                                    indices.size(),
                                    vertex_count);

        // Reduce overdraw to reduce for how many fragments we need to call fragment shader.
        meshopt_optimizeOverdraw(remapped_indices.data(),
                                 remapped_indices.data(),
                                 indices.size(),
                                 reinterpret_cast<float*>(remapped_vertices.data()),
                                 vertex_count,
                                 sizeof(math::Point3),
                                 1.05f);

        // Optimize vertex fetches by reordering the vertex buffer.
        meshopt_optimizeVertexFetch(remapped_vertices.data(),
                                    remapped_indices.data(),
                                    indices.size(),
                                    remapped_vertices.data(),
                                    vertex_count,
                                    sizeof(math::Point3));

        // Generate lower level LOD.
        constexpr float k_threshold = 0.2f;
        const size_t target_index_count =
            static_cast<size_t>(static_cast<float>(remapped_indices.size()) * k_threshold);
        constexpr float k_target_error = 1e-2f;
        indices_lod.resize(remapped_indices.size());
        indices_lod.resize(meshopt_simplify(indices_lod.data(),
                                            remapped_indices.data(),
                                            remapped_indices.size(),
                                            reinterpret_cast<float*>(remapped_vertices.data()),
                                            vertex_count,
                                            sizeof(math::Point3),
                                            target_index_count,
                                            k_target_error,
                                            0,
                                            nullptr));

        indices = remapped_indices;
        positions = remapped_vertices;
    }

    Rndr::Window window({.width = 1024, .height = 768, .name = "Mesh Optimizer Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    assert(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context,
                               {.width = window.GetWidth(), .height = window.GetHeight()});
    assert(swap_chain.IsValid());

    Rndr::Shader vertex_shader(graphics_context,
                               {.type = Rndr::ShaderType::Vertex, .source = g_shader_code_vertex});
    assert(vertex_shader.IsValid());
    Rndr::Shader geometry_shader(
        graphics_context,
        {.type = Rndr::ShaderType::Geometry, .source = g_shader_code_geometry});
    assert(geometry_shader.IsValid());
    Rndr::Shader pixel_shader(
        graphics_context,
        {.type = Rndr::ShaderType::Fragment, .source = g_shader_code_fragment});
    assert(pixel_shader.IsValid());

    const uint32_t size_indices = static_cast<uint32_t>(sizeof(uint32_t) * indices.size());
    const uint32_t size_indices_lod = static_cast<uint32_t>(sizeof(uint32_t) * indices_lod.size());
    const uint32_t size_vertices = static_cast<uint32_t>(sizeof(math::Point3) * positions.size());
    const uint32_t start_indices = 0;
    const uint32_t start_indices_lod = size_indices;
    //    const uint32_t start_vertices = size_indices + size_indices_lod;

    Rndr::Buffer vertex_buffer(graphics_context,
                               {.type = Rndr::BufferType::Vertex,
                                .usage = Rndr::Usage::Dynamic,
                                .size = size_vertices,
                                .stride = sizeof(math::Point3),
                                .offset = 0},
                               Rndr::ToByteSpan(positions));
    assert(vertex_buffer.IsValid());

    Rndr::Buffer index_buffer(graphics_context,
                              {.type = Rndr::BufferType::Index,
                               .usage = Rndr::Usage::Dynamic,
                               .size = size_indices + size_indices_lod,
                               .stride = sizeof(uint32_t),
                               .offset = 0});
    assert(index_buffer.IsValid());
    graphics_context.Update(index_buffer, Rndr::ToByteSpan(indices), start_indices);
    graphics_context.Update(index_buffer, Rndr::ToByteSpan(indices_lod), start_indices_lod);

    Rndr::InputLayoutBuilder builder;
    const Rndr::InputLayoutDesc input_layout_desc =
        builder.AddVertexBuffer(vertex_buffer, 0, Rndr::DataRepetition::PerVertex)
            .AppendElement(0, Rndr::PixelFormat::R32G32B32_FLOAT)
            .AddIndexBuffer(index_buffer)
            .Build();

    const Rndr::Pipeline solid_pipeline(graphics_context,
                                        {.vertex_shader = &vertex_shader,
                                         .pixel_shader = &pixel_shader,
                                         .geometry_shader = &geometry_shader,
                                         .input_layout = input_layout_desc,
                                         .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                         .depth_stencil = {.is_depth_enabled = true}});
    assert(solid_pipeline.IsValid());
    Rndr::Buffer per_frame_buffer(graphics_context,
                                  {.type = Rndr::BufferType::Constant,
                                   .usage = Rndr::Usage::Dynamic,
                                   .size = k_per_frame_size,
                                   .stride = k_per_frame_size});
    constexpr math::Vector4 k_clear_color{MATH_REALC(0.2),
                                          MATH_REALC(0.3),
                                          MATH_REALC(0.4),
                                          MATH_REALC(1.0)};

    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height)
                          { swap_chain.SetSize(width, height); });

    graphics_context.Bind(swap_chain);
    graphics_context.Bind(solid_pipeline);
    graphics_context.Bind(per_frame_buffer, 0);
    while (!window.IsClosed())
    {
        window.ProcessEvents();

        const float ratio = static_cast<Rndr::real>(window.GetWidth())
                            / static_cast<Rndr::real>(window.GetHeight());
        const float angle = std::fmod(10 * Rndr::GetSystemTime(), 360.0f);
        const math::Transform t1 = math::Translate(math::Vector3(-0.5f, -0.5f, -1.5f))
                                   * math::Rotate(angle, math::Vector3(0.0f, 1.0f, 0.0f))
                                   * math::RotateX(-90);
        const math::Transform t2 = math::Translate(math::Vector3(0.5f, -0.5f, -1.5f))
                                   * math::Rotate(angle, math::Vector3(0.0f, 1.0f, 0.0f))
                                   * math::RotateX(-90);
        const math::Matrix4x4 p = math::Perspective_RH_N1(45.0f, ratio, 0.1f, 1000.0f);
        math::Matrix4x4 mvp1 = math::Multiply(p, t1.GetMatrix());
        mvp1 = mvp1.Transpose();
        math::Matrix4x4 mvp2 = math::Multiply(p, t2.GetMatrix());
        mvp2 = mvp2.Transpose();

        graphics_context.ClearColorAndDepth(k_clear_color, 1);

        PerFrameData per_frame_data = {.mvp = mvp1};
        graphics_context.Update(per_frame_buffer, Rndr::ToByteSpan(per_frame_data));
        graphics_context.DrawIndices(Rndr::PrimitiveTopology::Triangle,
                                     static_cast<int32_t>(indices.size()));

        per_frame_data.mvp = mvp2;
        graphics_context.Update(per_frame_buffer, Rndr::ToByteSpan(per_frame_data));
        graphics_context.DrawIndices(Rndr::PrimitiveTopology::Triangle,
                                     static_cast<int32_t>(indices_lod.size()),
                                     1,
                                     static_cast<int32_t>(indices.size()));

        graphics_context.Present(swap_chain, true);
    }
}