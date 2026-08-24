#include "rndr/forge/pipeline.hpp"

#include "opal/exceptions.h"

#include "rndr/pixel-format.hpp"

#include "rndr/forge/descriptor-set.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/shader.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/log.hpp"

static Opal::Optional<VkPrimitiveTopology> ToVkPrimitiveTopology(Rndr::PrimitiveTopology topology)
{
    switch (topology)
    {
        case Rndr::PrimitiveTopology::Point:
            return Opal::Optional<VkPrimitiveTopology>(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
        case Rndr::PrimitiveTopology::Line:
            return Opal::Optional<VkPrimitiveTopology>(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
        case Rndr::PrimitiveTopology::LineStrip:
            return Opal::Optional<VkPrimitiveTopology>(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
        case Rndr::PrimitiveTopology::Triangle:
            return Opal::Optional<VkPrimitiveTopology>(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        case Rndr::PrimitiveTopology::TriangleStrip:
            return Opal::Optional<VkPrimitiveTopology>(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
        default:
            return {};
    }
}

static Opal::Optional<VkPolygonMode> ToVkPolygonMode(Rndr::FillMode fill_mode)
{
    switch (fill_mode)
    {
        case Rndr::FillMode::Solid:
            return Opal::Optional<VkPolygonMode>(VK_POLYGON_MODE_FILL);
        case Rndr::FillMode::Wireframe:
            return Opal::Optional<VkPolygonMode>(VK_POLYGON_MODE_LINE);
        default:
            return {};
    }
}

static Opal::Optional<VkCullModeFlags> ToVkCullMode(Rndr::Face cull_face)
{
    switch (cull_face)
    {
        case Rndr::Face::None:
            return Opal::Optional<VkCullModeFlags>(VK_CULL_MODE_NONE);
        case Rndr::Face::Front:
            return Opal::Optional<VkCullModeFlags>(VK_CULL_MODE_FRONT_BIT);
        case Rndr::Face::Back:
            return Opal::Optional<VkCullModeFlags>(VK_CULL_MODE_BACK_BIT);
        default:
            return {};
    }
}

/** The one place a SampleCount becomes a Vulkan bit, so a value with no counterpart cannot be cast into one. */
static Opal::Optional<VkSampleCountFlagBits> ToVkSampleCount(Rndr::Forge::SampleCount sample_count)
{
    switch (sample_count)
    {
        case Rndr::Forge::SampleCount::Count1:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_1_BIT);
        case Rndr::Forge::SampleCount::Count2:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_2_BIT);
        case Rndr::Forge::SampleCount::Count4:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_4_BIT);
        case Rndr::Forge::SampleCount::Count8:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_8_BIT);
        case Rndr::Forge::SampleCount::Count16:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_16_BIT);
        case Rndr::Forge::SampleCount::Count32:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_32_BIT);
        case Rndr::Forge::SampleCount::Count64:
            return Opal::Optional<VkSampleCountFlagBits>(VK_SAMPLE_COUNT_64_BIT);
    }
    return {};
}

static Opal::Optional<VkFrontFace> ToVkFrontFace(Rndr::WindingOrder winding_order)
{
    switch (winding_order)
    {
        case Rndr::WindingOrder::CW:
            return Opal::Optional<VkFrontFace>(VK_FRONT_FACE_CLOCKWISE);
        case Rndr::WindingOrder::CCW:
            return Opal::Optional<VkFrontFace>(VK_FRONT_FACE_COUNTER_CLOCKWISE);
        default:
            return {};
    }
}

static Opal::Optional<VkCompareOp> ToVkCompareOp(Rndr::Comparator comparator)
{
    switch (comparator)
    {
        case Rndr::Comparator::Never:
            return Opal::Optional<VkCompareOp>(VK_COMPARE_OP_NEVER);
        case Rndr::Comparator::Always:
            return Opal::Optional<VkCompareOp>(VK_COMPARE_OP_ALWAYS);
        case Rndr::Comparator::Less:
            return Opal::Optional<VkCompareOp>(VK_COMPARE_OP_LESS);
        case Rndr::Comparator::Greater:
            return Opal::Optional<VkCompareOp>(VK_COMPARE_OP_GREATER);
        case Rndr::Comparator::Equal:
            return Opal::Optional<VkCompareOp>(VK_COMPARE_OP_EQUAL);
        case Rndr::Comparator::NotEqual:
            return Opal::Optional<VkCompareOp>(VK_COMPARE_OP_NOT_EQUAL);
        case Rndr::Comparator::LessEqual:
            return Opal::Optional<VkCompareOp>(VK_COMPARE_OP_LESS_OR_EQUAL);
        case Rndr::Comparator::GreaterEqual:
            return Opal::Optional<VkCompareOp>(VK_COMPARE_OP_GREATER_OR_EQUAL);
        default:
            return {};
    }
}

static Opal::Optional<VkBlendFactor> ToVkBlendFactor(Rndr::BlendFactor factor)
{
    switch (factor)
    {
        case Rndr::BlendFactor::Zero:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_ZERO);
        case Rndr::BlendFactor::One:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_ONE);
        case Rndr::BlendFactor::SrcColor:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_SRC_COLOR);
        case Rndr::BlendFactor::DstColor:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_DST_COLOR);
        case Rndr::BlendFactor::InvSrcColor:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR);
        case Rndr::BlendFactor::InvDstColor:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR);
        case Rndr::BlendFactor::SrcAlpha:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_SRC_ALPHA);
        case Rndr::BlendFactor::DstAlpha:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_DST_ALPHA);
        case Rndr::BlendFactor::InvSrcAlpha:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        case Rndr::BlendFactor::InvDstAlpha:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA);
        case Rndr::BlendFactor::ConstColor:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_CONSTANT_COLOR);
        case Rndr::BlendFactor::InvConstColor:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR);
        case Rndr::BlendFactor::ConstAlpha:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_CONSTANT_ALPHA);
        case Rndr::BlendFactor::InvConstAlpha:
            return Opal::Optional<VkBlendFactor>(VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA);
        default:
            return {};
    }
}

static Opal::Optional<VkStencilOp> ToVkStencilOp(Rndr::StencilOperation op)
{
    switch (op)
    {
        case Rndr::StencilOperation::Keep:
            return Opal::Optional<VkStencilOp>(VK_STENCIL_OP_KEEP);
        case Rndr::StencilOperation::Zero:
            return Opal::Optional<VkStencilOp>(VK_STENCIL_OP_ZERO);
        case Rndr::StencilOperation::Replace:
            return Opal::Optional<VkStencilOp>(VK_STENCIL_OP_REPLACE);
        case Rndr::StencilOperation::Increment:
            return Opal::Optional<VkStencilOp>(VK_STENCIL_OP_INCREMENT_AND_CLAMP);
        case Rndr::StencilOperation::IncrementWrap:
            return Opal::Optional<VkStencilOp>(VK_STENCIL_OP_INCREMENT_AND_WRAP);
        case Rndr::StencilOperation::Decrement:
            return Opal::Optional<VkStencilOp>(VK_STENCIL_OP_DECREMENT_AND_CLAMP);
        case Rndr::StencilOperation::DecrementWrap:
            return Opal::Optional<VkStencilOp>(VK_STENCIL_OP_DECREMENT_AND_WRAP);
        case Rndr::StencilOperation::Invert:
            return Opal::Optional<VkStencilOp>(VK_STENCIL_OP_INVERT);
        default:
            return {};
    }
}

static Opal::Optional<VkBlendOp> ToVkBlendOp(Rndr::BlendOperation op)
{
    switch (op)
    {
        case Rndr::BlendOperation::Add:
            return Opal::Optional<VkBlendOp>(VK_BLEND_OP_ADD);
        case Rndr::BlendOperation::Subtract:
            return Opal::Optional<VkBlendOp>(VK_BLEND_OP_SUBTRACT);
        case Rndr::BlendOperation::ReverseSubtract:
            return Opal::Optional<VkBlendOp>(VK_BLEND_OP_REVERSE_SUBTRACT);
        case Rndr::BlendOperation::Min:
            return Opal::Optional<VkBlendOp>(VK_BLEND_OP_MIN);
        case Rndr::BlendOperation::Max:
            return Opal::Optional<VkBlendOp>(VK_BLEND_OP_MAX);
        default:
            return {};
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

static Opal::Optional<VkVertexInputRate> ToVkVertexInputRate(Rndr::DataRepetition repetition)
{
    switch (repetition)
    {
        case Rndr::DataRepetition::PerVertex:
            return Opal::Optional<VkVertexInputRate>(VK_VERTEX_INPUT_RATE_VERTEX);
        case Rndr::DataRepetition::PerInstance:
            return Opal::Optional<VkVertexInputRate>(VK_VERTEX_INPUT_RATE_INSTANCE);
        default:
            return {};
    }
}

Rndr::Forge::VertexInputDesc::Binding& Rndr::Forge::VertexInputDesc::AddBinding(u32 binding, u32 stride, DataRepetition input_rate)
{
    bindings.PushBack(Binding{.binding = binding, .stride = stride, .input_rate = input_rate});
    return bindings[bindings.GetSize() - 1];
}

Rndr::ErrorCode Rndr::Forge::VertexInputDesc::AddAttribute(u32 binding, u32 location, PixelFormat format, u32 offset)
{
    for (auto& b : bindings)
    {
        if (b.binding == binding)
        {
            b.attributes.PushBack(Attribute{.location = location, .format = format, .offset = offset});
            return ErrorCode::Success;
        }
    }
    RNDR_LOG_ERROR("Forge: this vertex input desc has no binding {}", binding);
    return ErrorCode::InvalidArgument;
}

Opal::Expected<Rndr::Forge::VertexInputDesc, Rndr::ErrorCode> Rndr::Forge::VertexInputDesc::FromShader(const Shader& vertex_shader,
                                                                                                       u32 binding,
                                                                                                       DataRepetition input_rate)
{
    using Result = Opal::Expected<VertexInputDesc, ErrorCode>;

    if (vertex_shader.GetShaderStage() != ShaderTypeBits::Vertex)
    {
        RNDR_LOG_ERROR("Forge: only a vertex shader is fed from a vertex buffer, so only one has attributes to read");
        return Result(ErrorCode::InvalidArgument);
    }
    const Opal::ArrayView<const ShaderInputInfo> inputs = vertex_shader.GetInputs();

    // Reflection reports the attributes in no promised order, and the packing below is defined by location.
    Opal::DynamicArray<const ShaderInputInfo*> sorted;
    sorted.Reserve(inputs.GetSize());
    for (i32 i = 0; i < inputs.GetSize(); ++i)
    {
        sorted.PushBack(&inputs[i]);
    }
    for (i32 i = 1; i < sorted.GetSize(); ++i)
    {
        for (i32 j = i; j > 0 && sorted[j - 1]->location > sorted[j]->location; --j)
        {
            const ShaderInputInfo* swap = sorted[j - 1];
            sorted[j - 1] = sorted[j];
            sorted[j] = swap;
        }
    }

    VertexInputDesc desc;
    VertexInputDesc::Binding& target = desc.AddBinding(binding, 0, input_rate);
    u32 offset = 0;
    for (i32 i = 0; i < sorted.GetSize(); ++i)
    {
        const ShaderInputInfo& input = *sorted[i];
        const u32 size = GetPixelSize(input.format);
        if (size == 0)
        {
            RNDR_LOG_ERROR(
                "Forge: vertex attribute {} has a format with no size, so where the next one starts is not something this "
                "can work out",
                reinterpret_cast<const char*>(input.name.GetData()));
            return Result(ErrorCode::InvalidArgument);
        }
        target.attributes.PushBack(Attribute{.location = input.location, .format = input.format, .offset = offset});
        offset += size;
    }
    // Tightly packed, so the stride is what the last attribute ends at.
    target.stride = offset;
    return Result(std::move(desc));
}

Opal::DynamicArray<Rndr::Forge::PushConstantRange> Rndr::Forge::PushConstantRangesFromShaders(
    Opal::ArrayView<const Opal::Ref<const Shader>> shaders)
{
    Opal::DynamicArray<PushConstantRange> ranges;
    for (i32 shader_index = 0; shader_index < shaders.GetSize(); ++shader_index)
    {
        const Shader& shader = shaders[shader_index].Get();
        const Opal::ArrayView<const ShaderPushConstantInfo> blocks = shader.GetPushConstants();
        for (i32 block_index = 0; block_index < blocks.GetSize(); ++block_index)
        {
            const ShaderPushConstantInfo& block = blocks[block_index];
            // One block declared by two stages is one range naming both, not two ranges - which is what
            // Vulkan wants and what a vertex and fragment shader sharing a block produce.
            bool merged = false;
            for (i32 i = 0; i < ranges.GetSize(); ++i)
            {
                if (ranges[i].offset == block.offset && ranges[i].size == block.size)
                {
                    ranges[i].shader_stages |= shader.GetShaderStage();
                    merged = true;
                    break;
                }
            }
            if (!merged)
            {
                ranges.PushBack(PushConstantRange{.shader_stages = shader.GetShaderStage(), .offset = block.offset, .size = block.size});
            }
        }
    }
    return ranges;
}

namespace
{
/**
 * Check the vertex input against what the vertex shader declares. Two things are refused: a location the
 * shader reads that no attribute feeds, and an attribute whose numeric class is not the one the shader
 * reads it as, integer against float. Component counts are deliberately left alone - Vulkan pads a shorter
 * attribute and drops the tail of a longer one, both on purpose.
 *
 * An attribute at a location the shader declares nothing at is *not* refused, however tempting: an input
 * the shader does not read is optimised out of the SPIR-V entirely, so reflection cannot tell a stale
 * attribute apart from one feeding a member this entry point happens to ignore. A vertex struct with a
 * field only some of its pipelines read is ordinary, and refusing it would be refusing correct code.
 */
Rndr::ErrorCode RequireVertexInputMatchesShader(const Rndr::Forge::VertexInputDesc& vertex_input, const Rndr::Forge::Shader& vertex_shader)
{
    const Opal::ArrayView<const Rndr::Forge::ShaderInputInfo> inputs = vertex_shader.GetInputs();
    for (Rndr::i32 i = 0; i < inputs.GetSize(); ++i)
    {
        const Rndr::Forge::ShaderInputInfo& input = inputs[i];
        const Rndr::Forge::VertexInputDesc::Attribute* attribute = nullptr;
        for (const Rndr::Forge::VertexInputDesc::Binding& binding : vertex_input.bindings)
        {
            for (const Rndr::Forge::VertexInputDesc::Attribute& candidate : binding.attributes)
            {
                if (candidate.location == input.location)
                {
                    attribute = &candidate;
                    break;
                }
            }
        }
        if (attribute == nullptr)
        {
            RNDR_LOG_ERROR("Forge: the vertex shader reads {} at location {} and no vertex attribute feeds it",
                           reinterpret_cast<const char*>(input.name.GetData()), input.location);
            return Rndr::ErrorCode::InvalidArgument;
        }
        const Rndr::FormatNumericClass wanted = Rndr::GetFormatNumericClass(input.format);
        const Rndr::FormatNumericClass given = Rndr::GetFormatNumericClass(attribute->format);
        if (wanted != given)
        {
            RNDR_LOG_ERROR("Forge: the vertex attribute at location {} does not have the numeric class the shader reads {} as",
                           input.location, reinterpret_cast<const char*>(input.name.GetData()));
            return Rndr::ErrorCode::InvalidArgument;
        }
    }
    return Rndr::ErrorCode::Success;
}

/**
 * Check that the supplied ranges cover every push constant block the shaders read. A range stopping short of
 * the block is the quiet half of this - Vulkan refuses a layout that names no stage at all for a declared
 * block, but a range four bytes too short for what the shader reads goes through.
 */
Rndr::ErrorCode RequirePushConstantsCovered(Opal::ArrayView<const Rndr::Forge::PushConstantRange> ranges,
                                            Opal::ArrayView<const Opal::Ref<const Rndr::Forge::Shader>> shaders)
{
    for (Rndr::i32 shader_index = 0; shader_index < shaders.GetSize(); ++shader_index)
    {
        const Rndr::Forge::Shader& shader = shaders[shader_index].Get();
        const Opal::ArrayView<const Rndr::Forge::ShaderPushConstantInfo> blocks = shader.GetPushConstants();
        for (Rndr::i32 block_index = 0; block_index < blocks.GetSize(); ++block_index)
        {
            const Rndr::Forge::ShaderPushConstantInfo& block = blocks[block_index];
            bool covered = false;
            for (Rndr::i32 i = 0; i < ranges.GetSize(); ++i)
            {
                const Rndr::Forge::PushConstantRange& range = ranges[i];
                if (range.offset <= block.offset && range.offset + range.size >= block.offset + block.size)
                {
                    covered = true;
                    break;
                }
            }
            if (!covered)
            {
                RNDR_LOG_ERROR(
                    "Forge: a shader reads a push constant block of {} bytes at offset {} and no push constant range of "
                    "this pipeline covers it",
                    block.size, block.offset);
                return Rndr::ErrorCode::InvalidArgument;
            }
        }
    }
    return Rndr::ErrorCode::Success;
}
}  // namespace

Rndr::ErrorCode Rndr::Forge::Pipeline::CreatePipelineLayout(
    Opal::ArrayView<const Opal::Ref<const DescriptorSetLayout>> descriptor_set_layouts,
    Opal::ArrayView<const PushConstantRange> push_constant_ranges)
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
    RNDR_FORGE_VK_CHECK(vkCreatePipelineLayout(m_device->GetNativeDevice(), &layout_create_info, nullptr, &m_pipeline_layout),
                        "vkCreatePipelineLayout");
    return ErrorCode::Success;
}

namespace
{
/**
 * What one stage needs handed to Vulkan: the map entries, the packed values they point into, and the
 * VkSpecializationInfo tying them together. Kept alive by the caller until vkCreate*Pipelines returns,
 * which is what pMapEntries and pData require - so this is a local of the pipeline constructor and never
 * something handed out of a helper.
 */
struct StageSpecialization
{
    Opal::DynamicArray<VkSpecializationMapEntry> entries;
    Opal::DynamicArray<Rndr::u8> data;
    VkSpecializationInfo info = {};
};

/** Reads as "Float32" rather than "3", so a mismatch says which two types disagreed. */
const char* SpecializationTypeName(Rndr::Forge::SpecializationType type)
{
    switch (type)
    {
        case Rndr::Forge::SpecializationType::Bool:
            return "Bool";
        case Rndr::Forge::SpecializationType::Int32:
            return "Int32";
        case Rndr::Forge::SpecializationType::UInt32:
            return "UInt32";
        case Rndr::Forge::SpecializationType::Float32:
            return "Float32";
        case Rndr::Forge::SpecializationType::Int64:
            return "Int64";
        case Rndr::Forge::SpecializationType::UInt64:
            return "UInt64";
        case Rndr::Forge::SpecializationType::Float64:
            return "Float64";
    }
    return "unknown";
}

/**
 * Refuses a value too wide for the constant it is going into. Only 8 and 16 bit constants can hit this:
 * they are reported as Int32 or UInt32 so a caller can write a plain integer, which leaves nothing but this
 * between a number too big for the declared width and it being quietly truncated on the way into the blob.
 */
Rndr::ErrorCode RequireValueFits(const Rndr::Forge::SpecializationConstant& value, Rndr::u32 byte_size)
{
    if (byte_size >= 4)
    {
        return Rndr::ErrorCode::Success;
    }
    const Rndr::u64 bits = value.value.bits;
    const Rndr::u32 width = byte_size * 8;
    const bool fits = value.value.type == Rndr::Forge::SpecializationType::Int32
                          ? static_cast<Rndr::i64>(static_cast<Rndr::i32>(bits)) >= -(Rndr::i64{1} << (width - 1)) &&
                                static_cast<Rndr::i64>(static_cast<Rndr::i32>(bits)) < (Rndr::i64{1} << (width - 1))
                          : bits < (Rndr::u64{1} << width);
    if (!fits)
    {
        RNDR_LOG_ERROR("Forge: specialization constant {} is declared {} bits wide and the value given does not fit in it",
                       reinterpret_cast<const char*>(value.name.GetData()), width);
        return Rndr::ErrorCode::InvalidArgument;
    }
    return Rndr::ErrorCode::Success;
}

/**
 * Two values under one name would become two map entries with the same constantID, which the specification
 * does not allow within one VkSpecializationInfo. Caught here rather than left to the validation layer,
 * since by name it is nothing more exotic than the same name written twice.
 */
Rndr::ErrorCode RequireNoDuplicateNames(Opal::ArrayView<const Rndr::Forge::SpecializationConstant> values)
{
    for (Rndr::i32 i = 0; i < values.GetSize(); ++i)
    {
        for (Rndr::i32 j = i + 1; j < values.GetSize(); ++j)
        {
            if (values[i].name == values[j].name)
            {
                RNDR_LOG_ERROR("Forge: specialization constant {} was given a value twice",
                               reinterpret_cast<const char*>(values[i].name.GetData()));
                return Rndr::ErrorCode::InvalidArgument;
            }
        }
    }
    return Rndr::ErrorCode::Success;
}

/**
 * Match the values against what this shader declares and pack the ones that belong to it. A value naming a
 * constant this stage does not have is skipped rather than refused - another stage may declare it, and the
 * caller is told about a name no stage at all declared once every stage has been looked at.
 *
 * @param out_matched One flag per value, set when this stage took it. Never cleared, so it accumulates.
 */
Opal::Expected<StageSpecialization, Rndr::ErrorCode> BuildStageSpecialization(
    const Rndr::Forge::Shader& shader, Opal::ArrayView<const Rndr::Forge::SpecializationConstant> values,
    Opal::DynamicArray<bool>& out_matched)
{
    using Result = Opal::Expected<StageSpecialization, Rndr::ErrorCode>;

    StageSpecialization result;
    const Opal::ArrayView<const Rndr::Forge::SpecializationConstantInfo> declared = shader.GetSpecializationConstants();
    for (Rndr::i32 value_index = 0; value_index < values.GetSize(); ++value_index)
    {
        const Rndr::Forge::SpecializationConstant& value = values[value_index];
        for (Rndr::i32 declared_index = 0; declared_index < declared.GetSize(); ++declared_index)
        {
            const Rndr::Forge::SpecializationConstantInfo& info = declared[declared_index];
            if (info.name != value.name)
            {
                continue;
            }
            // No coercion, not even an integer into a float: a value silently reinterpreted at the wrong
            // width is not something the caller could notice from the outside.
            if (info.type != value.value.type)
            {
                RNDR_LOG_ERROR("Forge: specialization constant {} is declared as {} but was given a {}",
                               reinterpret_cast<const char*>(value.name.GetData()), SpecializationTypeName(info.type),
                               SpecializationTypeName(value.value.type));
                return Result(Rndr::ErrorCode::InvalidArgument);
            }
            // The declared width, not the width of the value: VkSpecializationMapEntry::size has to match
            // the type the shader declared, and an 8 or 16 bit constant is reported as Int32 or UInt32.
            const Rndr::u32 size = info.byte_size;
            RNDR_FORGE_CHECK_EXPECTED(RequireValueFits(value, size), Result);
            const auto offset = static_cast<Rndr::u32>(result.data.GetSize());
            result.data.Resize(static_cast<Rndr::i32>(offset + size));
            // The low bytes of the pattern, which is the whole value on a little-endian host and what the
            // range check above just made sure of.
            memcpy(result.data.GetData() + offset, &value.value.bits, size);
            result.entries.PushBack(VkSpecializationMapEntry{.constantID = info.constant_id, .offset = offset, .size = size});
            out_matched[value_index] = true;
            break;
        }
    }
    result.info = {.mapEntryCount = static_cast<Rndr::u32>(result.entries.GetSize()),
                   .pMapEntries = result.entries.GetData(),
                   .dataSize = static_cast<size_t>(result.data.GetSize()),
                   .pData = result.data.GetData()};
    return Result(std::move(result));
}

/** Gives up on the first value no stage of the pipeline claimed, which Vulkan would have ignored in silence. */
Rndr::ErrorCode RequireEveryValueMatched(Opal::ArrayView<const Rndr::Forge::SpecializationConstant> values,
                                         const Opal::DynamicArray<bool>& matched)
{
    for (Rndr::i32 i = 0; i < values.GetSize(); ++i)
    {
        if (!matched[i])
        {
            RNDR_LOG_ERROR("Forge: no shader of this pipeline declares a specialization constant called {}",
                           reinterpret_cast<const char*>(values[i].name.GetData()));
            return Rndr::ErrorCode::InvalidArgument;
        }
    }
    return Rndr::ErrorCode::Success;
}
}  // namespace

Opal::Expected<Rndr::Forge::Pipeline, Rndr::ErrorCode> Rndr::Forge::Pipeline::Create(const Device& device, const GraphicsPipelineDesc& desc)
{
    using Result = Opal::Expected<Pipeline, ErrorCode>;

    // The layout and the pipeline are both built into this, so a way out of here releases whichever of them
    // already exists through the destructor - which is what the layout guard used to be for.
    Pipeline pipeline;
    pipeline.m_device = device;
    pipeline.m_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    RNDR_FORGE_CHECK_EXPECTED(pipeline.CreatePipelineLayout({desc.descriptor_set_layouts.GetData(), desc.descriptor_set_layouts.GetSize()},
                                                            {desc.push_constant_ranges.GetData(), desc.push_constant_ranges.GetSize()}),
                              Result);

    const bool has_vertex = desc.vertex_shader != nullptr;
    const bool has_mesh = desc.mesh_shader != nullptr;
    const bool has_task = desc.task_shader != nullptr;
    if (!has_vertex && !has_mesh)
    {
        RNDR_LOG_ERROR("Forge: a graphics pipeline needs either a vertex shader or a mesh shader");
        return Result(ErrorCode::InvalidArgument);
    }
    if (has_vertex && has_mesh)
    {
        RNDR_LOG_ERROR("Forge: a graphics pipeline cannot have both a vertex shader and a mesh shader");
        return Result(ErrorCode::InvalidArgument);
    }
    if (has_task && !has_mesh)
    {
        RNDR_LOG_ERROR("Forge: a task shader needs a mesh shader beside it");
        return Result(ErrorCode::InvalidArgument);
    }

    // The stages this pipeline has, gathered before anything points into a list: pMapEntries and pData have to
    // stay put until vkCreateGraphicsPipelines returns, and a list still growing would move them. Same trap
    // the descriptor writes hit once.
    Opal::DynamicArray<Opal::Ref<const Shader>> stage_shaders;
    if (has_vertex)
    {
        stage_shaders.PushBack(Opal::Ref<const Shader>(*desc.vertex_shader));
    }
    if (has_task)
    {
        stage_shaders.PushBack(Opal::Ref<const Shader>(*desc.task_shader));
    }
    if (has_mesh)
    {
        stage_shaders.PushBack(Opal::Ref<const Shader>(*desc.mesh_shader));
    }
    if (desc.fragment_shader != nullptr)
    {
        stage_shaders.PushBack(Opal::Ref<const Shader>(*desc.fragment_shader));
    }

    Opal::DynamicArray<bool> matched(desc.specialization.GetSize());
    const Opal::ArrayView<const SpecializationConstant> values(desc.specialization.GetData(), desc.specialization.GetSize());
    RNDR_FORGE_CHECK_EXPECTED(RequireNoDuplicateNames(values), Result);
    // Built at its final size and written by index, so no element ever moves.
    Opal::DynamicArray<StageSpecialization> stage_specializations(stage_shaders.GetSize());
    for (i32 i = 0; i < stage_shaders.GetSize(); ++i)
    {
        Opal::Expected<StageSpecialization, ErrorCode> stage_specialization =
            BuildStageSpecialization(stage_shaders[i].Get(), values, matched);
        if (!stage_specialization.HasValue())
        {
            return Result(stage_specialization.GetError());
        }
        stage_specializations[i] = std::move(stage_specialization.GetValue());
    }
    RNDR_FORGE_CHECK_EXPECTED(RequireEveryValueMatched(values, matched), Result);

    if (has_vertex)
    {
        RNDR_FORGE_CHECK_EXPECTED(RequireVertexInputMatchesShader(desc.vertex_input, *desc.vertex_shader), Result);
    }
    // Not conditional on there being any ranges: no range at all is the likeliest way to get this wrong,
    // and a shader declaring a block that the layout does not name is something Vulkan refuses outright.
    RNDR_FORGE_CHECK_EXPECTED(RequirePushConstantsCovered({desc.push_constant_ranges.GetData(), desc.push_constant_ranges.GetSize()},
                                                          {stage_shaders.GetData(), stage_shaders.GetSize()}),
                              Result);

    Opal::DynamicArray<VkPipelineShaderStageCreateInfo> shader_stages(stage_shaders.GetSize());
    for (i32 i = 0; i < stage_shaders.GetSize(); ++i)
    {
        const Shader& shader = stage_shaders[i].Get();
        shader_stages[i] = VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = shader.GetNativeShaderStage(),
            .module = shader.GetNativeShaderModule(),
            .pName = shader.GetEntryPoint().GetData(),
            // Null rather than an empty one, which is what a stage with nothing to specialize means.
            .pSpecializationInfo = stage_specializations[i].entries.IsEmpty() ? nullptr : &stage_specializations[i].info,
        };
    }

    Opal::DynamicArray<VkVertexInputBindingDescription> vk_bindings;
    Opal::DynamicArray<VkVertexInputAttributeDescription> vk_attributes;
    for (const auto& binding : desc.vertex_input.bindings)
    {
        RNDR_FORGE_TRANSLATE_EXPECTED(input_rate, ToVkVertexInputRate(binding.input_rate), "VertexInputDesc::Binding::input_rate", Result);
        vk_bindings.PushBack(VkVertexInputBindingDescription{
            .binding = binding.binding,
            .stride = binding.stride,
            .inputRate = input_rate,
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

    RNDR_FORGE_TRANSLATE_EXPECTED(topology, ToVkPrimitiveTopology(desc.topology), "GraphicsPipelineDesc::topology", Result);
    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = topology,
    };

    const VkPipelineViewportStateCreateInfo viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    // Both of these are features rather than something every device does, and asking for one the device did
    // not enable is undefined rather than a pipeline that fails to create - so it is named here instead of
    // being left to the validation layer, the way CmdSetLineWidth names wide_lines.
    if (desc.rasterizer.fill_mode == FillMode::Wireframe && !device.GetFeatures().fill_mode_non_solid)
    {
        RNDR_LOG_ERROR("Forge: a wireframe fill mode needs the device created with DeviceFeatures::fill_mode_non_solid");
        return Result(ErrorCode::InvalidArgument);
    }
    if (desc.rasterizer.depth_clamp && !device.GetFeatures().depth_clamp)
    {
        RNDR_LOG_ERROR("Forge: clamping depth needs the device created with DeviceFeatures::depth_clamp");
        return Result(ErrorCode::InvalidArgument);
    }

    RNDR_FORGE_TRANSLATE_EXPECTED(polygon_mode, ToVkPolygonMode(desc.rasterizer.fill_mode), "RasterizerDesc::fill_mode", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(cull_mode, ToVkCullMode(desc.rasterizer.cull_mode), "RasterizerDesc::cull_mode", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(front_face, ToVkFrontFace(desc.rasterizer.front_face), "RasterizerDesc::front_face", Result);
    const VkPipelineRasterizationStateCreateInfo rasterization_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = desc.rasterizer.depth_clamp ? VK_TRUE : VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = polygon_mode,
        .cullMode = cull_mode,
        .frontFace = front_face,
        .depthBiasEnable = desc.rasterizer.depth_bias_enabled ? VK_TRUE : VK_FALSE,
        .depthBiasConstantFactor = desc.rasterizer.depth_bias_constant_factor,
        .depthBiasClamp = desc.rasterizer.depth_bias_clamp,
        .depthBiasSlopeFactor = desc.rasterizer.depth_bias_slope_factor,
        .lineWidth = 1.0f,
    };

    RNDR_FORGE_TRANSLATE_EXPECTED(sample_count, ToVkSampleCount(desc.sample_count), "GraphicsPipelineDesc::sample_count", Result);
    // Colour and depth are checked together: a pipeline renders into both, and a count one of them cannot
    // carry is as unusable as one neither can.
    const VkPhysicalDeviceLimits& limits = device.GetPhysicalDevice().GetProperties().limits;
    const VkSampleCountFlags supported_counts = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    if ((supported_counts & sample_count) == 0)
    {
        RNDR_LOG_ERROR("Forge: this device does not support that many samples per pixel");
        return Result(ErrorCode::FeatureNotSupported);
    }
    const VkPipelineMultisampleStateCreateInfo multisample_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = sample_count,
    };

    const auto& ds = desc.depth_stencil;
    RNDR_FORGE_TRANSLATE_EXPECTED(depth_comparator, ToVkCompareOp(ds.depth_comparator), "DepthStencilDesc::depth_comparator", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(front_stencil_fail, ToVkStencilOp(ds.front_stencil_fail), "DepthStencilDesc::front_stencil_fail", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(front_pass, ToVkStencilOp(ds.front_pass), "DepthStencilDesc::front_pass", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(front_depth_fail, ToVkStencilOp(ds.front_depth_fail), "DepthStencilDesc::front_depth_fail", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(front_stencil_comparator, ToVkCompareOp(ds.front_stencil_comparator),
                                  "DepthStencilDesc::front_stencil_comparator", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(back_stencil_fail, ToVkStencilOp(ds.back_stencil_fail), "DepthStencilDesc::back_stencil_fail", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(back_pass, ToVkStencilOp(ds.back_pass), "DepthStencilDesc::back_pass", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(back_depth_fail, ToVkStencilOp(ds.back_depth_fail), "DepthStencilDesc::back_depth_fail", Result);
    RNDR_FORGE_TRANSLATE_EXPECTED(back_stencil_comparator, ToVkCompareOp(ds.back_stencil_comparator),
                                  "DepthStencilDesc::back_stencil_comparator", Result);
    const VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = ds.depth_test_enabled ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = ds.depth_write_enabled ? VK_TRUE : VK_FALSE,
        .depthCompareOp = depth_comparator,
        .stencilTestEnable = ds.stencil_test_enabled ? VK_TRUE : VK_FALSE,
        .front =
            {
                .failOp = front_stencil_fail,
                .passOp = front_pass,
                .depthFailOp = front_depth_fail,
                .compareOp = front_stencil_comparator,
                .compareMask = ds.front_compare_mask,
                .writeMask = ds.front_write_mask,
                .reference = ds.front_reference,
            },
        .back =
            {
                .failOp = back_stencil_fail,
                .passOp = back_pass,
                .depthFailOp = back_depth_fail,
                .compareOp = back_stencil_comparator,
                .compareMask = ds.back_compare_mask,
                .writeMask = ds.back_write_mask,
                .reference = ds.back_reference,
            },
    };

    Opal::DynamicArray<VkPipelineColorBlendAttachmentState> color_blend_attachments;
    for (const auto& cb : desc.color_blend_attachments)
    {
        RNDR_FORGE_TRANSLATE_EXPECTED(src_color, ToVkBlendFactor(cb.src_color_factor), "BlendDesc::src_color_factor", Result);
        RNDR_FORGE_TRANSLATE_EXPECTED(dst_color, ToVkBlendFactor(cb.dst_color_factor), "BlendDesc::dst_color_factor", Result);
        RNDR_FORGE_TRANSLATE_EXPECTED(color_operation, ToVkBlendOp(cb.color_operation), "BlendDesc::color_operation", Result);
        RNDR_FORGE_TRANSLATE_EXPECTED(src_alpha, ToVkBlendFactor(cb.src_alpha_factor), "BlendDesc::src_alpha_factor", Result);
        RNDR_FORGE_TRANSLATE_EXPECTED(dst_alpha, ToVkBlendFactor(cb.dst_alpha_factor), "BlendDesc::dst_alpha_factor", Result);
        RNDR_FORGE_TRANSLATE_EXPECTED(alpha_operation, ToVkBlendOp(cb.alpha_operation), "BlendDesc::alpha_operation", Result);
        color_blend_attachments.PushBack(VkPipelineColorBlendAttachmentState{
            .blendEnable = cb.blend_enabled ? VK_TRUE : VK_FALSE,
            .srcColorBlendFactor = src_color,
            .dstColorBlendFactor = dst_color,
            .colorBlendOp = color_operation,
            .srcAlphaBlendFactor = src_alpha,
            .dstAlphaBlendFactor = dst_alpha,
            .alphaBlendOp = alpha_operation,
            .colorWriteMask = static_cast<VkColorComponentFlags>(cb.color_write_mask),
        });
    }

    const VkPipelineColorBlendStateCreateInfo color_blend_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = static_cast<u32>(color_blend_attachments.GetSize()),
        .pAttachments = color_blend_attachments.GetData(),
        .blendConstants = {desc.blend_constants.x, desc.blend_constants.y, desc.blend_constants.z, desc.blend_constants.w},
    };

    // Viewport and scissor are always dynamic - nothing in the desc describes them, and CmdSetViewport and
    // CmdSetScissor are the only way to give them a value - so the desc only adds to these two.
    VkDynamicState dynamic_states[7] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    u32 dynamic_state_count = 2;
    if (!!(desc.dynamic_state & DynamicStateBits::DepthBias))
    {
        dynamic_states[dynamic_state_count++] = VK_DYNAMIC_STATE_DEPTH_BIAS;
    }
    if (!!(desc.dynamic_state & DynamicStateBits::StencilReference))
    {
        dynamic_states[dynamic_state_count++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
    }
    if (!!(desc.dynamic_state & DynamicStateBits::LineWidth))
    {
        dynamic_states[dynamic_state_count++] = VK_DYNAMIC_STATE_LINE_WIDTH;
    }
    if (!!(desc.dynamic_state & DynamicStateBits::StencilCompareMask))
    {
        dynamic_states[dynamic_state_count++] = VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
    }
    if (!!(desc.dynamic_state & DynamicStateBits::StencilWriteMask))
    {
        dynamic_states[dynamic_state_count++] = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
    }
    const VkPipelineDynamicStateCreateInfo dynamic_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamic_state_count,
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
        .layout = pipeline.m_pipeline_layout,
    };

    RNDR_FORGE_VK_CHECK_EXPECTED(
        vkCreateGraphicsPipelines(device.GetNativeDevice(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline.m_pipeline),
        "vkCreateGraphicsPipelines", Result);
    return Result(std::move(pipeline));
}

Opal::Expected<Rndr::Forge::Pipeline, Rndr::ErrorCode> Rndr::Forge::Pipeline::Create(const Device& device, const ComputePipelineDesc& desc)
{
    using Result = Opal::Expected<Pipeline, ErrorCode>;

    Pipeline pipeline;
    pipeline.m_device = device;
    pipeline.m_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
    RNDR_FORGE_CHECK_EXPECTED(pipeline.CreatePipelineLayout({desc.descriptor_set_layouts.GetData(), desc.descriptor_set_layouts.GetSize()},
                                                            {desc.push_constant_ranges.GetData(), desc.push_constant_ranges.GetSize()}),
                              Result);

    Opal::DynamicArray<bool> matched(desc.specialization.GetSize());
    const Opal::ArrayView<const SpecializationConstant> values(desc.specialization.GetData(), desc.specialization.GetSize());
    RNDR_FORGE_CHECK_EXPECTED(RequireNoDuplicateNames(values), Result);
    Opal::Expected<StageSpecialization, ErrorCode> stage_specialization_result = BuildStageSpecialization(*desc.shader, values, matched);
    if (!stage_specialization_result.HasValue())
    {
        return Result(stage_specialization_result.GetError());
    }
    const StageSpecialization& stage_specialization = stage_specialization_result.GetValue();
    RNDR_FORGE_CHECK_EXPECTED(RequireEveryValueMatched(values, matched), Result);

    const VkComputePipelineCreateInfo pipeline_create_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage =
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = desc.shader->GetNativeShaderStage(),
                .module = desc.shader->GetNativeShaderModule(),
                .pName = desc.shader->GetEntryPoint().GetData(),
                // Null rather than an empty one, which is what a stage with nothing to specialize means.
                .pSpecializationInfo = stage_specialization.entries.IsEmpty() ? nullptr : &stage_specialization.info,
            },
        .layout = pipeline.m_pipeline_layout,
    };

    RNDR_FORGE_VK_CHECK_EXPECTED(
        vkCreateComputePipelines(device.GetNativeDevice(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline.m_pipeline),
        "vkCreateComputePipelines", Result);
    return Result(std::move(pipeline));
}

Rndr::Forge::Pipeline::~Pipeline()
{
    Destroy();
}

Rndr::Forge::Pipeline::Pipeline(Pipeline&& other) noexcept
    : m_device(std::move(other.m_device)),
      m_pipeline(other.m_pipeline),
      m_pipeline_layout(other.m_pipeline_layout),
      m_bind_point(other.m_bind_point)
{
    other.m_pipeline = VK_NULL_HANDLE;
    other.m_pipeline_layout = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::Forge::Pipeline& Rndr::Forge::Pipeline::operator=(Pipeline&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_pipeline = other.m_pipeline;
        m_pipeline_layout = other.m_pipeline_layout;
        m_bind_point = other.m_bind_point;
        other.m_pipeline = VK_NULL_HANDLE;
        other.m_pipeline_layout = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::Forge::Pipeline::Destroy()
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
