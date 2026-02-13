#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/ref.h"

#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

struct AdvancedPushConstantRange
{
    ShaderTypeBits shader_stages;
    u32 offset = 0;
    u32 size = 0;
};

struct AdvancedVertexInputDesc
{
    struct Attribute
    {
        u32 location;
        PixelFormat format;
        u32 offset;
    };

    struct Binding
    {
        u32 binding;
        u32 stride;
        DataRepetition input_rate = DataRepetition::PerVertex;
        Opal::DynamicArray<Attribute> attributes;
    };

    Opal::DynamicArray<Binding> bindings;

    Binding& AddBinding(u32 binding, u32 stride, DataRepetition input_rate = DataRepetition::PerVertex);
    void AddAttribute(u32 binding, u32 location, PixelFormat format, u32 offset);
};

struct AdvancedRasterizerDesc
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

struct AdvancedDepthStencilDesc
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

struct AdvancedColorBlendDesc
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
};

struct AdvancedGraphicsPipelineDesc
{
    AdvancedVertexInputDesc vertex_input;

    Opal::Ref<const class AdvancedShader> vertex_shader;
    Opal::Ref<const class AdvancedShader> fragment_shader;
    Opal::Ref<const class AdvancedShader> task_shader;
    Opal::Ref<const class AdvancedShader> mesh_shader;

    Opal::DynamicArray<Opal::Ref<const class AdvancedDescriptorSetLayout>> descriptor_set_layouts;
    Opal::DynamicArray<AdvancedPushConstantRange> push_constant_ranges;

    PrimitiveTopology topology = PrimitiveTopology::Triangle;
    AdvancedRasterizerDesc rasterizer;
    AdvancedDepthStencilDesc depth_stencil;
    Opal::DynamicArray<AdvancedColorBlendDesc> color_blend_attachments;

    Opal::DynamicArray<PixelFormat> color_attachment_formats;
    PixelFormat depth_attachment_format = PixelFormat::Undefined;
    PixelFormat stencil_attachment_format = PixelFormat::Undefined;
};

struct AdvancedComputePipelineDesc
{
    Opal::Ref<const class AdvancedShader> shader;
    Opal::DynamicArray<Opal::Ref<const class AdvancedDescriptorSetLayout>> descriptor_set_layouts;
    Opal::DynamicArray<AdvancedPushConstantRange> push_constant_ranges;
};

class AdvancedPipeline
{
public:
    AdvancedPipeline() = default;
    explicit AdvancedPipeline(const class AdvancedDevice& device, const AdvancedGraphicsPipelineDesc& desc);
    explicit AdvancedPipeline(const class AdvancedDevice& device, const AdvancedComputePipelineDesc& desc);
    ~AdvancedPipeline();

    AdvancedPipeline(const AdvancedPipeline&) = delete;
    AdvancedPipeline& operator=(const AdvancedPipeline&) = delete;
    AdvancedPipeline(AdvancedPipeline&&) noexcept;
    AdvancedPipeline& operator=(AdvancedPipeline&&) noexcept;

    void Destroy();

    [[nodiscard]] VkPipeline GetNativePipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetNativePipelineLayout() const { return m_pipeline_layout; }
    [[nodiscard]] VkPipelineBindPoint GetBindPoint() const { return m_bind_point; }

private:
    void CreatePipelineLayout(
        Opal::ArrayView<const Opal::Ref<const class AdvancedDescriptorSetLayout>> descriptor_set_layouts,
        Opal::ArrayView<const AdvancedPushConstantRange> push_constant_ranges);

    Opal::Ref<const class AdvancedDevice> m_device;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VkPipelineBindPoint m_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

}  // namespace Rndr
