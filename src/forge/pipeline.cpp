#include "rndr/forge/pipeline.hpp"

#include "opal/exceptions.h"

#include "rndr/pixel-format.hpp"

#include "rndr/forge/descriptor-set.hpp"
#include "rndr/forge/shader.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-exception.hpp"

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

/** The one place a SampleCount becomes a Vulkan bit, so a value with no counterpart cannot be cast into one. */
static VkSampleCountFlagBits ToVkSampleCount(Rndr::Forge::SampleCount sample_count)
{
    switch (sample_count)
    {
        case Rndr::Forge::SampleCount::Count1:
            return VK_SAMPLE_COUNT_1_BIT;
        case Rndr::Forge::SampleCount::Count2:
            return VK_SAMPLE_COUNT_2_BIT;
        case Rndr::Forge::SampleCount::Count4:
            return VK_SAMPLE_COUNT_4_BIT;
        case Rndr::Forge::SampleCount::Count8:
            return VK_SAMPLE_COUNT_8_BIT;
        case Rndr::Forge::SampleCount::Count16:
            return VK_SAMPLE_COUNT_16_BIT;
        case Rndr::Forge::SampleCount::Count32:
            return VK_SAMPLE_COUNT_32_BIT;
        case Rndr::Forge::SampleCount::Count64:
            return VK_SAMPLE_COUNT_64_BIT;
    }
    throw Opal::Exception("Unknown sample count!");
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

Rndr::Forge::VertexInputDesc::Binding& Rndr::Forge::VertexInputDesc::AddBinding(u32 binding, u32 stride, DataRepetition input_rate)
{
    bindings.PushBack(Binding{.binding = binding, .stride = stride, .input_rate = input_rate});
    return bindings[bindings.GetSize() - 1];
}

void Rndr::Forge::VertexInputDesc::AddAttribute(u32 binding, u32 location, PixelFormat format, u32 offset)
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

Rndr::Forge::VertexInputDesc Rndr::Forge::VertexInputDesc::FromShader(const Shader& vertex_shader, u32 binding,
                                                                      DataRepetition input_rate)
{
    if (vertex_shader.GetShaderStage() != ShaderTypeBits::Vertex)
    {
        throw Opal::Exception("Only a vertex shader is fed from a vertex buffer, so only one has attributes to read!");
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
            throw Opal::Exception(Opal::StringEx("Vertex attribute ") + input.name.GetData() +
                                  " has a format with no size, so where the next one starts is not something this can work out!");
        }
        target.attributes.PushBack(Attribute{.location = input.location, .format = input.format, .offset = offset});
        offset += size;
    }
    // Tightly packed, so the stride is what the last attribute ends at.
    target.stride = offset;
    return desc;
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
                ranges.PushBack(PushConstantRange{
                    .shader_stages = shader.GetShaderStage(), .offset = block.offset, .size = block.size});
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
void RequireVertexInputMatchesShader(const Rndr::Forge::VertexInputDesc& vertex_input, const Rndr::Forge::Shader& vertex_shader)
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
            throw Opal::Exception(Opal::StringEx("The vertex shader reads ") + input.name.GetData() + " at location " +
                                  input.location + " and no vertex attribute feeds it!");
        }
        const Rndr::FormatNumericClass wanted = Rndr::GetFormatNumericClass(input.format);
        const Rndr::FormatNumericClass given = Rndr::GetFormatNumericClass(attribute->format);
        if (wanted != given)
        {
            throw Opal::Exception(Opal::StringEx("The vertex attribute at location ") + input.location +
                                  " does not have the numeric class the shader reads " + input.name.GetData() + " as!");
        }
    }
}

/**
 * Check that the supplied ranges cover every push constant block the shaders read. A range stopping short of
 * the block is the quiet half of this - Vulkan refuses a layout that names no stage at all for a declared
 * block, but a range four bytes too short for what the shader reads goes through.
 */
void RequirePushConstantsCovered(Opal::ArrayView<const Rndr::Forge::PushConstantRange> ranges,
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
                throw Opal::Exception(Opal::StringEx("A shader reads a push constant block of ") + block.size + " bytes at offset " +
                                      block.offset + " and no push constant range of this pipeline covers it!");
            }
        }
    }
}
}  // namespace

void Rndr::Forge::Pipeline::CreatePipelineLayout(
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
    const VkResult result = vkCreatePipelineLayout(m_device->GetNativeDevice(), &layout_create_info, nullptr, &m_pipeline_layout);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreatePipelineLayout");
    }
}

namespace
{
/**
 * The pipeline layout is created before the checks that can throw, and a constructor that throws is never
 * a destructor's to clean up - the object it would have built never came to be. This releases the layout
 * on the way out of such a constructor, and the constructor that reaches its end dismisses it.
 */
class PipelineLayoutGuard
{
public:
    PipelineLayoutGuard(VkDevice device, VkPipelineLayout& layout) : m_device(device), m_layout(&layout) {}

    ~PipelineLayoutGuard()
    {
        if (m_layout != nullptr && *m_layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_device, *m_layout, nullptr);
            *m_layout = VK_NULL_HANDLE;
        }
    }

    // A scope, not a value: copying one would release the layout twice and moving it would leave the
    // question of which copy owns it.
    PipelineLayoutGuard(const PipelineLayoutGuard&) = delete;
    PipelineLayoutGuard& operator=(const PipelineLayoutGuard&) = delete;
    PipelineLayoutGuard(PipelineLayoutGuard&&) = delete;
    PipelineLayoutGuard& operator=(PipelineLayoutGuard&&) = delete;

    /** The pipeline is built and owns its layout, so leaving the scope must not release it. */
    void Dismiss() { m_layout = nullptr; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout* m_layout = nullptr;
};

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
void RequireValueFits(const Rndr::Forge::SpecializationConstant& value, Rndr::u32 byte_size)
{
    if (byte_size >= 4)
    {
        return;
    }
    const Rndr::u64 bits = value.value.bits;
    const Rndr::u32 width = byte_size * 8;
    const bool fits = value.value.type == Rndr::Forge::SpecializationType::Int32
                          ? static_cast<Rndr::i64>(static_cast<Rndr::i32>(bits)) >= -(Rndr::i64{1} << (width - 1)) &&
                                static_cast<Rndr::i64>(static_cast<Rndr::i32>(bits)) < (Rndr::i64{1} << (width - 1))
                          : bits < (Rndr::u64{1} << width);
    if (!fits)
    {
        throw Opal::Exception(Opal::StringEx("Specialization constant ") + reinterpret_cast<const char*>(value.name.GetData()) +
                              " is declared " + width + " bits wide and the value given does not fit in it!");
    }
}

/**
 * Two values under one name would become two map entries with the same constantID, which the specification
 * does not allow within one VkSpecializationInfo. Caught here rather than left to the validation layer,
 * since by name it is nothing more exotic than the same name written twice.
 */
void RequireNoDuplicateNames(Opal::ArrayView<const Rndr::Forge::SpecializationConstant> values)
{
    for (Rndr::i32 i = 0; i < values.GetSize(); ++i)
    {
        for (Rndr::i32 j = i + 1; j < values.GetSize(); ++j)
        {
            if (values[i].name == values[j].name)
            {
                throw Opal::Exception(Opal::StringEx("Specialization constant ") + reinterpret_cast<const char*>(values[i].name.GetData()) +
                                      " was given a value twice!");
            }
        }
    }
}

/**
 * Match the values against what this shader declares and pack the ones that belong to it. A value naming a
 * constant this stage does not have is skipped rather than refused - another stage may declare it, and the
 * caller is told about a name no stage at all declared once every stage has been looked at.
 *
 * @param out_matched One flag per value, set when this stage took it. Never cleared, so it accumulates.
 */
StageSpecialization BuildStageSpecialization(const Rndr::Forge::Shader& shader,
                                             Opal::ArrayView<const Rndr::Forge::SpecializationConstant> values,
                                             Opal::DynamicArray<bool>& out_matched)
{
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
                throw Opal::Exception(Opal::StringEx("Specialization constant ") + reinterpret_cast<const char*>(value.name.GetData()) +
                                      " is declared as " + SpecializationTypeName(info.type) + " but was given a " +
                                      SpecializationTypeName(value.value.type) + "!");
            }
            // The declared width, not the width of the value: VkSpecializationMapEntry::size has to match
            // the type the shader declared, and an 8 or 16 bit constant is reported as Int32 or UInt32.
            const Rndr::u32 size = info.byte_size;
            RequireValueFits(value, size);
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
    return result;
}

/** Fails on the first value no stage of the pipeline claimed, which Vulkan would have ignored in silence. */
void RequireEveryValueMatched(Opal::ArrayView<const Rndr::Forge::SpecializationConstant> values,
                              const Opal::DynamicArray<bool>& matched)
{
    for (Rndr::i32 i = 0; i < values.GetSize(); ++i)
    {
        if (!matched[i])
        {
            throw Opal::Exception(Opal::StringEx("No shader of this pipeline declares a specialization constant called ") +
                                  reinterpret_cast<const char*>(values[i].name.GetData()) + "!");
        }
    }
}
}  // namespace

Rndr::Forge::Pipeline::Pipeline(const Device& device, const GraphicsPipelineDesc& desc)
    : m_device(device), m_bind_point(VK_PIPELINE_BIND_POINT_GRAPHICS)
{
    CreatePipelineLayout(
        {desc.descriptor_set_layouts.GetData(), desc.descriptor_set_layouts.GetSize()},
        {desc.push_constant_ranges.GetData(), desc.push_constant_ranges.GetSize()});
    PipelineLayoutGuard layout_guard(m_device->GetNativeDevice(), m_pipeline_layout);

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
    RequireNoDuplicateNames(values);
    // Built at its final size and written by index, so no element ever moves.
    Opal::DynamicArray<StageSpecialization> stage_specializations(stage_shaders.GetSize());
    for (i32 i = 0; i < stage_shaders.GetSize(); ++i)
    {
        stage_specializations[i] = BuildStageSpecialization(stage_shaders[i].Get(), values, matched);
    }
    RequireEveryValueMatched(values, matched);

    if (has_vertex)
    {
        RequireVertexInputMatchesShader(desc.vertex_input, *desc.vertex_shader);
    }
    // Not conditional on there being any ranges: no range at all is the likeliest way to get this wrong,
    // and a shader declaring a block that the layout does not name is something Vulkan refuses outright.
    RequirePushConstantsCovered({desc.push_constant_ranges.GetData(), desc.push_constant_ranges.GetSize()},
                                {stage_shaders.GetData(), stage_shaders.GetSize()});

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

    // Both of these are features rather than something every device does, and asking for one the device did
    // not enable is undefined rather than a pipeline that fails to create - so it is named here instead of
    // being left to the validation layer, the way CmdSetLineWidth names wide_lines.
    if (desc.rasterizer.fill_mode == FillMode::Wireframe && !device.GetFeatures().fill_mode_non_solid)
    {
        throw Opal::Exception("A wireframe fill mode needs the device created with DeviceFeatures::fill_mode_non_solid!");
    }
    if (desc.rasterizer.depth_clamp && !device.GetFeatures().depth_clamp)
    {
        throw Opal::Exception("Clamping depth needs the device created with DeviceFeatures::depth_clamp!");
    }

    const VkPipelineRasterizationStateCreateInfo rasterization_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = desc.rasterizer.depth_clamp ? VK_TRUE : VK_FALSE,
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

    const VkSampleCountFlagBits sample_count = ToVkSampleCount(desc.sample_count);
    // Colour and depth are checked together: a pipeline renders into both, and a count one of them cannot
    // carry is as unusable as one neither can.
    const VkPhysicalDeviceLimits& limits = device.GetPhysicalDevice().GetProperties().limits;
    const VkSampleCountFlags supported_counts = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    if ((supported_counts & sample_count) == 0)
    {
        throw Opal::Exception("This device does not support that many samples per pixel!");
    }
    const VkPipelineMultisampleStateCreateInfo multisample_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = sample_count,
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
                .compareMask = ds.front_compare_mask,
                .writeMask = ds.front_write_mask,
                .reference = ds.front_reference,
            },
        .back =
            {
                .failOp = ToVkStencilOp(ds.back_stencil_fail),
                .passOp = ToVkStencilOp(ds.back_pass),
                .depthFailOp = ToVkStencilOp(ds.back_depth_fail),
                .compareOp = ToVkCompareOp(ds.back_stencil_comparator),
                .compareMask = ds.back_compare_mask,
                .writeMask = ds.back_write_mask,
                .reference = ds.back_reference,
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
            .colorWriteMask = static_cast<VkColorComponentFlags>(cb.color_write_mask),
        });
    }

    const VkPipelineColorBlendStateCreateInfo color_blend_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = static_cast<u32>(color_blend_attachments.GetSize()),
        .pAttachments = color_blend_attachments.GetData(),
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
        .layout = m_pipeline_layout,
    };

    const VkResult gfx_result =
        vkCreateGraphicsPipelines(m_device->GetNativeDevice(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &m_pipeline);
    if (gfx_result != VK_SUCCESS)
    {
        throw VulkanException(gfx_result, "vkCreateGraphicsPipelines");
    }
    layout_guard.Dismiss();
}

Rndr::Forge::Pipeline::Pipeline(const Device& device, const ComputePipelineDesc& desc)
    : m_device(device), m_bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
{
    CreatePipelineLayout(
        {desc.descriptor_set_layouts.GetData(), desc.descriptor_set_layouts.GetSize()},
        {desc.push_constant_ranges.GetData(), desc.push_constant_ranges.GetSize()});
    PipelineLayoutGuard layout_guard(m_device->GetNativeDevice(), m_pipeline_layout);

    Opal::DynamicArray<bool> matched(desc.specialization.GetSize());
    const Opal::ArrayView<const SpecializationConstant> values(desc.specialization.GetData(), desc.specialization.GetSize());
    RequireNoDuplicateNames(values);
    const StageSpecialization stage_specialization = BuildStageSpecialization(*desc.shader, values, matched);
    RequireEveryValueMatched(values, matched);

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
        .layout = m_pipeline_layout,
    };

    const VkResult compute_result =
        vkCreateComputePipelines(m_device->GetNativeDevice(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &m_pipeline);
    if (compute_result != VK_SUCCESS)
    {
        throw VulkanException(compute_result, "vkCreateComputePipelines");
    }
    layout_guard.Dismiss();
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
