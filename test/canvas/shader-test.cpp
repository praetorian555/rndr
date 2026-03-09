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
FSOutput FragmentMain()
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

const char* k_fragment_with_params_source = R"(
Sampler2D diffuse_texture;

struct MaterialData
{
    float4 color;
    float roughness;
};

ConstantBuffer<MaterialData> material;

struct FSOutput
{
    float4 color : SV_TARGET;
};

[shader("fragment")]
FSOutput FragmentMain(float4 pos : SV_POSITION, float2 uv : TEXCOORD)
{
    FSOutput output;
    output.color = diffuse_texture.Sample(uv) * material.color;
    return output;
}
)";

const char* k_standalone_uniform_source = R"(
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
)";

}  // namespace

TEST_CASE("Canvas ShaderType enum", "[canvas][shader]")
{
    constexpr auto count = static_cast<Rndr::u8>(Rndr::Canvas::ShaderType::EnumCount);
    REQUIRE(count == 3);
}

TEST_CASE("Canvas Shader", "[canvas][shader]")
{
    ShaderTestFixture const f;

    SECTION("Default constructed shader is invalid")
    {
        Rndr::Canvas::Shader const shader;
        REQUIRE_FALSE(shader.IsValid());
        REQUIRE(shader.GetType() == Rndr::Canvas::ShaderType::EnumCount);
    }

    SECTION("Create vertex shader from source in memory")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_vertex_source, "VertexMain");
        REQUIRE(shader.IsValid());
        REQUIRE(shader.GetType() == Rndr::Canvas::ShaderType::Vertex);
        REQUIRE(shader.GetNativeHandle() != 0);
    }

    SECTION("Create fragment shader from source in memory")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_fragment_source, "FragmentMain");
        REQUIRE(shader.IsValid());
        REQUIRE(shader.GetType() == Rndr::Canvas::ShaderType::Fragment);
        REQUIRE(shader.GetNativeHandle() != 0);
    }

    SECTION("Shader type is deduced from entry point annotation")
    {
        Rndr::Canvas::Shader const vs = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source, "VertexMain");
        Rndr::Canvas::Shader const fs = Rndr::Canvas::Shader::FromSourceInMemory(k_combined_source, "FragmentMain");
        REQUIRE(vs.GetType() == Rndr::Canvas::ShaderType::Vertex);
        REQUIRE(fs.GetType() == Rndr::Canvas::ShaderType::Fragment);
    }

    SECTION("Empty source throws")
    {
        REQUIRE_THROWS(Rndr::Canvas::Shader::FromSourceInMemory("", "VertexMain"));
    }

    SECTION("Invalid entry point throws")
    {
        REQUIRE_THROWS(Rndr::Canvas::Shader::FromSourceInMemory(k_vertex_source, "NonExistent"));
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
        fwrite(k_vertex_source, 1, strlen(k_vertex_source), tmp);
        fclose(tmp);

        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSource(tmp_path, "VertexMain");
        REQUIRE(shader.IsValid());
        REQUIRE(shader.GetType() == Rndr::Canvas::ShaderType::Vertex);

        remove(tmp_path);
    }

    SECTION("FromSource with non-existent file throws")
    {
        REQUIRE_THROWS(Rndr::Canvas::Shader::FromSource("non_existent_file.slang", "main"));
    }

    SECTION("Destroy makes shader invalid")
    {
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_vertex_source, "VertexMain");
        REQUIRE(shader.IsValid());
        shader.Destroy();
        REQUIRE_FALSE(shader.IsValid());
        REQUIRE(shader.GetType() == Rndr::Canvas::ShaderType::EnumCount);
    }

    SECTION("Move constructor")
    {
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_vertex_source, "VertexMain");
        REQUIRE(shader.IsValid());
        const Rndr::u32 handle = shader.GetNativeHandle();

        Rndr::Canvas::Shader const moved(std::move(shader));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetNativeHandle() == handle);
        REQUIRE(moved.GetType() == Rndr::Canvas::ShaderType::Vertex);
        REQUIRE_FALSE(shader.IsValid());
    }

    SECTION("Move assignment")
    {
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_fragment_source, "FragmentMain");
        Rndr::Canvas::Shader other;

        other = std::move(shader);
        REQUIRE(other.IsValid());
        REQUIRE(other.GetType() == Rndr::Canvas::ShaderType::Fragment);
        REQUIRE_FALSE(shader.IsValid());
    }

    SECTION("Clone")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_vertex_source, "VertexMain");
        REQUIRE(shader.IsValid());

        Rndr::Canvas::Shader const clone = shader.Clone();
        REQUIRE(clone.IsValid());
        REQUIRE(clone.GetType() == Rndr::Canvas::ShaderType::Vertex);
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
        Rndr::Canvas::Shader const a = Rndr::Canvas::Shader::FromSourceInMemory(k_vertex_source, "VertexMain");
        Rndr::Canvas::Shader const b = Rndr::Canvas::Shader::FromSourceInMemory(k_vertex_source, "VertexMain");
        REQUIRE(a.IsValid());
        REQUIRE(b.IsValid());
        REQUIRE(a.GetNativeHandle() != b.GetNativeHandle());
    }

    SECTION("Simple shader has vertex input parameter")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_vertex_source, "VertexMain");
        REQUIRE(shader.IsValid());
        REQUIRE_FALSE(shader.GetParameters().IsEmpty());

        const Rndr::Canvas::ShaderParameter* input = shader.FindParameter("input");
        REQUIRE(input != nullptr);
        REQUIRE(input->category == Rndr::Canvas::ParameterCategory::VaryingInput);
    }

    SECTION("Fragment shader with texture and uniform parameters")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_fragment_with_params_source, "FragmentMain");
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::ShaderParameter* texture = shader.FindParameter("diffuse_texture");
        REQUIRE(texture != nullptr);
        REQUIRE(texture->category == Rndr::Canvas::ParameterCategory::Texture);

        const Rndr::Canvas::ShaderParameter* material = shader.FindParameter("material");
        REQUIRE(material != nullptr);
        REQUIRE(material->category == Rndr::Canvas::ParameterCategory::Uniform);

        // Fields inside the ConstantBuffer are extracted individually.
        const Rndr::Canvas::ShaderParameter* color = shader.FindParameter("color");
        REQUIRE(color != nullptr);
        REQUIRE(color->category == Rndr::Canvas::ParameterCategory::Uniform);
        REQUIRE(color->offset == 0);
        REQUIRE(color->size == 16);  // float4 = 16 bytes
        // Same binding as parent buffer.
        REQUIRE(color->binding_index == material->binding_index);

        const Rndr::Canvas::ShaderParameter* roughness = shader.FindParameter("roughness");
        REQUIRE(roughness != nullptr);
        REQUIRE(roughness->category == Rndr::Canvas::ParameterCategory::Uniform);
        REQUIRE(roughness->offset == 16);  // After float4 color.
        REQUIRE(roughness->size == 4);     // float = 4 bytes
    }

    SECTION("Standalone uniforms are extracted as individual parameters")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_standalone_uniform_source, "VertexMain");
        REQUIRE(shader.IsValid());

        const Rndr::Canvas::ShaderParameter* mvp = shader.FindParameter("mvp");
        REQUIRE(mvp != nullptr);
        REQUIRE(mvp->category == Rndr::Canvas::ParameterCategory::Uniform);

        const Rndr::Canvas::ShaderParameter* tint = shader.FindParameter("tint_color");
        REQUIRE(tint != nullptr);
        REQUIRE(tint->category == Rndr::Canvas::ParameterCategory::Uniform);
    }

    SECTION("FindParameter returns nullptr for non-existent name")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_vertex_source, "VertexMain");
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
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_fragment_with_params_source, "FragmentMain");
        REQUIRE_FALSE(shader.GetParameters().IsEmpty());
        shader.Destroy();
        REQUIRE(shader.GetParameters().IsEmpty());
    }

    SECTION("Move transfers parameters")
    {
        Rndr::Canvas::Shader shader = Rndr::Canvas::Shader::FromSourceInMemory(k_fragment_with_params_source, "FragmentMain");
        const auto param_count = shader.GetParameters().GetSize();
        REQUIRE(param_count > 0);

        Rndr::Canvas::Shader const moved(std::move(shader));
        REQUIRE(moved.GetParameters().GetSize() == param_count);
        REQUIRE(moved.FindParameter("diffuse_texture") != nullptr);
    }

    SECTION("Clone preserves parameters")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_fragment_with_params_source, "FragmentMain");
        Rndr::Canvas::Shader const clone = shader.Clone();
        REQUIRE(clone.GetParameters().GetSize() == shader.GetParameters().GetSize());
        REQUIRE(clone.FindParameter("diffuse_texture") != nullptr);
        REQUIRE(clone.FindParameter("material") != nullptr);
    }
}
