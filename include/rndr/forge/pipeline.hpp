#pragma once

#include "volk/volk.h"

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/ref.h"

#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"

namespace Rndr::Forge
{

struct PushConstantRange
{
    ShaderTypeBits shader_stages = ShaderTypeBits::AllGraphics;
    u32 offset = 0;
    u32 size = 0;
};

struct VertexInputDesc : Opal::ClonableBase<VertexInputDesc>
{
    struct Attribute
    {
        u32 location = 0;
        PixelFormat format = PixelFormat::Undefined;
        u32 offset = 0;
    };

    struct Binding : Opal::ClonableBase<Binding>
    {
        u32 binding = 0;
        u32 stride = 0;
        DataRepetition input_rate = DataRepetition::PerVertex;
        Opal::DynamicArray<Attribute> attributes;
        OPAL_CLONE_FIELDS(binding, stride, input_rate, attributes);
    };

    Opal::DynamicArray<Binding> bindings;

    OPAL_CLONE_FIELDS(bindings);

    Binding& AddBinding(u32 binding, u32 stride, DataRepetition input_rate = DataRepetition::PerVertex);
    void AddAttribute(u32 binding, u32 location, PixelFormat format, u32 offset);

    /**
     * One binding holding every attribute the vertex shader reads, in location order.
     *
     * Reflection has the location and the format of each attribute but not its offset, and not the stride -
     * those describe the vertex struct on the CPU side and appear nowhere in the SPIR-V. This fills them in
     * by packing the attributes tightly in location order, which is the layout of a plain struct whose
     * members are declared in the order the shader reads them and nothing else. A vertex buffer laid out any
     * other way needs the desc written by hand; the pipeline checks either one against the shader.
     *
     * @param vertex_shader Shader to read the attributes from. A stage other than the vertex one throws,
     *                      since no other stage is fed from a vertex buffer.
     * @param binding Index of the single binding this produces.
     * @param input_rate Whether the binding advances per vertex or per instance.
     */
    [[nodiscard]] static VertexInputDesc FromShader(const Shader& vertex_shader, u32 binding = 0,
                                                    DataRepetition input_rate = DataRepetition::PerVertex);
};

/**
 * The push constant ranges the given shaders read, merged. A block declared by two stages becomes one range
 * naming both, which is what Vulkan wants and what a vertex and fragment shader sharing a block produce.
 *
 * Saves repeating an offset and a size that the shader already fixed. A graphics pipeline given ranges of
 * its own checks them against the same reflection either way.
 */
[[nodiscard]] Opal::DynamicArray<PushConstantRange> PushConstantRangesFromShaders(
    Opal::ArrayView<const Opal::Ref<const Shader>> shaders);

struct RasterizerDesc
{
    /** Whether primitives are filled or drawn as wireframe. */
    FillMode fill_mode = FillMode::Solid;

    /** Which face to cull. None disables culling. */
    Face cull_mode = Face::Back;

    /** Winding order that defines the front face of a triangle. */
    WindingOrder front_face = WindingOrder::CCW;

    /** Enables depth bias (polygon offset) for rendered fragments. Useful for shadow mapping. */
    bool depth_bias_enabled = false;

    /** Constant value added to each fragment's depth. Scaled by the implementation's minimum resolvable depth difference. */
    f32 depth_bias_constant_factor = 0.0f;

    /** Bias value scaled by the fragment's slope in screen space. Steeper polygons get a larger bias. */
    f32 depth_bias_slope_factor = 0.0f;

    /** Maximum absolute depth bias value. Set to 0 to disable clamping. */
    f32 depth_bias_clamp = 0.0f;
};

struct DepthStencilDesc
{
    /** Enables depth testing. When disabled, all fragments pass the depth test. */
    bool depth_test_enabled = false;

    /** Enables writing to the depth buffer. Can be disabled while depth testing is still active. */
    bool depth_write_enabled = true;

    /** Comparison function used for the depth test. Fragment passes if its depth satisfies this comparison against the stored value. */
    Comparator depth_comparator = Comparator::Less;

    /** Enables stencil testing. */
    bool stencil_test_enabled = false;

    /** Operation performed when the stencil test fails for front-facing fragments. */
    StencilOperation front_stencil_fail = StencilOperation::Keep;

    /** Operation performed when stencil passes but depth fails for front-facing fragments. */
    StencilOperation front_depth_fail = StencilOperation::Keep;

    /** Operation performed when both stencil and depth tests pass for front-facing fragments. */
    StencilOperation front_pass = StencilOperation::Keep;

    /** Comparison function for the stencil test on front-facing fragments. */
    Comparator front_stencil_comparator = Comparator::Always;

    /** Operation performed when the stencil test fails for back-facing fragments. */
    StencilOperation back_stencil_fail = StencilOperation::Keep;

    /** Operation performed when stencil passes but depth fails for back-facing fragments. */
    StencilOperation back_depth_fail = StencilOperation::Keep;

    /** Operation performed when both stencil and depth tests pass for back-facing fragments. */
    StencilOperation back_pass = StencilOperation::Keep;

    /** Comparison function for the stencil test on back-facing fragments. */
    Comparator back_stencil_comparator = Comparator::Always;
};

struct ColorBlendDesc
{
    /** Enables blending. When disabled, the source fragment color is written directly. */
    bool blend_enabled = false;

    /** Factor applied to the source color. */
    BlendFactor src_color_factor = BlendFactor::SrcAlpha;

    /** Factor applied to the destination color. */
    BlendFactor dst_color_factor = BlendFactor::InvSrcAlpha;

    /** Operation used to combine source and destination colors. */
    BlendOperation color_operation = BlendOperation::Add;

    /** Factor applied to the source alpha. */
    BlendFactor src_alpha_factor = BlendFactor::One;

    /** Factor applied to the destination alpha. */
    BlendFactor dst_alpha_factor = BlendFactor::InvSrcAlpha;

    /** Operation used to combine source and destination alpha values. */
    BlendOperation alpha_operation = BlendOperation::Add;

    /**
     * Which channels this attachment is written to. A channel left out keeps what the attachment already
     * held, blending included - the mask is applied after the blend, not instead of it.
     */
    ColorWriteMaskBits color_write_mask = ColorWriteMaskBits::All;
};

struct GraphicsPipelineDesc
{
    VertexInputDesc vertex_input;

    Opal::Ref<const Shader> vertex_shader;
    Opal::Ref<const Shader> fragment_shader;
    Opal::Ref<const Shader> task_shader;
    Opal::Ref<const Shader> mesh_shader;

    Opal::DynamicArray<Opal::Ref<const DescriptorSetLayout>> descriptor_set_layouts;
    Opal::DynamicArray<PushConstantRange> push_constant_ranges;

    PrimitiveTopology topology = PrimitiveTopology::Triangle;
    RasterizerDesc rasterizer;

    /**
     * Samples per pixel the attachments of this pipeline carry. Has to match what the attachments were
     * created with, and a count this device does not support for both colour and depth throws rather than
     * being left to the validation layer.
     */
    SampleCount sample_count = SampleCount::Count1;

    /**
     * State this pipeline leaves to the command buffer. Viewport and scissor are always dynamic and are not
     * named here; anything named has to be set with its Cmd* call before a draw that uses it.
     */
    DynamicStateBits dynamic_state = DynamicStateBits::None;
    DepthStencilDesc depth_stencil;
    Opal::DynamicArray<ColorBlendDesc> color_blend_attachments;

    /**
     * Values for constants the shaders of this pipeline declare, by name. A constant left unnamed keeps the
     * default the shader gave it, and a name no stage of this pipeline declares throws - Vulkan ignores a
     * numeric id that matches nothing, which is exactly the mistake worth catching.
     *
     * Applied when the pipeline is built rather than baked into the SPIR-V, so two pipelines can share one
     * Shader and differ only in these. Shader::GetSpecializationConstants says what the names are.
     */
    Opal::DynamicArray<SpecializationConstant> specialization;

    Opal::DynamicArray<PixelFormat> color_attachment_formats;
    PixelFormat depth_attachment_format = PixelFormat::Undefined;
    PixelFormat stencil_attachment_format = PixelFormat::Undefined;
};

struct ComputePipelineDesc
{
    Opal::Ref<const Shader> shader;
    Opal::DynamicArray<Opal::Ref<const DescriptorSetLayout>> descriptor_set_layouts;
    Opal::DynamicArray<PushConstantRange> push_constant_ranges;

    /**
     * Values for constants the shaders of this pipeline declare, by name. A constant left unnamed keeps the
     * default the shader gave it, and a name no stage of this pipeline declares throws - Vulkan ignores a
     * numeric id that matches nothing, which is exactly the mistake worth catching.
     *
     * Applied when the pipeline is built rather than baked into the SPIR-V, so two pipelines can share one
     * Shader and differ only in these. Shader::GetSpecializationConstants says what the names are.
     */
    Opal::DynamicArray<SpecializationConstant> specialization;
};

class Pipeline
{
public:
    Pipeline() = default;
    explicit Pipeline(const Device& device, const GraphicsPipelineDesc& desc);
    explicit Pipeline(const Device& device, const ComputePipelineDesc& desc);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) noexcept;
    Pipeline& operator=(Pipeline&&) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_pipeline != VK_NULL_HANDLE; }
    [[nodiscard]] VkPipeline GetNativePipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetNativePipelineLayout() const { return m_pipeline_layout; }
    [[nodiscard]] VkPipelineBindPoint GetBindPoint() const { return m_bind_point; }

private:
    void CreatePipelineLayout(
        Opal::ArrayView<const Opal::Ref<const DescriptorSetLayout>> descriptor_set_layouts,
        Opal::ArrayView<const PushConstantRange> push_constant_ranges);

    Opal::Ref<const Device> m_device;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VkPipelineBindPoint m_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

}  // namespace Rndr::Forge
