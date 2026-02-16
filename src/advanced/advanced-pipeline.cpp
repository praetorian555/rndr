#include "rndr/advanced/advanced-pipeline.hpp"

#include "opal/exceptions.h"

#include "rndr/pixel-format.hpp"

#include "rndr/advanced/advanced-descriptor-set.hpp"
#include "rndr/advanced/advanced-shader.hpp"
#include "rndr/advanced/device.hpp"
#include "rndr/advanced/vulkan-exception.hpp"

static VkPrimitiveTopology ToVkPrimitiveTopology(Rndr::PrimitiveTopology topology)
{
    switch (topology)
    {
        case Rndr::PrimitiveTopology::Point:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case Rndr::PrimitiveTopology::Line:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case Rndr::PrimitiveTopology::LineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case Rndr::PrimitiveTopology::Triangle:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case Rndr::PrimitiveTopology::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        default:
            throw Opal::Exception("Unsupported primitive topology");
    }
}

static VkPolygonMode ToVkPolygonMode(Rndr::FillMode fill_mode)
{
    switch (fill_mode)
    {
        case Rndr::FillMode::Solid:
            return VK_POLYGON_MODE_FILL;
        case Rndr::FillMode::Wireframe:
            return VK_POLYGON_MODE_LINE;
        default:
            throw Opal::Exception("Unsupported fill mode");
    }
}

static VkCullModeFlags ToVkCullMode(Rndr::Face cull_face)
{
    switch (cull_face)
    {
        case Rndr::Face::None:
            return VK_CULL_MODE_NONE;
        case Rndr::Face::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case Rndr::Face::Back:
            return VK_CULL_MODE_BACK_BIT;
        default:
            throw Opal::Exception("Unsupported cull mode");
    }
}

static VkFrontFace ToVkFrontFace(Rndr::WindingOrder winding_order)
{
    switch (winding_order)
    {
        case Rndr::WindingOrder::CW:
            return VK_FRONT_FACE_CLOCKWISE;
        case Rndr::WindingOrder::CCW:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        default:
            throw Opal::Exception("Unsupported winding order");
    }
}

static VkCompareOp ToVkCompareOp(Rndr::Comparator comparator)
{
    switch (comparator)
    {
        case Rndr::Comparator::Never:
            return VK_COMPARE_OP_NEVER;
        case Rndr::Comparator::Always:
            return VK_COMPARE_OP_ALWAYS;
        case Rndr::Comparator::Less:
            return VK_COMPARE_OP_LESS;
        case Rndr::Comparator::Greater:
            return VK_COMPARE_OP_GREATER;
        case Rndr::Comparator::Equal:
            return VK_COMPARE_OP_EQUAL;
        case Rndr::Comparator::NotEqual:
            return VK_COMPARE_OP_NOT_EQUAL;
        case Rndr::Comparator::LessEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case Rndr::Comparator::GreaterEqual:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        default:
            throw Opal::Exception("Unsupported comparator");
    }
}

static VkBlendFactor ToVkBlendFactor(Rndr::BlendFactor factor)
{
    switch (factor)
    {
        case Rndr::BlendFactor::Zero:
            return VK_BLEND_FACTOR_ZERO;
        case Rndr::BlendFactor::One:
            return VK_BLEND_FACTOR_ONE;
        case Rndr::BlendFactor::SrcColor:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case Rndr::BlendFactor::DstColor:
            return VK_BLEND_FACTOR_DST_COLOR;
        case Rndr::BlendFactor::InvSrcColor:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case Rndr::BlendFactor::InvDstColor:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case Rndr::BlendFactor::SrcAlpha:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case Rndr::BlendFactor::DstAlpha:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case Rndr::BlendFactor::InvSrcAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case Rndr::BlendFactor::InvDstAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case Rndr::BlendFactor::ConstColor:
            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case Rndr::BlendFactor::InvConstColor:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case Rndr::BlendFactor::ConstAlpha:
            return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case Rndr::BlendFactor::InvConstAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        default:
            throw Opal::Exception("Unsupported blend factor");
    }
}

static VkStencilOp ToVkStencilOp(Rndr::StencilOperation op)
{
    switch (op)
    {
        case Rndr::StencilOperation::Keep:
            return VK_STENCIL_OP_KEEP;
        case Rndr::StencilOperation::Zero:
            return VK_STENCIL_OP_ZERO;
        case Rndr::StencilOperation::Replace:
            return VK_STENCIL_OP_REPLACE;
        case Rndr::StencilOperation::Increment:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case Rndr::StencilOperation::IncrementWrap:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case Rndr::StencilOperation::Decrement:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case Rndr::StencilOperation::DecrementWrap:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        case Rndr::StencilOperation::Invert:
            return VK_STENCIL_OP_INVERT;
        default:
            throw Opal::Exception("Unsupported stencil operation");
    }
}

static VkBlendOp ToVkBlendOp(Rndr::BlendOperation op)
{
    switch (op)
    {
        case Rndr::BlendOperation::Add:
            return VK_BLEND_OP_ADD;
        case Rndr::BlendOperation::Subtract:
            return VK_BLEND_OP_SUBTRACT;
        case Rndr::BlendOperation::ReverseSubtract:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case Rndr::BlendOperation::Min:
            return VK_BLEND_OP_MIN;
        case Rndr::BlendOperation::Max:
            return VK_BLEND_OP_MAX;
        default:
            throw Opal::Exception("Unsupported blend operation");
    }
}

static VkShaderStageFlags ToVkShaderStageFlags(Rndr::ShaderTypeBits stages)
{
    VkShaderStageFlags flags = 0;
    if (!!(stages & Rndr::ShaderTypeBits::AllGraphics))
    {
        flags |= VK_SHADER_STAGE_ALL_GRAPHICS;
    }
    if (!!(stages & Rndr::ShaderTypeBits::Vertex))
    {
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if (!!(stages & Rndr::ShaderTypeBits::Fragment))
    {
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    if (!!(stages & Rndr::ShaderTypeBits::Compute))
    {
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    }
    if (!!(stages & Rndr::ShaderTypeBits::Task))
    {
        flags |= VK_SHADER_STAGE_TASK_BIT_EXT;
    }
    if (!!(stages & Rndr::ShaderTypeBits::Mesh))
    {
        flags |= VK_SHADER_STAGE_MESH_BIT_EXT;
    }
    return flags;
}

static VkVertexInputRate ToVkVertexInputRate(Rndr::DataRepetition repetition)
{
    switch (repetition)
    {
        case Rndr::DataRepetition::PerVertex:
            return VK_VERTEX_INPUT_RATE_VERTEX;
        case Rndr::DataRepetition::PerInstance:
            return VK_VERTEX_INPUT_RATE_INSTANCE;
        default:
            throw Opal::Exception("Unsupported data repetition");
    }
}

Rndr::AdvancedVertexInputDesc::Binding& Rndr::AdvancedVertexInputDesc::AddBinding(u32 binding, u32 stride, DataRepetition input_rate)
{
    bindings.PushBack(Binding{.binding = binding, .stride = stride, .input_rate = input_rate});
    return bindings[bindings.GetSize() - 1];
}

void Rndr::AdvancedVertexInputDesc::AddAttribute(u32 binding, u32 location, PixelFormat format, u32 offset)
{
    for (auto& b : bindings)
    {
        if (b.binding == binding)
        {
            b.attributes.PushBack(Attribute{.location = location, .format = format, .offset = offset});
            return;
        }
    }
    throw Opal::Exception("Binding not found in vertex input desc");
}

void Rndr::AdvancedPipeline::CreatePipelineLayout(
    Opal::ArrayView<const Opal::Ref<const AdvancedDescriptorSetLayout>> descriptor_set_layouts,
    Opal::ArrayView<const AdvancedPushConstantRange> push_constant_ranges)
{
    Opal::DynamicArray<VkDescriptorSetLayout> native_layouts;
    for (const auto& layout : descriptor_set_layouts)
    {
        native_layouts.PushBack(layout->GetNativeDescriptorSetLayout());
    }

    Opal::DynamicArray<VkPushConstantRange> native_push_constants;
    for (const auto& range : push_constant_ranges)
    {
        native_push_constants.PushBack(VkPushConstantRange{
            .stageFlags = ToVkShaderStageFlags(range.shader_stages),
            .offset = range.offset,
            .size = range.size,
        });
    }

    const VkPipelineLayoutCreateInfo layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<u32>(native_layouts.GetSize()),
        .pSetLayouts = native_layouts.GetData(),
        .pushConstantRangeCount = static_cast<u32>(native_push_constants.GetSize()),
        .pPushConstantRanges = native_push_constants.GetData(),
    };
    const VkResult result = vkCreatePipelineLayout(m_device->GetNativeDevice(), &layout_create_info, nullptr, &m_pipeline_layout);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreatePipelineLayout");
    }
}

Rndr::AdvancedPipeline::AdvancedPipeline(const AdvancedDevice& device, const AdvancedGraphicsPipelineDesc& desc)
    : m_device(device), m_bind_point(VK_PIPELINE_BIND_POINT_GRAPHICS)
{
    CreatePipelineLayout(
        {desc.descriptor_set_layouts.GetData(), desc.descriptor_set_layouts.GetSize()},
        {desc.push_constant_ranges.GetData(), desc.push_constant_ranges.GetSize()});

    const bool has_vertex = desc.vertex_shader != nullptr;
    const bool has_mesh = desc.mesh_shader != nullptr;
    const bool has_task = desc.task_shader != nullptr;
    if (!has_vertex && !has_mesh)
    {
        throw Opal::Exception("Graphics pipeline requires either a vertex shader or a mesh shader");
    }
    if (has_vertex && has_mesh)
    {
        throw Opal::Exception("Graphics pipeline cannot have both a vertex shader and a mesh shader");
    }
    if (has_task && !has_mesh)
    {
        throw Opal::Exception("Task shader requires a mesh shader");
    }

    Opal::DynamicArray<VkPipelineShaderStageCreateInfo> shader_stages;
    auto add_shader_stage = [&shader_stages](const AdvancedShader& shader)
    {
        shader_stages.PushBack(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = shader.GetNativeShaderStage(),
            .module = shader.GetNativeShaderModule(),
            .pName = shader.GetEntryPoint().GetData(),
        });
    };

    if (has_vertex)
    {
        add_shader_stage(*desc.vertex_shader);
    }
    if (has_task)
    {
        add_shader_stage(*desc.task_shader);
    }
    if (has_mesh)
    {
        add_shader_stage(*desc.mesh_shader);
    }
    if (desc.fragment_shader != nullptr)
    {
        add_shader_stage(*desc.fragment_shader);
    }

    Opal::DynamicArray<VkVertexInputBindingDescription> vk_bindings;
    Opal::DynamicArray<VkVertexInputAttributeDescription> vk_attributes;
    for (const auto& binding : desc.vertex_input.bindings)
    {
        vk_bindings.PushBack(VkVertexInputBindingDescription{
            .binding = binding.binding,
            .stride = binding.stride,
            .inputRate = ToVkVertexInputRate(binding.input_rate),
        });
        for (const auto& attr : binding.attributes)
        {
            vk_attributes.PushBack(VkVertexInputAttributeDescription{
                .location = attr.location,
                .binding = binding.binding,
                .format = ToVkFormat(attr.format),
                .offset = attr.offset,
            });
        }
    }

    const VkPipelineVertexInputStateCreateInfo vertex_input_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = static_cast<u32>(vk_bindings.GetSize()),
        .pVertexBindingDescriptions = vk_bindings.GetData(),
        .vertexAttributeDescriptionCount = static_cast<u32>(vk_attributes.GetSize()),
        .pVertexAttributeDescriptions = vk_attributes.GetData(),
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = ToVkPrimitiveTopology(desc.topology),
    };

    const VkPipelineViewportStateCreateInfo viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    const VkPipelineRasterizationStateCreateInfo rasterization_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = ToVkPolygonMode(desc.rasterizer.fill_mode),
        .cullMode = ToVkCullMode(desc.rasterizer.cull_mode),
        .frontFace = ToVkFrontFace(desc.rasterizer.front_face),
        .depthBiasEnable = desc.rasterizer.depth_bias_enabled ? VK_TRUE : VK_FALSE,
        .depthBiasConstantFactor = desc.rasterizer.depth_bias_constant_factor,
        .depthBiasClamp = desc.rasterizer.depth_bias_clamp,
        .depthBiasSlopeFactor = desc.rasterizer.depth_bias_slope_factor,
        .lineWidth = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisample_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    const auto& ds = desc.depth_stencil;
    const VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = ds.depth_test_enabled ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = ds.depth_write_enabled ? VK_TRUE : VK_FALSE,
        .depthCompareOp = ToVkCompareOp(ds.depth_comparator),
        .stencilTestEnable = ds.stencil_test_enabled ? VK_TRUE : VK_FALSE,
        .front =
            {
                .failOp = ToVkStencilOp(ds.front_stencil_fail),
                .passOp = ToVkStencilOp(ds.front_pass),
                .depthFailOp = ToVkStencilOp(ds.front_depth_fail),
                .compareOp = ToVkCompareOp(ds.front_stencil_comparator),
            },
        .back =
            {
                .failOp = ToVkStencilOp(ds.back_stencil_fail),
                .passOp = ToVkStencilOp(ds.back_pass),
                .depthFailOp = ToVkStencilOp(ds.back_depth_fail),
                .compareOp = ToVkCompareOp(ds.back_stencil_comparator),
            },
    };

    Opal::DynamicArray<VkPipelineColorBlendAttachmentState> color_blend_attachments;
    for (const auto& cb : desc.color_blend_attachments)
    {
        color_blend_attachments.PushBack(VkPipelineColorBlendAttachmentState{
            .blendEnable = cb.blend_enabled ? VK_TRUE : VK_FALSE,
            .srcColorBlendFactor = ToVkBlendFactor(cb.src_color_factor),
            .dstColorBlendFactor = ToVkBlendFactor(cb.dst_color_factor),
            .colorBlendOp = ToVkBlendOp(cb.color_operation),
            .srcAlphaBlendFactor = ToVkBlendFactor(cb.src_alpha_factor),
            .dstAlphaBlendFactor = ToVkBlendFactor(cb.dst_alpha_factor),
            .alphaBlendOp = ToVkBlendOp(cb.alpha_operation),
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        });
    }

    const VkPipelineColorBlendStateCreateInfo color_blend_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = static_cast<u32>(color_blend_attachments.GetSize()),
        .pAttachments = color_blend_attachments.GetData(),
    };

    constexpr VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    Opal::DynamicArray<VkFormat> vk_color_formats;
    for (const auto& format : desc.color_attachment_formats)
    {
        vk_color_formats.PushBack(ToVkFormat(format));
    }

    const VkPipelineRenderingCreateInfo rendering_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = static_cast<u32>(vk_color_formats.GetSize()),
        .pColorAttachmentFormats = vk_color_formats.GetData(),
        .depthAttachmentFormat = ToVkFormat(desc.depth_attachment_format),
        .stencilAttachmentFormat = ToVkFormat(desc.stencil_attachment_format),
    };

    const VkGraphicsPipelineCreateInfo pipeline_create_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_create_info,
        .stageCount = static_cast<u32>(shader_stages.GetSize()),
        .pStages = shader_stages.GetData(),
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &depth_stencil_state,
        .pColorBlendState = &color_blend_state,
        .pDynamicState = &dynamic_state,
        .layout = m_pipeline_layout,
    };

    const VkResult gfx_result =
        vkCreateGraphicsPipelines(m_device->GetNativeDevice(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &m_pipeline);
    if (gfx_result != VK_SUCCESS)
    {
        throw VulkanException(gfx_result, "vkCreateGraphicsPipelines");
    }
}

Rndr::AdvancedPipeline::AdvancedPipeline(const AdvancedDevice& device, const AdvancedComputePipelineDesc& desc)
    : m_device(device), m_bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
{
    CreatePipelineLayout(
        {desc.descriptor_set_layouts.GetData(), desc.descriptor_set_layouts.GetSize()},
        {desc.push_constant_ranges.GetData(), desc.push_constant_ranges.GetSize()});

    const VkComputePipelineCreateInfo pipeline_create_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage =
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = desc.shader->GetNativeShaderStage(),
                .module = desc.shader->GetNativeShaderModule(),
                .pName = desc.shader->GetEntryPoint().GetData(),
            },
        .layout = m_pipeline_layout,
    };

    const VkResult compute_result =
        vkCreateComputePipelines(m_device->GetNativeDevice(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &m_pipeline);
    if (compute_result != VK_SUCCESS)
    {
        throw VulkanException(compute_result, "vkCreateComputePipelines");
    }
}

Rndr::AdvancedPipeline::~AdvancedPipeline()
{
    Destroy();
}

Rndr::AdvancedPipeline::AdvancedPipeline(AdvancedPipeline&& other) noexcept
    : m_device(other.m_device),
      m_pipeline(other.m_pipeline),
      m_pipeline_layout(other.m_pipeline_layout),
      m_bind_point(other.m_bind_point)
{
    other.m_pipeline = VK_NULL_HANDLE;
    other.m_pipeline_layout = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::AdvancedPipeline& Rndr::AdvancedPipeline::operator=(AdvancedPipeline&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = other.m_device;
        m_pipeline = other.m_pipeline;
        m_pipeline_layout = other.m_pipeline_layout;
        m_bind_point = other.m_bind_point;
        other.m_pipeline = VK_NULL_HANDLE;
        other.m_pipeline_layout = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::AdvancedPipeline::Destroy()
{
    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device->GetNativeDevice(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipeline_layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device->GetNativeDevice(), m_pipeline_layout, nullptr);
        m_pipeline_layout = VK_NULL_HANDLE;
    }
    m_device = nullptr;
}
