#include "rndr/core/shader-compiler.hpp"

#include "slang-com-ptr.h"
#include "slang.h"

#include "rndr/exception.hpp"
#include "rndr/log.hpp"

#include <cstring>

namespace
{

Rndr::ShaderStage FromSlangStage(SlangStage stage)
{
    switch (stage)
    {
        case SLANG_STAGE_VERTEX:
            return Rndr::ShaderStage::Vertex;
        case SLANG_STAGE_FRAGMENT:
            return Rndr::ShaderStage::Fragment;
        case SLANG_STAGE_COMPUTE:
            return Rndr::ShaderStage::Compute;
        default:
            return Rndr::ShaderStage::Unknown;
    }
}

Rndr::ParameterCategory CategorizeFromType(slang::TypeLayoutReflection* type_layout)
{
    if (type_layout == nullptr)
    {
        return Rndr::ParameterCategory::EnumCount;
    }
    const slang::TypeReflection::Kind kind = type_layout->getType()->getKind();
    switch (kind)
    {
        case slang::TypeReflection::Kind::SamplerState:
            return Rndr::ParameterCategory::Sampler;
        case slang::TypeReflection::Kind::Resource:
        {
            const auto shape = static_cast<SlangResourceShape>(type_layout->getType()->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK);
            if (shape == SLANG_STRUCTURED_BUFFER || shape == SLANG_BYTE_ADDRESS_BUFFER)
            {
                return Rndr::ParameterCategory::StorageBuffer;
            }
            const SlangResourceAccess access = type_layout->getType()->getResourceAccess();
            if (access == SLANG_RESOURCE_ACCESS_READ_WRITE)
            {
                return Rndr::ParameterCategory::StorageBuffer;
            }
            return Rndr::ParameterCategory::Texture;
        }
        case slang::TypeReflection::Kind::ConstantBuffer:
        case slang::TypeReflection::Kind::ParameterBlock:
            return Rndr::ParameterCategory::Uniform;
        default:
            return Rndr::ParameterCategory::EnumCount;
    }
}

Rndr::ParameterCategory FromSlangCategory(slang::VariableLayoutReflection* param)
{
    const slang::ParameterCategory category = param->getCategory();
    switch (category)
    {
        case slang::ParameterCategory::Uniform:
        case slang::ParameterCategory::ConstantBuffer:
        case slang::ParameterCategory::PushConstantBuffer:
            return Rndr::ParameterCategory::Uniform;
        case slang::ParameterCategory::ShaderResource:
            return CategorizeFromType(param->getTypeLayout());
        case slang::ParameterCategory::SamplerState:
            return Rndr::ParameterCategory::Sampler;
        case slang::ParameterCategory::UnorderedAccess:
            return Rndr::ParameterCategory::StorageBuffer;
        case slang::ParameterCategory::VaryingInput:
            return Rndr::ParameterCategory::VaryingInput;
        case slang::ParameterCategory::VaryingOutput:
            return Rndr::ParameterCategory::VaryingOutput;
        case slang::ParameterCategory::DescriptorTableSlot:
        case slang::ParameterCategory::SubElementRegisterSpace:
            return CategorizeFromType(param->getTypeLayout());
        default:
            return Rndr::ParameterCategory::EnumCount;
    }
}

void ExtractUniformFields(slang::TypeLayoutReflection* type_layout, Rndr::i32 binding_index, Rndr::i32 binding_space,
                          Opal::DynamicArray<Rndr::ShaderParameter>& out_params)
{
    slang::TypeLayoutReflection* element_layout = type_layout->getElementTypeLayout();
    if (element_layout == nullptr)
    {
        return;
    }
    const unsigned field_count = element_layout->getFieldCount();
    for (unsigned i = 0; i < field_count; ++i)
    {
        slang::VariableLayoutReflection* field = element_layout->getFieldByIndex(i);
        Rndr::ShaderParameter sp;
        sp.name = field->getName();
        sp.binding_index = binding_index;
        sp.binding_space = binding_space;
        sp.offset = static_cast<Rndr::i32>(field->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM));
        sp.size = static_cast<Rndr::i32>(field->getTypeLayout()->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
        sp.category = Rndr::ParameterCategory::Uniform;

        slang::TypeReflection* field_type = field->getTypeLayout()->getType();
        if (field_type->getKind() == slang::TypeReflection::Kind::Array)
        {
            const auto element_count = static_cast<Rndr::i32>(field_type->getElementCount());
            if (element_count > 0)
            {
                sp.array_element_count = element_count;
                sp.array_stride = sp.size / element_count;
            }
        }

        out_params.PushBack(std::move(sp));
    }
}

void ExtractParam(slang::VariableLayoutReflection* param, Opal::DynamicArray<Rndr::ShaderParameter>& out_params)
{
    const Rndr::ParameterCategory category = FromSlangCategory(param);
    if (category == Rndr::ParameterCategory::EnumCount)
    {
        return;
    }
    const Rndr::i32 binding_index = static_cast<Rndr::i32>(param->getBindingIndex());
    const Rndr::i32 binding_space = static_cast<Rndr::i32>(param->getBindingSpace());

    Rndr::ShaderParameter sp;
    sp.name = param->getName();
    sp.binding_index = binding_index;
    sp.binding_space = binding_space;
    sp.category = category;

    if (category == Rndr::ParameterCategory::Uniform)
    {
        slang::TypeLayoutReflection* type_layout = param->getTypeLayout();
        if (type_layout != nullptr)
        {
            const slang::TypeReflection::Kind kind = type_layout->getType()->getKind();
            if (kind == slang::TypeReflection::Kind::ConstantBuffer || kind == slang::TypeReflection::Kind::ParameterBlock)
            {
                out_params.PushBack(std::move(sp));
                ExtractUniformFields(type_layout, binding_index, binding_space, out_params);
                return;
            }

            sp.offset = static_cast<Rndr::i32>(param->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM));
            sp.size = static_cast<Rndr::i32>(type_layout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));

            slang::TypeReflection* type = type_layout->getType();
            if (type->getKind() == slang::TypeReflection::Kind::Array)
            {
                const auto element_count = static_cast<Rndr::i32>(type->getElementCount());
                if (element_count > 0)
                {
                    sp.array_element_count = element_count;
                    sp.array_stride = sp.size / element_count;
                }
            }
        }
    }

    out_params.PushBack(std::move(sp));
}

void ExtractParameters(slang::ProgramLayout* layout, Opal::DynamicArray<Rndr::ShaderParameter>& out_params)
{
    for (unsigned i = 0; i < layout->getParameterCount(); ++i)
    {
        ExtractParam(layout->getParameterByIndex(i), out_params);
    }

    if (layout->getEntryPointCount() > 0)
    {
        slang::EntryPointReflection* ep = layout->getEntryPointByIndex(0);
        for (unsigned i = 0; i < ep->getParameterCount(); ++i)
        {
            ExtractParam(ep->getParameterByIndex(i), out_params);
        }
    }
}

// ---------------------------------------------------------------------------
// Vertex input attribute extraction from reflection.
// ---------------------------------------------------------------------------

Rndr::ScalarType FromSlangScalarType(slang::TypeReflection::ScalarType scalar)
{
    switch (scalar)
    {
        case slang::TypeReflection::Float32:
            return Rndr::ScalarType::Float32;
        case slang::TypeReflection::Int32:
            return Rndr::ScalarType::Int32;
        default:
            return Rndr::ScalarType::Unknown;
    }
}

void ExtractVertexInputFromType(const char* name, slang::TypeLayoutReflection* type_layout,
                                Opal::DynamicArray<Rndr::VertexInputAttribute>& out_inputs)
{
    slang::TypeReflection* type = type_layout->getType();
    const slang::TypeReflection::Kind kind = type->getKind();

    Rndr::VertexInputAttribute attr;
    attr.name = name;

    if (kind == slang::TypeReflection::Kind::Vector)
    {
        attr.component_count = static_cast<Rndr::u8>(type->getElementCount());
        attr.scalar_type = FromSlangScalarType(type->getElementType()->getScalarType());
    }
    else if (kind == slang::TypeReflection::Kind::Scalar)
    {
        attr.component_count = 1;
        attr.scalar_type = FromSlangScalarType(type->getScalarType());
    }
    else
    {
        return;
    }

    out_inputs.PushBack(std::move(attr));
}

void ExtractVertexInputs(slang::ProgramLayout* layout, Opal::DynamicArray<Rndr::VertexInputAttribute>& out_inputs)
{
    if (layout == nullptr || layout->getEntryPointCount() == 0)
    {
        return;
    }

    slang::EntryPointReflection* ep = layout->getEntryPointByIndex(0);
    for (unsigned i = 0; i < ep->getParameterCount(); ++i)
    {
        slang::VariableLayoutReflection* param = ep->getParameterByIndex(i);
        if (param->getCategory() != slang::ParameterCategory::VaryingInput)
        {
            continue;
        }

        slang::TypeLayoutReflection* type_layout = param->getTypeLayout();
        if (type_layout == nullptr)
        {
            continue;
        }

        slang::TypeReflection* type = type_layout->getType();
        if (type->getKind() == slang::TypeReflection::Kind::Struct)
        {
            const unsigned field_count = type_layout->getFieldCount();
            for (unsigned j = 0; j < field_count; ++j)
            {
                slang::VariableLayoutReflection* field = type_layout->getFieldByIndex(j);
                ExtractVertexInputFromType(field->getName(), field->getTypeLayout(), out_inputs);
            }
        }
        else
        {
            ExtractVertexInputFromType(param->getName(), type_layout, out_inputs);
        }
    }
}

// ---------------------------------------------------------------------------
// Merge parameters helpers.
// ---------------------------------------------------------------------------

bool IsTopLevelResource(const Rndr::ShaderParameter& p)
{
    switch (p.category)
    {
        case Rndr::ParameterCategory::Texture:
        case Rndr::ParameterCategory::Sampler:
        case Rndr::ParameterCategory::StorageBuffer:
            return true;
        case Rndr::ParameterCategory::Uniform:
            return p.size == 0;
        default:
            return false;
    }
}

Rndr::ShaderParameter CloneParameter(const Rndr::ShaderParameter& p)
{
    Rndr::ShaderParameter copy;
    copy.name = p.name.Clone();
    copy.binding_index = p.binding_index;
    copy.binding_space = p.binding_space;
    copy.offset = p.offset;
    copy.size = p.size;
    copy.array_element_count = p.array_element_count;
    copy.array_stride = p.array_stride;
    copy.category = p.category;
    return copy;
}

// ---------------------------------------------------------------------------
// GLSL post-processing.
// ---------------------------------------------------------------------------

// Slang lowers SV_InstanceID / SV_VulkanInstanceID to the Vulkan-GLSL builtin gl_InstanceIndex even
// when targeting OpenGL GLSL, but gl_InstanceIndex only exists in Vulkan GLSL - desktop GL has no
// such builtin and the shader fails to compile. The Canvas backend issues only non-base-instance
// instanced draws (glDraw*Instanced), so base instance is always 0 and the OpenGL-core gl_InstanceID
// is an exact equivalent. Rewrite every whole-token occurrence in place.
//
// The replacement is shorter than the token, so we compact into a scratch buffer (always reading from
// the original text) and swap it back, which keeps the token-boundary checks reading unmodified bytes.
void PatchGlslInstanceBuiltin(Opal::DynamicArray<Rndr::u8>& code)
{
    static constexpr char k_from[] = "gl_InstanceIndex";
    static constexpr char k_to[] = "gl_InstanceID";
    constexpr Rndr::u64 from_len = sizeof(k_from) - 1;
    constexpr Rndr::u64 to_len = sizeof(k_to) - 1;

    const Rndr::u64 size = code.GetSize();
    if (size < from_len)
    {
        return;
    }

    const auto is_ident = [](Rndr::u8 c)
    { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; };

    const Rndr::u8* src = code.GetData();
    Opal::DynamicArray<Rndr::u8> out;
    out.Resize(size);  // The output is never longer than the input.
    Rndr::u8* dst = out.GetData();

    Rndr::u64 read = 0;
    Rndr::u64 write = 0;
    while (read < size)
    {
        const bool token_match = read + from_len <= size && memcmp(src + read, k_from, from_len) == 0 &&
                                 (read == 0 || !is_ident(src[read - 1])) &&
                                 (read + from_len == size || !is_ident(src[read + from_len]));
        if (token_match)
        {
            memcpy(dst + write, k_to, to_len);
            write += to_len;
            read += from_len;
        }
        else
        {
            dst[write++] = src[read++];
        }
    }

    out.Resize(write);
    code = std::move(out);
}

}  // namespace

// ---------------------------------------------------------------------------
// ShaderCompiler::Impl
// ---------------------------------------------------------------------------

namespace
{
/**
 * The one Slang global session this process has. Creating it loads Slang's core module, which costs about
 * 650 ms in a debug build - per compile, before this was shared, and it is the same session whatever is
 * being compiled. Sessions and modules stay per compile, since the SPIR-V and GLSL paths configure
 * different targets from it.
 *
 * Not synchronized. Slang does not document IGlobalSession as safe for concurrent use, and nothing in Rndr
 * compiles shaders off the main thread; the day something does, this needs a mutex rather than a comment.
 *
 * Released after main, along with every other static. If Slang's teardown ever turns out to mind that, the
 * fix is an explicit shutdown call rather than leaning harder on static destruction order.
 */
slang::IGlobalSession& GetGlobalSession()
{
    static Slang::ComPtr<slang::IGlobalSession> global_session = []
    {
        Slang::ComPtr<slang::IGlobalSession> session;
        const SlangResult result = slang::createGlobalSession(session.writeRef());
        if (SLANG_FAILED(result))
        {
            throw Rndr::GraphicsAPIException(result, "Failed to create Slang global session!");
        }
        return session;
    }();
    return *global_session;
}
}  // namespace

struct Rndr::ShaderCompiler::Impl
{
    Slang::ComPtr<slang::ISession> session;
    slang::IModule* module = nullptr;
    ShaderOutputFormat format = ShaderOutputFormat::SpirV;
};

// ---------------------------------------------------------------------------
// ShaderCompiler
// ---------------------------------------------------------------------------

Rndr::ShaderCompiler::ShaderCompiler() : m_impl(new Impl) {}

Rndr::ShaderCompiler::~ShaderCompiler()
{
    delete m_impl;
}

Rndr::ShaderCompiler::ShaderCompiler(ShaderCompiler&& other) noexcept : m_impl(other.m_impl)
{
    other.m_impl = nullptr;
}

Rndr::ShaderCompiler& Rndr::ShaderCompiler::operator=(ShaderCompiler&& other) noexcept
{
    if (this != &other)
    {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

void Rndr::ShaderCompiler::LoadModule(const Opal::StringUtf8& source, ShaderOutputFormat format)
{
    m_impl->format = format;

    slang::IGlobalSession& global_session = GetGlobalSession();

    slang::TargetDesc target_desc = {};
    // Preserve original Slang entry-point names in the emitted SPIR-V instead of renaming them
    // all to "main". This lets downstream consumers (e.g. spirv-reflect) look up entry points by
    // their original name. GLSL has no such option - its entry point is always "main".
    slang::CompilerOptionEntry session_options[] = {
        {slang::CompilerOptionName::VulkanUseEntryPointName, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}},
    };

    slang::SessionDesc session_desc = {};
    if (format == ShaderOutputFormat::Glsl)
    {
        // OpenGL-flavoured GLSL (gl_VertexID/gl_InstanceID semantics, layout(binding=...)), targeting
        // GL 4.5. No SPIR-V-specific flags or options are needed here.
        target_desc.format = SLANG_GLSL;
        target_desc.profile = global_session.findProfile("glsl_450");
    }
    else
    {
        target_desc.format = SLANG_SPIRV;
        target_desc.profile = global_session.findProfile("spirv_1_5");
        target_desc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
        session_desc.compilerOptionEntries = session_options;
        session_desc.compilerOptionEntryCount = sizeof(session_options) / sizeof(session_options[0]);
    }
    session_desc.targets = &target_desc;
    session_desc.targetCount = 1;
    // No search paths, so the string below is the whole of what Slang sees and hashing it is exact. Setting
    // searchPaths here would let a module pull in a file this never reads, and ShaderCacheKey would go on
    // claiming a hit for a source that changed underneath it.

    const SlangResult result = global_session.createSession(session_desc, m_impl->session.writeRef());
    if (SLANG_FAILED(result))
    {
        throw GraphicsAPIException(result, "Failed to create Slang session!");
    }

    Slang::ComPtr<ISlangBlob> diagnostics;
    m_impl->module =
        m_impl->session->loadModuleFromSourceString("shader_module", "shader_module.slang", *source, diagnostics.writeRef());
    if (m_impl->module == nullptr)
    {
        Opal::StringUtf8 msg = "Failed to load Slang module!";
        if (diagnostics != nullptr)
        {
            msg = msg + "\n" + static_cast<const char*>(diagnostics->getBufferPointer());
        }
        RNDR_LOG_ERROR("{}", *msg);
        throw GraphicsAPIException(0, *msg);
    }
}

Opal::DynamicArray<Rndr::EntryPointInfo> Rndr::ShaderCompiler::DiscoverEntryPoints() const
{
    Opal::DynamicArray<EntryPointInfo> entries;

    const SlangInt32 count = m_impl->module->getDefinedEntryPointCount();
    for (SlangInt32 i = 0; i < count; ++i)
    {
        Slang::ComPtr<slang::IEntryPoint> ep;
        m_impl->module->getDefinedEntryPoint(i, ep.writeRef());
        if (ep == nullptr)
        {
            continue;
        }

        slang::IComponentType* components[] = {m_impl->module, ep.get()};
        Slang::ComPtr<slang::IComponentType> linked;
        const SlangResult result = m_impl->session->createCompositeComponentType(components, 2, linked.writeRef(), nullptr);
        if (SLANG_FAILED(result))
        {
            continue;
        }

        slang::ProgramLayout* layout = linked->getLayout();
        if (layout != nullptr && layout->getEntryPointCount() > 0)
        {
            slang::EntryPointReflection* ep_ref = layout->getEntryPointByIndex(0);
            EntryPointInfo info;
            info.name = ep_ref->getName();
            info.stage = FromSlangStage(ep_ref->getStage());
            entries.PushBack(std::move(info));
        }
    }

    return entries;
}

Rndr::CompileResult Rndr::ShaderCompiler::CompileEntryPoint(const Opal::StringUtf8& entry_point) const
{
    Slang::ComPtr<slang::IEntryPoint> ep;
    SlangResult result = m_impl->module->findEntryPointByName(*entry_point, ep.writeRef());
    if (SLANG_FAILED(result))
    {
        Opal::StringUtf8 msg = Opal::StringUtf8("Failed to find entry point '") + entry_point + "'!";
        throw GraphicsAPIException(result, msg.GetData());
    }

    slang::IComponentType* components[] = {m_impl->module, ep};
    Slang::ComPtr<slang::IComponentType> linked_program;
    Slang::ComPtr<ISlangBlob> diagnostics;
    result = m_impl->session->createCompositeComponentType(components, 2, linked_program.writeRef(), diagnostics.writeRef());
    if (SLANG_FAILED(result))
    {
        Opal::StringUtf8 msg = "Failed to link Slang program!";
        if (diagnostics != nullptr)
        {
            msg = msg + "\n" + static_cast<const char*>(diagnostics->getBufferPointer());
        }
        throw GraphicsAPIException(result, msg.GetData());
    }

    Slang::ComPtr<ISlangBlob> code_blob;
    result = linked_program->getEntryPointCode(0, 0, code_blob.writeRef(), diagnostics.writeRef());
    if (SLANG_FAILED(result))
    {
        Opal::StringUtf8 msg = "Failed to get entry point code!";
        if (diagnostics != nullptr)
        {
            msg = msg + "\n" + static_cast<const char*>(diagnostics->getBufferPointer());
        }
        throw GraphicsAPIException(result, msg.GetData());
    }

    CompileResult out;

    // Copy the compiled code (SPIR-V bytecode or GLSL text) into an owning array.
    const auto* code_data = static_cast<const u8*>(code_blob->getBufferPointer());
    const u64 code_size = code_blob->getBufferSize();
    out.code.Resize(code_size);
    memcpy(out.code.GetData(), code_data, code_size);

    if (m_impl->format == ShaderOutputFormat::Glsl)
    {
        PatchGlslInstanceBuiltin(out.code);
    }

    slang::ProgramLayout* layout = linked_program->getLayout();
    if (layout != nullptr)
    {
        if (layout->getEntryPointCount() > 0)
        {
            slang::EntryPointReflection* ep_reflection = layout->getEntryPointByIndex(0);
            out.stage = FromSlangStage(ep_reflection->getStage());
        }

        ExtractParameters(layout, out.parameters);

        if (out.stage == ShaderStage::Vertex)
        {
            ExtractVertexInputs(layout, out.vertex_inputs);
        }
        if (out.stage == ShaderStage::Compute)
        {
            slang::EntryPointReflection* ep_ref = layout->getEntryPointByIndex(0);
            SlangUInt thread_group_size[3];
            ep_ref->getComputeThreadGroupSize(3, thread_group_size);
            out.num_threads.x = static_cast<u32>(thread_group_size[0]);
            out.num_threads.y = static_cast<u32>(thread_group_size[1]);
            out.num_threads.z = static_cast<u32>(thread_group_size[2]);
        }
    }

    return out;
}

Opal::StringUtf8 Rndr::ShaderCompiler::FindSingleEntryPoint(const Opal::DynamicArray<EntryPointInfo>& entries,
                                                              ShaderStage target_stage, const char* stage_name)
{
    Opal::StringUtf8 found_name;
    int count = 0;
    for (u64 i = 0; i < entries.GetSize(); ++i)
    {
        if (entries[i].stage == target_stage)
        {
            found_name = entries[i].name.Clone();
            ++count;
        }
    }

    if (count == 0)
    {
        Opal::StringUtf8 msg = Opal::StringUtf8("No ") + stage_name + " entry point found in shader source!";
        throw GraphicsAPIException(0, msg.GetData());
    }
    if (count > 1)
    {
        Opal::StringUtf8 msg = Opal::StringUtf8("Multiple ") + stage_name + " entry points found, expected exactly 1!";
        throw GraphicsAPIException(0, msg.GetData());
    }

    return found_name;
}

Opal::DynamicArray<Rndr::ShaderParameter> Rndr::ShaderCompiler::MergeParameters(
    const Opal::DynamicArray<ShaderParameter>& stage_a_params, const Opal::DynamicArray<ShaderParameter>& stage_b_params)
{
    Opal::DynamicArray<ShaderParameter> merged;

    // Include stage_a parameters, skip VaryingOutput (inter-stage).
    for (u64 i = 0; i < stage_a_params.GetSize(); ++i)
    {
        const ShaderParameter& p = stage_a_params[i];
        if (p.category != ParameterCategory::VaryingOutput)
        {
            merged.PushBack(CloneParameter(p));
        }
    }

    // Include stage_b parameters, skip VaryingInput (inter-stage), check for conflicts.
    for (u64 i = 0; i < stage_b_params.GetSize(); ++i)
    {
        const ShaderParameter& fp = stage_b_params[i];
        if (fp.category == ParameterCategory::VaryingInput)
        {
            continue;
        }

        bool duplicate = false;
        for (u64 j = 0; j < merged.GetSize(); ++j)
        {
            const ShaderParameter& ep = merged[j];
            if (ep.name == fp.name)
            {
                if (ep.category != fp.category)
                {
                    Opal::StringUtf8 msg =
                        Opal::StringUtf8("Parameter '") + fp.name + "' has conflicting types in vertex and fragment stages!";
                    throw GraphicsAPIException(0, msg.GetData());
                }
                duplicate = true;
                break;
            }
        }

        if (!duplicate)
        {
            if (IsTopLevelResource(fp))
            {
                for (u64 j = 0; j < merged.GetSize(); ++j)
                {
                    const ShaderParameter& ep = merged[j];
                    if (IsTopLevelResource(ep) && ep.binding_index == fp.binding_index && ep.binding_space == fp.binding_space &&
                        ep.category != fp.category)
                    {
                        throw GraphicsAPIException(
                            0, "Binding slot conflict between shader stages! Same binding slot used for different types.");
                    }
                }
            }
            merged.PushBack(CloneParameter(fp));
        }
    }

    return merged;
}