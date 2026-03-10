#include <catch2/catch2.hpp>

#include "opal/container/scope-ptr.h"
#include "opal/exceptions.h"

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/shader.hpp"
#include "rndr/generic-window.hpp"

namespace
{

Rndr::Canvas::Context CreateTestContext(Opal::ScopePtr<Rndr::Application>& app, Opal::Ref<Rndr::GenericWindow>& window)
{
    app = Rndr::Application::Create();
    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    window = app->CreateGenericWindow(window_desc);
    return Rndr::Canvas::Context::Init(window->GetNativeHandle());
}

struct ShaderTestFixture
{
    Opal::ScopePtr<Rndr::Application> app;
    Opal::Ref<Rndr::GenericWindow> window;
    Rndr::Canvas::Context context;

    ShaderTestFixture() : context(CreateTestContext(app, window)) {}
};

const char* k_vertex_source = R"(
struct VSInput
{
    float3 position;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

[shader("vertex")]
VSOutput VertexMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    return output;
}
)";

const char* k_fragment_source = R"(
struct FSOutput
{
    float4 color : SV_TARGET;
};

[shader("fragment")]
FSOutput FragmentMain(float4 pos : SV_POSITION)
{
    FSOutput output;
    output.color = float4(1.0, 0.0, 0.0, 1.0);
    return output;
}
)";

const char* k_combined_source = R"(
struct VSInput
{
    float3 position;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

[shader("vertex")]
VSOutput VertexMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    return output;
}

struct FSOutput
{
    float4 color : SV_TARGET;
};

[shader("fragment")]
FSOutput FragmentMain(VSOutput input)
{
    FSOutput output;
    output.color = float4(1.0, 0.0, 0.0, 1.0);
    return output;
}
)";

const char* k_combined_with_params_source = R"(
Sampler2D diffuse_texture;

struct MaterialData
{
    float4 color;
    float roughness;
};

ConstantBuffer<MaterialData> material;

struct VSInput
{
    float3 position;
    float2 uv;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv;
};

[shader("vertex")]
VSOutput VertexMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    output.uv = input.uv;
    return output;
}

struct FSOutput
{
    float4 color : SV_TARGET;
};

[shader("fragment")]
FSOutput FragmentMain(VSOutput input)
{
    FSOutput output;
    output.color = diffuse_texture.Sample(input.uv) * material.color;
    return output;
}
)";

const char* k_combined_with_standalone_uniforms = R"(
float4x4 mvp;
float4 tint_color;

struct VSInput
{
    float3 position;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

[shader("vertex")]
VSOutput VertexMain(VSInput input)
{
    VSOutput output;
    output.position = mul(mvp, float4(input.position, 1.0));
    return output;
}

struct FSOutput
{
    float4 color : SV_TARGET;
};

[shader("fragment")]
FSOutput FragmentMain(VSOutput input)
{
    FSOutput output;
    output.color = tint_color;
    return output;
}
)";

const char* k_vertex_conflict_source = R"(
Sampler2D shared_resource;

struct VSInput
{
    float3 position;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

[shader("vertex")]
VSOutput VertexMain(VSInput input)
{
    VSOutput output;
    float4 tex_val = shared_resource.Sample(float2(0, 0));
    output.position = float4(input.position + tex_val.xyz, 1.0);
    return output;
}
)";

const char* k_fragment_conflict_source = R"(
struct SharedData
{
    float4 color;
};

ConstantBuffer<SharedData> shared_resource;

struct FSOutput
{
    float4 color : SV_TARGET;
};

[shader("fragment")]
FSOutput FragmentMain(float4 pos : SV_POSITION)
{
    FSOutput output;
    output.color = shared_resource.color;
    return output;
}
)";

const char* k_vertex_only_source = R"(
struct VSInput
{
    float3 position;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

[shader("vertex")]
VSOutput VertexMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    return output;
}
)";

const char* k_two_vertex_source = R"(
struct VSInput
{
    float3 position;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

[shader("vertex")]
VSOutput VertexA(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    return output;
}

[shader("vertex")]
VSOutput VertexB(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    return output;
}

struct FSOutput
{
    float4 color : SV_TARGET;
};

[shader("fragment")]
FSOutput FragmentMain(VSOutput input)
{
    FSOutput output;
    output.color = float4(1.0, 0.0, 0.0, 1.0);
    return output;
}
)";

const char* k_parameter_block_source = R"(
struct LightData
{
    float4 direction;
    float4 color;
    float intensity;
};

ParameterBlock<LightData> light;

struct VSInput
{
    float3 position;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

[shader("vertex")]
VSOutput VertexMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position * light.intensity, 1.0);
    return output;
}

struct FSOutput
{
    float4 color : SV_TARGET;
};

[shader("fragment")]
FSOutput FragmentMain(VSOutput input)
{
    FSOutput output;
    output.color = light.color;
    return output;
}
)";

const char* k_compute_source = R"(
RWStructuredBuffer<float4> output_buffer;

struct ComputeParams
{
    uint count;
};

ConstantBuffer<ComputeParams> params;

[shader("compute")]
[numthreads(64, 1, 1)]
void ComputeMain(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x < params.count)
    {
        output_buffer[tid.x] = float4(float(tid.x), 0, 0, 1);
    }
}
)";

const char* k_mixed_compute_graphics_source = R"(
struct VSInput
{
    float3 position;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

[shader("vertex")]
VSOutput VertexMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    return output;
}

RWStructuredBuffer<float4> output_buffer;

[shader("compute")]
[numthreads(64, 1, 1)]
void ComputeMain(uint3 tid : SV_DispatchThreadID)
{
    output_buffer[tid.x] = float4(0, 0, 0, 1);
}
)";

}  // namespace

TEST_CASE("Canvas Shader", "[canvas][shader]")
{
    ShaderTestFixture const f;

    SECTION("Default constructed shader is invalid")
    {
        Rndr::Canvas::Shader const shader;
        REQUIRE_FALSE(shader.IsValid());
    }

    SECTION("Create shader from combined source")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source);
        REQUIRE(shader.IsValid());
        REQUIRE(shader.GetNativeHandle() != 0);
    }

    SECTION("Create shader from separate sources")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourcesInMemory(k_vertex_source, k_fragment_source);
        REQUIRE(shader.IsValid());
        REQUIRE(shader.GetNativeHandle() != 0);
    }

    SECTION("Empty source throws")
    {
        REQUIRE_THROWS(Rndr::Canvas::Shader::FromSourceInMemory(""));
    }

    SECTION("Source with no vertex entry point throws")
    {
        REQUIRE_THROWS(Rndr::Canvas::Shader::FromSourcesInMemory(k_fragment_source, k_fragment_source));
    }

    SECTION("Source with no fragment entry point throws")
    {
        REQUIRE_THROWS(Rndr::Canvas::Shader::FromSourceInMemory(k_vertex_only_source));
    }

    SECTION("Source with multiple vertex entry points throws")
    {
        REQUIRE_THROWS(Rndr::Canvas::Shader::FromSourceInMemory(k_two_vertex_source));
    }

    SECTION("FromSource with file on disk")
    {
        const char* tmp_path = "shader_test_tmp.slang";
        FILE* tmp = nullptr;
#if defined(RNDR_WINDOWS)
        fopen_s(&tmp, tmp_path, "wb");
#else
        tmp = fopen(tmp_path, "wb");
#endif
        REQUIRE(tmp != nullptr);
        fwrite(k_combined_source, 1, strlen(k_combined_source), tmp);
        fclose(tmp);

        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSource(tmp_path);
        REQUIRE(shader.IsValid());

        remove(tmp_path);
    }

    SECTION("FromSources with files on disk")
    {
        const char* vs_path = "shader_test_vs_tmp.slang";
        const char* fs_path = "shader_test_fs_tmp.slang";
        FILE* vs_file = nullptr;
        FILE* fs_file = nullptr;
#if defined(RNDR_WINDOWS)
        fopen_s(&vs_file, vs_path, "wb");
        fopen_s(&fs_file, fs_path, "wb");
#else
        vs_file = fopen(vs_path, "wb");
        fs_file = fopen(fs_path, "wb");
#endif
        REQUIRE(vs_file != nullptr);
        REQUIRE(fs_file != nullptr);
        fwrite(k_vertex_source, 1, strlen(k_vertex_source), vs_file);
        fclose(vs_file);
        fwrite(k_fragment_source, 1, strlen(k_fragment_source), fs_file);
        fclose(fs_file);

        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSources(vs_path, fs_path);
        REQUIRE(shader.IsValid());

        remove(vs_path);
        remove(fs_path);
    }

    SECTION("FromSource with non-existent file throws")
    {
        REQUIRE_THROWS(Rndr::Canvas::Shader::FromSource("non_existent_file.slang"));
    }

    SECTION("Destroy makes shader invalid")
    {
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source);
        REQUIRE(shader.IsValid());
        shader.Destroy();
        REQUIRE_FALSE(shader.IsValid());
    }

    SECTION("Move constructor")
    {
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source);
        REQUIRE(shader.IsValid());
        const Rndr::u32 handle = shader.GetNativeHandle();

        Rndr::Canvas::Shader const moved(std::move(shader));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetNativeHandle() == handle);
        REQUIRE_FALSE(shader.IsValid());
    }

    SECTION("Move assignment")
    {
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source);
        Rndr::Canvas::Shader other;

        other = std::move(shader);
        REQUIRE(other.IsValid());
        REQUIRE_FALSE(shader.IsValid());
    }

    SECTION("Clone")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source);
        REQUIRE(shader.IsValid());

        Rndr::Canvas::Shader const clone = shader.Clone();
        REQUIRE(clone.IsValid());
        // Original still valid.
        REQUIRE(shader.IsValid());
        // Different native handles.
        REQUIRE(clone.GetNativeHandle() != shader.GetNativeHandle());
    }

    SECTION("Clone of invalid shader returns invalid")
    {
        Rndr::Canvas::Shader const shader;
        Rndr::Canvas::Shader const clone = shader.Clone();
        REQUIRE_FALSE(clone.IsValid());
    }

    SECTION("Two shaders from same source have different handles")
    {
        Rndr::Canvas::Shader const a = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source);
        Rndr::Canvas::Shader const b = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source);
        REQUIRE(a.IsValid());
        REQUIRE(b.IsValid());
        REQUIRE(a.GetNativeHandle() != b.GetNativeHandle());
    }

    SECTION("Vertex input parameters are in merged reflection")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source);
        REQUIRE(shader.IsValid());
        REQUIRE_FALSE(shader.GetParameters().IsEmpty());

        const Rndr::Canvas::ShaderParameter* input = shader.FindParameter("input");
        REQUIRE(input != nullptr);
        REQUIRE(input->category == Rndr::Canvas::ParameterCategory::VaryingInput);
    }

    SECTION("Fragment resources are in merged reflection")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_with_params_source);
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::ShaderParameter* texture = shader.FindParameter("diffuse_texture");
        REQUIRE(texture != nullptr);
        REQUIRE(texture->category == Rndr::Canvas::ParameterCategory::Texture);

        const Rndr::Canvas::ShaderParameter* mat = shader.FindParameter("material");
        REQUIRE(mat != nullptr);
        REQUIRE(mat->category == Rndr::Canvas::ParameterCategory::Uniform);
    }

    SECTION("Constant buffer fields are extracted in merged reflection")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_with_params_source);
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::ShaderParameter* mat = shader.FindParameter("material");
        REQUIRE(mat != nullptr);

        const Rndr::Canvas::ShaderParameter* color = shader.FindParameter("color");
        REQUIRE(color != nullptr);
        REQUIRE(color->category == Rndr::Canvas::ParameterCategory::Uniform);
        REQUIRE(color->offset == 0);
        REQUIRE(color->size == 16);  // float4 = 16 bytes
        REQUIRE(color->binding_index == mat->binding_index);

        const Rndr::Canvas::ShaderParameter* roughness = shader.FindParameter("roughness");
        REQUIRE(roughness != nullptr);
        REQUIRE(roughness->category == Rndr::Canvas::ParameterCategory::Uniform);
        REQUIRE(roughness->offset == 16);  // After float4 color.
        REQUIRE(roughness->size == 4);     // float = 4 bytes
    }

    SECTION("Standalone uniforms are extracted as individual parameters")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_with_standalone_uniforms);
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::ShaderParameter* mvp = shader.FindParameter("mvp");
        REQUIRE(mvp != nullptr);
        REQUIRE(mvp->category == Rndr::Canvas::ParameterCategory::Uniform);

        const Rndr::Canvas::ShaderParameter* tint = shader.FindParameter("tint_color");
        REQUIRE(tint != nullptr);
        REQUIRE(tint->category == Rndr::Canvas::ParameterCategory::Uniform);
    }

    SECTION("ParameterBlock fields are extracted")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_parameter_block_source);
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::ShaderParameter* light = shader.FindParameter("light");
        REQUIRE(light != nullptr);
        REQUIRE(light->category == Rndr::Canvas::ParameterCategory::Uniform);

        const Rndr::Canvas::ShaderParameter* direction = shader.FindParameter("direction");
        REQUIRE(direction != nullptr);
        REQUIRE(direction->category == Rndr::Canvas::ParameterCategory::Uniform);
        REQUIRE(direction->offset == 0);
        REQUIRE(direction->size == 16);  // float4 = 16 bytes
        REQUIRE(direction->binding_index == light->binding_index);

        const Rndr::Canvas::ShaderParameter* color = shader.FindParameter("color");
        REQUIRE(color != nullptr);
        REQUIRE(color->category == Rndr::Canvas::ParameterCategory::Uniform);
        REQUIRE(color->offset == 16);  // After float4 direction.
        REQUIRE(color->size == 16);    // float4 = 16 bytes

        const Rndr::Canvas::ShaderParameter* intensity = shader.FindParameter("intensity");
        REQUIRE(intensity != nullptr);
        REQUIRE(intensity->category == Rndr::Canvas::ParameterCategory::Uniform);
        REQUIRE(intensity->offset == 32);  // After two float4s.
        REQUIRE(intensity->size == 4);     // float = 4 bytes
    }

    SECTION("Conflicting parameter types between stages throws")
    {
        REQUIRE_THROWS(Rndr::Canvas::Shader::FromSourcesInMemory(k_vertex_conflict_source, k_fragment_conflict_source));
    }

    SECTION("FindParameter returns nullptr for non-existent name")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source);
        REQUIRE(shader.FindParameter("does_not_exist") == nullptr);
    }

    SECTION("Default constructed shader has no parameters")
    {
        Rndr::Canvas::Shader const shader;
        REQUIRE(shader.GetParameters().IsEmpty());
        REQUIRE(shader.FindParameter("anything") == nullptr);
    }

    SECTION("Destroy clears parameters")
    {
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_with_params_source);
        REQUIRE_FALSE(shader.GetParameters().IsEmpty());
        shader.Destroy();
        REQUIRE(shader.GetParameters().IsEmpty());
    }

    SECTION("Move transfers parameters")
    {
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_with_params_source);
        const auto param_count = shader.GetParameters().GetSize();
        REQUIRE(param_count > 0);

        Rndr::Canvas::Shader const moved(std::move(shader));
        REQUIRE(moved.GetParameters().GetSize() == param_count);
        REQUIRE(moved.FindParameter("diffuse_texture") != nullptr);
    }

    SECTION("Clone preserves parameters")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_with_params_source);
        Rndr::Canvas::Shader const clone = shader.Clone();
        REQUIRE(clone.GetParameters().GetSize() == shader.GetParameters().GetSize());
        REQUIRE(clone.FindParameter("diffuse_texture") != nullptr);
        REQUIRE(clone.FindParameter("material") != nullptr);
    }

    SECTION("Create compute shader from single source")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_compute_source);
        REQUIRE(shader.IsValid());
        REQUIRE(shader.GetNativeHandle() != 0);
    }

    SECTION("Compute shader reflection includes resources")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_compute_source);
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::ShaderParameter* buffer = shader.FindParameter("output_buffer");
        REQUIRE(buffer != nullptr);
        REQUIRE(buffer->category == Rndr::Canvas::ParameterCategory::StorageBuffer);

        const Rndr::Canvas::ShaderParameter* cb = shader.FindParameter("params");
        REQUIRE(cb != nullptr);
        REQUIRE(cb->category == Rndr::Canvas::ParameterCategory::Uniform);

        const Rndr::Canvas::ShaderParameter* count = shader.FindParameter("count");
        REQUIRE(count != nullptr);
        REQUIRE(count->category == Rndr::Canvas::ParameterCategory::Uniform);
        REQUIRE(count->size == 4);  // uint = 4 bytes
    }

    SECTION("Compute shader clone")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_compute_source);
        REQUIRE(shader.IsValid());

        Rndr::Canvas::Shader const clone = shader.Clone();
        REQUIRE(clone.IsValid());
        REQUIRE(clone.GetNativeHandle() != shader.GetNativeHandle());
        REQUIRE(clone.FindParameter("output_buffer") != nullptr);
    }

    SECTION("Mixed compute and graphics entry points throws")
    {
        REQUIRE_THROWS(Rndr::Canvas::Shader::FromSourceInMemory(k_mixed_compute_graphics_source));
    }

    SECTION("GetVertexLayout from combined source with position only")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source);
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::VertexLayout& layout = shader.GetVertexLayout();
        REQUIRE(layout.IsValid());
        REQUIRE(layout.GetAttributeCount() == 1);
        REQUIRE(layout.GetStride() == 12);  // float3 = 12 bytes
        REQUIRE(layout.GetAttribute(0).attrib == Rndr::Canvas::Attrib::Position);
        REQUIRE(layout.GetAttribute(0).format == Rndr::Canvas::Format::Float3);
    }

    SECTION("GetVertexLayout from combined source with position and uv")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_with_params_source);
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::VertexLayout& layout = shader.GetVertexLayout();
        REQUIRE(layout.IsValid());
        REQUIRE(layout.GetAttributeCount() == 2);
        REQUIRE(layout.GetStride() == 20);  // float3 + float2 = 12 + 8 = 20 bytes
        REQUIRE(layout.GetAttribute(0).attrib == Rndr::Canvas::Attrib::Position);
        REQUIRE(layout.GetAttribute(0).format == Rndr::Canvas::Format::Float3);
        REQUIRE(layout.GetAttribute(1).attrib == Rndr::Canvas::Attrib::UV);
        REQUIRE(layout.GetAttribute(1).format == Rndr::Canvas::Format::Float2);
    }

    SECTION("GetVertexLayout from separate sources")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourcesInMemory(k_vertex_source, k_fragment_source);
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::VertexLayout& layout = shader.GetVertexLayout();
        REQUIRE(layout.IsValid());
        REQUIRE(layout.GetAttributeCount() == 1);
        REQUIRE(layout.GetStride() == 12);  // float3 = 12 bytes
    }

    SECTION("Compute shader GetNumThreads")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_compute_source);
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::NumThreads& num_threads = shader.GetNumThreads();
        REQUIRE(num_threads.x == 64);
        REQUIRE(num_threads.y == 1);
        REQUIRE(num_threads.z == 1);
    }

    SECTION("Graphics shader GetNumThreads is zero")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source);
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::NumThreads& num_threads = shader.GetNumThreads();
        REQUIRE(num_threads.x == 0);
        REQUIRE(num_threads.y == 0);
        REQUIRE(num_threads.z == 0);
    }

    SECTION("GetVertexLayout is empty for compute shader")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_compute_source);
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::VertexLayout& layout = shader.GetVertexLayout();
        REQUIRE_FALSE(layout.IsValid());
        REQUIRE(layout.GetAttributeCount() == 0);
    }

    SECTION("GetVertexLayout is empty for default constructed shader")
    {
        Rndr::Canvas::Shader const shader;
        const Rndr::Canvas::VertexLayout& layout = shader.GetVertexLayout();
        REQUIRE_FALSE(layout.IsValid());
    }

    SECTION("Move transfers vertex layout")
    {
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_with_params_source);
        REQUIRE(shader.GetVertexLayout().IsValid());

        Rndr::Canvas::Shader const moved(std::move(shader));
        REQUIRE(moved.GetVertexLayout().IsValid());
        REQUIRE(moved.GetVertexLayout().GetAttributeCount() == 2);
    }

    SECTION("Clone preserves vertex layout")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_with_params_source);
        REQUIRE(shader.GetVertexLayout().IsValid());

        Rndr::Canvas::Shader const clone = shader.Clone();
        REQUIRE(clone.GetVertexLayout().IsValid());
        REQUIRE(clone.GetVertexLayout().GetAttributeCount() == shader.GetVertexLayout().GetAttributeCount());
        REQUIRE(clone.GetVertexLayout().GetStride() == shader.GetVertexLayout().GetStride());
    }

    SECTION("Destroy clears vertex layout")
    {
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_with_params_source);
        REQUIRE(shader.GetVertexLayout().IsValid());
        shader.Destroy();
        REQUIRE_FALSE(shader.GetVertexLayout().IsValid());
    }
}
