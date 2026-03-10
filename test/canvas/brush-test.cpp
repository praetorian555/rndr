#include <catch2/catch2.hpp>

#include "opal/container/scope-ptr.h"

#include "rndr/application.hpp"
#include "rndr/canvas/brush.hpp"
#include "rndr/canvas/buffer.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/shader.hpp"
#include "rndr/canvas/texture.hpp"
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

struct BrushTestFixture
{
    Opal::ScopePtr<Rndr::Application> app;
    Opal::Ref<Rndr::GenericWindow> window;
    Rndr::Canvas::Context context;

    BrushTestFixture() : context(CreateTestContext(app, window)) {}
};

const char* k_simple_shader = R"(
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

const char* k_uniform_shader = R"(
struct MaterialData
{
    float4 color;
    float roughness;
};

ConstantBuffer<MaterialData> material;

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
    output.color = material.color * material.roughness;
    return output;
}
)";

const char* k_standalone_uniform_shader = R"(
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

}  // namespace

TEST_CASE("Canvas BrushDesc defaults", "[canvas][brush]")
{
    Rndr::Canvas::BrushDesc desc;
    REQUIRE(desc.blend_mode == Rndr::Canvas::BlendMode::None);
    REQUIRE(desc.depth_test == false);
    REQUIRE(desc.depth_write == true);
    REQUIRE(desc.depth_compare == Rndr::Canvas::CompareFunc::Less);
    REQUIRE(desc.cull_mode == Rndr::Canvas::CullMode::Back);
    REQUIRE(desc.fill_mode == Rndr::Canvas::FillMode::Solid);
    REQUIRE(desc.depth_bias_factor == 0.0f);
    REQUIRE(desc.depth_bias_units == 0.0f);
}

TEST_CASE("Canvas Brush", "[canvas][brush]")
{
    BrushTestFixture const f;

    SECTION("Default constructed brush is invalid")
    {
        Rndr::Canvas::Brush const brush;
        REQUIRE_FALSE(brush.IsValid());
        REQUIRE(brush.GetShader() == nullptr);
    }

    SECTION("Brush with desc but no shader is invalid")
    {
        Rndr::Canvas::BrushDesc desc;
        desc.depth_test = true;
        Rndr::Canvas::Brush const brush(desc);
        REQUIRE_FALSE(brush.IsValid());
    }

    SECTION("Brush with shader is valid")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_simple_shader);
        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);
        REQUIRE(brush.IsValid());
        REQUIRE(brush.GetShader() == &shader);
    }

    SECTION("Construct from BrushDesc")
    {
        Rndr::Canvas::BrushDesc desc;
        desc.blend_mode = Rndr::Canvas::BlendMode::Alpha;
        desc.depth_test = true;
        desc.depth_write = false;
        desc.depth_compare = Rndr::Canvas::CompareFunc::LessEqual;
        desc.cull_mode = Rndr::Canvas::CullMode::Front;
        desc.fill_mode = Rndr::Canvas::FillMode::Wireframe;
        desc.depth_bias_factor = 1.0f;
        desc.depth_bias_units = 2.0f;

        Rndr::Canvas::Brush const brush(desc);
        const Rndr::Canvas::BrushDesc& result = brush.GetDesc();
        REQUIRE(result.blend_mode == Rndr::Canvas::BlendMode::Alpha);
        REQUIRE(result.depth_test == true);
        REQUIRE(result.depth_write == false);
        REQUIRE(result.depth_compare == Rndr::Canvas::CompareFunc::LessEqual);
        REQUIRE(result.cull_mode == Rndr::Canvas::CullMode::Front);
        REQUIRE(result.fill_mode == Rndr::Canvas::FillMode::Wireframe);
        REQUIRE(result.depth_bias_factor == 1.0f);
        REQUIRE(result.depth_bias_units == 2.0f);
    }

    SECTION("Individual setters update desc")
    {
        Rndr::Canvas::Brush brush;

        brush.SetBlendMode(Rndr::Canvas::BlendMode::Additive);
        REQUIRE(brush.GetDesc().blend_mode == Rndr::Canvas::BlendMode::Additive);

        brush.SetDepthTest(true);
        REQUIRE(brush.GetDesc().depth_test == true);

        brush.SetDepthWrite(false);
        REQUIRE(brush.GetDesc().depth_write == false);

        brush.SetDepthCompare(Rndr::Canvas::CompareFunc::GreaterEqual);
        REQUIRE(brush.GetDesc().depth_compare == Rndr::Canvas::CompareFunc::GreaterEqual);

        brush.SetCullMode(Rndr::Canvas::CullMode::None);
        REQUIRE(brush.GetDesc().cull_mode == Rndr::Canvas::CullMode::None);

        brush.SetFillMode(Rndr::Canvas::FillMode::Wireframe);
        REQUIRE(brush.GetDesc().fill_mode == Rndr::Canvas::FillMode::Wireframe);

        brush.SetDepthBias(1.5f, 3.0f);
        REQUIRE(brush.GetDesc().depth_bias_factor == 1.5f);
        REQUIRE(brush.GetDesc().depth_bias_units == 3.0f);
    }

    SECTION("SetUniform stores value by name")
    {
        Rndr::Canvas::Brush brush;
        const float value = 42.0f;
        brush.SetUniform("brightness", value);

        REQUIRE(brush.GetUniforms().GetSize() == 1);
        REQUIRE(brush.GetUniforms()[0].name == "brightness");
        REQUIRE(brush.GetUniforms()[0].data.GetSize() == sizeof(float));

        float stored = 0.0f;
        memcpy(&stored, brush.GetUniforms()[0].data.GetData(), sizeof(float));
        REQUIRE(stored == 42.0f);
    }

    SECTION("SetUniform updates existing binding")
    {
        Rndr::Canvas::Brush brush;
        brush.SetUniform("value", 1.0f);
        brush.SetUniform("value", 2.0f);

        REQUIRE(brush.GetUniforms().GetSize() == 1);

        float stored = 0.0f;
        memcpy(&stored, brush.GetUniforms()[0].data.GetData(), sizeof(float));
        REQUIRE(stored == 2.0f);
    }

    SECTION("SetUniform with struct type")
    {
        struct MVP
        {
            float data[16];
        };
        MVP mvp = {};
        mvp.data[0] = 1.0f;
        mvp.data[5] = 1.0f;
        mvp.data[10] = 1.0f;
        mvp.data[15] = 1.0f;

        Rndr::Canvas::Brush brush;
        brush.SetUniform("mvp", mvp);

        REQUIRE(brush.GetUniforms().GetSize() == 1);
        REQUIRE(brush.GetUniforms()[0].data.GetSize() == sizeof(MVP));

        MVP stored = {};
        memcpy(&stored, brush.GetUniforms()[0].data.GetData(), sizeof(MVP));
        REQUIRE(stored.data[0] == 1.0f);
        REQUIRE(stored.data[15] == 1.0f);
    }

    SECTION("Multiple uniforms")
    {
        Rndr::Canvas::Brush brush;
        brush.SetUniform("alpha", 0.5f);
        brush.SetUniform("scale", 2.0f);
        brush.SetUniform("offset", 10);

        REQUIRE(brush.GetUniforms().GetSize() == 3);
        REQUIRE(brush.GetUniforms()[0].name == "alpha");
        REQUIRE(brush.GetUniforms()[1].name == "scale");
        REQUIRE(brush.GetUniforms()[2].name == "offset");
    }

    SECTION("SetTexture stores binding")
    {
        Rndr::Canvas::Brush brush;

        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 2;
        tex_desc.height = 2;
        tex_desc.format = Rndr::Canvas::Format::RGBA8;
        Rndr::Canvas::Texture tex(f.context, tex_desc);

        brush.SetTexture("diffuse", tex);

        REQUIRE(brush.GetTextures().GetSize() == 1);
        REQUIRE(brush.GetTextures()[0].name == "diffuse");
        REQUIRE(brush.GetTextures()[0].texture == &tex);
    }

    SECTION("SetTexture updates existing binding")
    {
        Rndr::Canvas::Brush brush;

        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 2;
        tex_desc.height = 2;
        tex_desc.format = Rndr::Canvas::Format::RGBA8;
        Rndr::Canvas::Texture tex1(f.context, tex_desc);
        Rndr::Canvas::Texture tex2(f.context, tex_desc);

        brush.SetTexture("diffuse", tex1);
        brush.SetTexture("diffuse", tex2);

        REQUIRE(brush.GetTextures().GetSize() == 1);
        REQUIRE(brush.GetTextures()[0].texture == &tex2);
    }

    SECTION("SetBuffer stores binding")
    {
        Rndr::Canvas::Brush brush;
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Storage, 64);

        brush.SetBuffer("data", buf);

        REQUIRE(brush.GetBuffers().GetSize() == 1);
        REQUIRE(brush.GetBuffers()[0].name == "data");
        REQUIRE(brush.GetBuffers()[0].buffer == &buf);
    }

    SECTION("Move transfers bindings")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_simple_shader);

        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);
        brush.SetUniform("alpha", 0.5f);

        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 2;
        tex_desc.height = 2;
        tex_desc.format = Rndr::Canvas::Format::RGBA8;
        Rndr::Canvas::Texture tex(f.context, tex_desc);
        brush.SetTexture("diffuse", tex);

        Rndr::Canvas::Brush const moved(std::move(brush));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetUniforms().GetSize() == 1);
        REQUIRE(moved.GetUniforms()[0].name == "alpha");
        REQUIRE(moved.GetTextures().GetSize() == 1);
        REQUIRE(moved.GetTextures()[0].name == "diffuse");
        REQUIRE_FALSE(brush.IsValid());
    }

    SECTION("Clone preserves bindings")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_simple_shader);

        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);
        brush.SetUniform("brightness", 0.8f);

        Rndr::Canvas::Brush const clone = brush.Clone();
        REQUIRE(clone.IsValid());
        REQUIRE(clone.GetUniforms().GetSize() == 1);
        REQUIRE(clone.GetUniforms()[0].name == "brightness");

        float stored = 0.0f;
        memcpy(&stored, clone.GetUniforms()[0].data.GetData(), sizeof(float));
        REQUIRE(stored == 0.8f);

        // Original still valid.
        REQUIRE(brush.IsValid());
        REQUIRE(brush.GetUniforms().GetSize() == 1);
    }

    SECTION("Move constructor transfers desc")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_simple_shader);

        Rndr::Canvas::BrushDesc desc;
        desc.depth_test = true;
        desc.cull_mode = Rndr::Canvas::CullMode::None;
        Rndr::Canvas::Brush brush(desc);
        brush.SetShader(shader);

        Rndr::Canvas::Brush const moved(std::move(brush));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetDesc().depth_test == true);
        REQUIRE(moved.GetDesc().cull_mode == Rndr::Canvas::CullMode::None);
        REQUIRE_FALSE(brush.IsValid());
    }

    SECTION("Move assignment transfers desc")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_simple_shader);

        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);
        brush.SetDepthTest(true);

        Rndr::Canvas::Brush other;
        other = std::move(brush);
        REQUIRE(other.IsValid());
        REQUIRE(other.GetDesc().depth_test == true);
        REQUIRE_FALSE(brush.IsValid());
    }

    SECTION("Clone preserves desc and shader")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_simple_shader);

        Rndr::Canvas::BrushDesc desc;
        desc.blend_mode = Rndr::Canvas::BlendMode::Multiply;
        desc.fill_mode = Rndr::Canvas::FillMode::Wireframe;
        Rndr::Canvas::Brush brush(desc);
        brush.SetShader(shader);

        Rndr::Canvas::Brush const clone = brush.Clone();
        REQUIRE(clone.IsValid());
        REQUIRE(clone.GetDesc().blend_mode == Rndr::Canvas::BlendMode::Multiply);
        REQUIRE(clone.GetDesc().fill_mode == Rndr::Canvas::FillMode::Wireframe);
        REQUIRE(brush.IsValid());
    }

    SECTION("Default brush has empty bindings")
    {
        Rndr::Canvas::Brush const brush;
        REQUIRE(brush.GetUniforms().IsEmpty());
        REQUIRE(brush.GetTextures().IsEmpty());
        REQUIRE(brush.GetBuffers().IsEmpty());
        REQUIRE(brush.GetUniformBufferSlots().IsEmpty());
    }

    SECTION("SetShader creates UBO slots for ConstantBuffer")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_uniform_shader);
        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);

        REQUIRE(brush.GetUniformBufferSlots().GetSize() == 1);
        REQUIRE(brush.GetUniformBufferSlots()[0].gpu_buffer.IsValid());
        REQUIRE(brush.GetUniformBufferSlots()[0].dirty == false);
    }

    SECTION("SetShader creates UBO slots for standalone uniforms")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_standalone_uniform_shader);
        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);

        REQUIRE(brush.GetUniformBufferSlots().GetSize() >= 1);
        REQUIRE(brush.GetUniformBufferSlots()[0].gpu_buffer.IsValid());
    }

    SECTION("SetShader with no uniforms creates no UBO slots")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_simple_shader);
        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);

        REQUIRE(brush.GetUniformBufferSlots().IsEmpty());
    }

    SECTION("SetUniform writes to UBO slot when shader is set")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_uniform_shader);
        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);

        const float roughness = 0.75f;
        brush.SetUniform("roughness", roughness);

        REQUIRE(brush.GetUniformBufferSlots()[0].dirty == true);
        // Value went to UBO slot, not to m_uniforms.
        REQUIRE(brush.GetUniforms().IsEmpty());
    }

    SECTION("SetUniform for unknown param falls back to m_uniforms")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_uniform_shader);
        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);

        const float unknown = 1.0f;
        brush.SetUniform("unknown_param", unknown);

        REQUIRE(brush.GetUniforms().GetSize() == 1);
        REQUIRE(brush.GetUniforms()[0].name == "unknown_param");
    }

    SECTION("UploadUniforms clears dirty flag")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_uniform_shader);
        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);

        const float roughness = 0.75f;
        brush.SetUniform("roughness", roughness);
        REQUIRE(brush.GetUniformBufferSlots()[0].dirty == true);

        brush.UploadUniforms();
        REQUIRE(brush.GetUniformBufferSlots()[0].dirty == false);
    }

    SECTION("Move transfers UBO slots")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_uniform_shader);
        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);

        REQUIRE(brush.GetUniformBufferSlots().GetSize() == 1);

        const Rndr::Canvas::Brush moved(std::move(brush));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetUniformBufferSlots().GetSize() == 1);
        REQUIRE(moved.GetUniformBufferSlots()[0].gpu_buffer.IsValid());
    }

    SECTION("Clone deep-copies UBO slots")
    {
        Rndr::Canvas::Shader const shader = Rndr::Canvas::Shader::FromSourceInMemory(k_uniform_shader);
        Rndr::Canvas::Brush brush;
        brush.SetShader(shader);

        const float roughness = 0.5f;
        brush.SetUniform("roughness", roughness);

        Rndr::Canvas::Brush const clone = brush.Clone();
        REQUIRE(clone.GetUniformBufferSlots().GetSize() == 1);
        REQUIRE(clone.GetUniformBufferSlots()[0].gpu_buffer.IsValid());
        REQUIRE(clone.GetUniformBufferSlots()[0].dirty == true);

        // Original still valid.
        REQUIRE(brush.IsValid());
        REQUIRE(brush.GetUniformBufferSlots().GetSize() == 1);
    }
}
