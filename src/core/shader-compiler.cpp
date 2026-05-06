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

}  // namespace

// ---------------------------------------------------------------------------
// ShaderCompiler::Impl
// ---------------------------------------------------------------------------

struct Rndr::ShaderCompiler::Impl
{
    Slang::ComPtr<slang::IGlobalSession> global_session;
    Slang::ComPtr<slang::ISession> session;
    slang::IModule* module = nullptr;
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

void Rndr::ShaderCompiler::LoadModule(const Opal::StringUtf8& source)
{
    SlangResult result = slang::createGlobalSession(m_impl->global_session.writeRef());
    if (SLANG_FAILED(result))
    {
        throw GraphicsAPIException(result, "Failed to create Slang global session!");
    }

    slang::TargetDesc target_desc = {};
    target_desc.format = SLANG_SPIRV;
    target_desc.profile = m_impl->global_session->findProfile("spirv_1_5");
    target_desc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

    slang::SessionDesc session_desc = {};
    session_desc.targets = &target_desc;
    session_desc.targetCount = 1;

    result = m_impl->global_session->createSession(session_desc, m_impl->session.writeRef());
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

    Slang::ComPtr<ISlangBlob> spirv_blob;
    result = linked_program->getEntryPointCode(0, 0, spirv_blob.writeRef(), diagnostics.writeRef());
    if (SLANG_FAILED(result))
    {
        Opal::StringUtf8 msg = "Failed to get SPIR-V!";
        if (diagnostics != nullptr)
        {
            msg = msg + "\n" + static_cast<const char*>(diagnostics->getBufferPointer());
        }
        throw GraphicsAPIException(result, msg.GetData());
    }

    CompileResult out;

    // Copy SPIR-V bytecode into owning array.
    const auto* spirv_data = static_cast<const u8*>(spirv_blob->getBufferPointer());
    const u64 spirv_size = spirv_blob->getBufferSize();
    out.spirv.Resize(spirv_size);
    memcpy(out.spirv.GetData(), spirv_data, spirv_size);

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