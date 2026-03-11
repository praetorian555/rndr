#include "rndr/canvas/shader.hpp"

#include "glad/glad.h"
#include "slang-com-ptr.h"
#include "slang.h"

#include "rndr/exception.hpp"
#include "rndr/file.hpp"
#include "rndr/trace.hpp"

#include <cstring>

namespace
{

enum class ShaderStage : Rndr::u8
{
    Vertex,
    Fragment,
    Compute,
    Unknown,
};

ShaderStage FromSlangStage(SlangStage stage)
{
    switch (stage)
    {
        case SLANG_STAGE_VERTEX:
            return ShaderStage::Vertex;
        case SLANG_STAGE_FRAGMENT:
            return ShaderStage::Fragment;
        case SLANG_STAGE_COMPUTE:
            return ShaderStage::Compute;
        default:
            return ShaderStage::Unknown;
    }
}

GLenum ToGLShaderType(ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex:
            return GL_VERTEX_SHADER;
        case ShaderStage::Fragment:
            return GL_FRAGMENT_SHADER;
        case ShaderStage::Compute:
            return GL_COMPUTE_SHADER;
        default:
            return 0;
    }
}

Rndr::Canvas::ParameterCategory CategorizeFromType(slang::TypeLayoutReflection* type_layout)
{
    if (type_layout == nullptr)
    {
        return Rndr::Canvas::ParameterCategory::EnumCount;
    }
    const slang::TypeReflection::Kind kind = type_layout->getType()->getKind();
    switch (kind)
    {
        case slang::TypeReflection::Kind::SamplerState:
            return Rndr::Canvas::ParameterCategory::Sampler;
        case slang::TypeReflection::Kind::Resource:
        {
            const SlangResourceAccess access = type_layout->getType()->getResourceAccess();
            if (access == SLANG_RESOURCE_ACCESS_READ_WRITE)
            {
                return Rndr::Canvas::ParameterCategory::StorageBuffer;
            }
            return Rndr::Canvas::ParameterCategory::Texture;
        }
        case slang::TypeReflection::Kind::ConstantBuffer:
        case slang::TypeReflection::Kind::ParameterBlock:
            return Rndr::Canvas::ParameterCategory::Uniform;
        default:
            return Rndr::Canvas::ParameterCategory::EnumCount;
    }
}

Rndr::Canvas::ParameterCategory FromSlangCategory(slang::VariableLayoutReflection* param)
{
    const slang::ParameterCategory category = param->getCategory();
    switch (category)
    {
        case slang::ParameterCategory::Uniform:
        case slang::ParameterCategory::ConstantBuffer:
        case slang::ParameterCategory::PushConstantBuffer:
            return Rndr::Canvas::ParameterCategory::Uniform;
        case slang::ParameterCategory::ShaderResource:
            return Rndr::Canvas::ParameterCategory::Texture;
        case slang::ParameterCategory::SamplerState:
            return Rndr::Canvas::ParameterCategory::Sampler;
        case slang::ParameterCategory::UnorderedAccess:
            return Rndr::Canvas::ParameterCategory::StorageBuffer;
        case slang::ParameterCategory::VaryingInput:
            return Rndr::Canvas::ParameterCategory::VaryingInput;
        case slang::ParameterCategory::VaryingOutput:
            return Rndr::Canvas::ParameterCategory::VaryingOutput;
        case slang::ParameterCategory::DescriptorTableSlot:
        case slang::ParameterCategory::SubElementRegisterSpace:
            return CategorizeFromType(param->getTypeLayout());
        default:
            return Rndr::Canvas::ParameterCategory::EnumCount;
    }
}

void ExtractUniformFields(slang::TypeLayoutReflection* type_layout, Rndr::i32 binding_index, Rndr::i32 binding_space,
                          Opal::DynamicArray<Rndr::Canvas::ShaderParameter>& out_params)
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
        Rndr::Canvas::ShaderParameter sp;
        sp.name = field->getName();
        sp.binding_index = binding_index;
        sp.binding_space = binding_space;
        sp.offset = static_cast<Rndr::i32>(field->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM));
        sp.size = static_cast<Rndr::i32>(field->getTypeLayout()->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
        sp.category = Rndr::Canvas::ParameterCategory::Uniform;
        out_params.PushBack(std::move(sp));
    }
}

void ExtractParam(slang::VariableLayoutReflection* param, Opal::DynamicArray<Rndr::Canvas::ShaderParameter>& out_params)
{
    const Rndr::Canvas::ParameterCategory category = FromSlangCategory(param);
    if (category == Rndr::Canvas::ParameterCategory::EnumCount)
    {
        return;
    }
    const Rndr::i32 binding_index = static_cast<Rndr::i32>(param->getBindingIndex());
    const Rndr::i32 binding_space = static_cast<Rndr::i32>(param->getBindingSpace());

    Rndr::Canvas::ShaderParameter sp;
    sp.name = param->getName();
    sp.binding_index = binding_index;
    sp.binding_space = binding_space;
    sp.category = category;

    // For uniform parameters, extract size/offset and fields.
    if (category == Rndr::Canvas::ParameterCategory::Uniform)
    {
        slang::TypeLayoutReflection* type_layout = param->getTypeLayout();
        if (type_layout != nullptr)
        {
            const slang::TypeReflection::Kind kind = type_layout->getType()->getKind();
            if (kind == slang::TypeReflection::Kind::ConstantBuffer || kind == slang::TypeReflection::Kind::ParameterBlock)
            {
                // Top-level UBO declaration, size stays 0.
                out_params.PushBack(std::move(sp));
                ExtractUniformFields(type_layout, binding_index, binding_space, out_params);
                return;
            }

            // Standalone uniform — extract size and offset.
            sp.offset = static_cast<Rndr::i32>(param->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM));
            sp.size = static_cast<Rndr::i32>(type_layout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
        }
    }

    out_params.PushBack(std::move(sp));
}

void ExtractParameters(slang::ProgramLayout* layout, Opal::DynamicArray<Rndr::Canvas::ShaderParameter>& out_params)
{
    // Global parameters.
    for (unsigned i = 0; i < layout->getParameterCount(); ++i)
    {
        ExtractParam(layout->getParameterByIndex(i), out_params);
    }

    // Entry point parameters.
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
// Vertex layout extraction from reflection.
// ---------------------------------------------------------------------------

Rndr::Canvas::Attrib AttribFromName(const char* name)
{
    if (strcmp(name, "position") == 0 || strcmp(name, "pos") == 0)
    {
        return Rndr::Canvas::Attrib::Position;
    }
    if (strcmp(name, "normal") == 0 || strcmp(name, "norm") == 0)
    {
        return Rndr::Canvas::Attrib::Normal;
    }
    if (strcmp(name, "uv") == 0 || strcmp(name, "texcoord") == 0 || strcmp(name, "texCoord") == 0)
    {
        return Rndr::Canvas::Attrib::UV;
    }
    if (strcmp(name, "color") == 0 || strcmp(name, "col") == 0)
    {
        return Rndr::Canvas::Attrib::Color;
    }
    if (strcmp(name, "tangent") == 0 || strcmp(name, "tan") == 0)
    {
        return Rndr::Canvas::Attrib::Tangent;
    }
    return Rndr::Canvas::Attrib::EnumCount;
}

Rndr::Canvas::Format FormatFromSlangType(slang::TypeLayoutReflection* type_layout)
{
    slang::TypeReflection* type = type_layout->getType();
    const slang::TypeReflection::Kind kind = type->getKind();

    slang::TypeReflection::ScalarType scalar = slang::TypeReflection::None;
    unsigned element_count = 0;

    if (kind == slang::TypeReflection::Kind::Vector)
    {
        element_count = static_cast<unsigned>(type->getElementCount());
        scalar = type->getElementType()->getScalarType();
    }
    else if (kind == slang::TypeReflection::Kind::Scalar)
    {
        element_count = 1;
        scalar = type->getScalarType();
    }
    else
    {
        return Rndr::Canvas::Format::EnumCount;
    }

    if (scalar == slang::TypeReflection::Float32)
    {
        switch (element_count)
        {
            case 1:
                return Rndr::Canvas::Format::Float1;
            case 2:
                return Rndr::Canvas::Format::Float2;
            case 3:
                return Rndr::Canvas::Format::Float3;
            case 4:
                return Rndr::Canvas::Format::Float4;
            default:
                return Rndr::Canvas::Format::EnumCount;
        }
    }
    if (scalar == slang::TypeReflection::Int32)
    {
        switch (element_count)
        {
            case 1:
                return Rndr::Canvas::Format::Int1;
            case 2:
                return Rndr::Canvas::Format::Int2;
            case 3:
                return Rndr::Canvas::Format::Int3;
            case 4:
                return Rndr::Canvas::Format::Int4;
            default:
                return Rndr::Canvas::Format::EnumCount;
        }
    }

    return Rndr::Canvas::Format::EnumCount;
}

Rndr::Canvas::VertexLayout ExtractVertexLayout(slang::ProgramLayout* layout)
{
    Rndr::Canvas::VertexLayout vertex_layout;

    if (layout == nullptr || layout->getEntryPointCount() == 0)
    {
        return vertex_layout;
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
            // Struct vertex input — iterate fields.
            const unsigned field_count = type_layout->getFieldCount();
            for (unsigned j = 0; j < field_count; ++j)
            {
                slang::VariableLayoutReflection* field = type_layout->getFieldByIndex(j);
                const char* name = field->getName();

                Rndr::Canvas::Attrib attrib = AttribFromName(name);
                if (attrib == Rndr::Canvas::Attrib::EnumCount)
                {
                    Opal::StringUtf8 msg =
                        Opal::StringUtf8("Cannot map vertex attribute '") + name + "' to a known Attrib semantic!";
                    throw Rndr::GraphicsAPIException(0, msg.GetData());
                }

                Rndr::Canvas::Format format = FormatFromSlangType(field->getTypeLayout());
                if (format == Rndr::Canvas::Format::EnumCount)
                {
                    Opal::StringUtf8 msg =
                        Opal::StringUtf8("Unsupported vertex attribute format for '") + name + "'!";
                    throw Rndr::GraphicsAPIException(0, msg.GetData());
                }

                vertex_layout.Add(attrib, format);
            }
        }
        else
        {
            // Non-struct vertex input — single attribute.
            const char* name = param->getName();

            Rndr::Canvas::Attrib attrib = AttribFromName(name);
            if (attrib == Rndr::Canvas::Attrib::EnumCount)
            {
                Opal::StringUtf8 msg =
                    Opal::StringUtf8("Cannot map vertex attribute '") + name + "' to a known Attrib semantic!";
                throw Rndr::GraphicsAPIException(0, msg.GetData());
            }

            Rndr::Canvas::Format format = FormatFromSlangType(type_layout);
            if (format == Rndr::Canvas::Format::EnumCount)
            {
                Opal::StringUtf8 msg = Opal::StringUtf8("Unsupported vertex attribute format for '") + name + "'!";
                throw Rndr::GraphicsAPIException(0, msg.GetData());
            }

            vertex_layout.Add(attrib, format);
        }
    }

    return vertex_layout;
}

// ---------------------------------------------------------------------------
// Slang module loading and entry point compilation.
// ---------------------------------------------------------------------------

struct LoadedModule
{
    Slang::ComPtr<slang::IGlobalSession> global_session;
    Slang::ComPtr<slang::ISession> session;
    slang::IModule* module = nullptr;
};

LoadedModule LoadModule(const Opal::StringUtf8& source)
{
    LoadedModule mod;

    SlangResult result = slang::createGlobalSession(mod.global_session.writeRef());
    if (SLANG_FAILED(result))
    {
        throw Rndr::GraphicsAPIException(result, "Failed to create Slang global session!");
    }

    slang::TargetDesc target_desc = {};
    target_desc.format = SLANG_SPIRV;
    target_desc.profile = mod.global_session->findProfile("spirv_1_5");
    target_desc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

    slang::SessionDesc session_desc = {};
    session_desc.targets = &target_desc;
    session_desc.targetCount = 1;

    result = mod.global_session->createSession(session_desc, mod.session.writeRef());
    if (SLANG_FAILED(result))
    {
        throw Rndr::GraphicsAPIException(result, "Failed to create Slang session!");
    }

    Slang::ComPtr<ISlangBlob> diagnostics;
    mod.module = mod.session->loadModuleFromSourceString("canvas_shader", "canvas_shader.slang", *source, diagnostics.writeRef());
    if (mod.module == nullptr)
    {
        Opal::StringUtf8 msg = "Failed to load Slang module!";
        if (diagnostics != nullptr)
        {
            msg = msg + "\n" + static_cast<const char*>(diagnostics->getBufferPointer());
        }
        throw Rndr::GraphicsAPIException(0, *msg);
    }

    return mod;
}

struct SlangCompileResult
{
    Slang::ComPtr<ISlangBlob> spirv;
    ShaderStage stage = ShaderStage::Unknown;
    Opal::DynamicArray<Rndr::Canvas::ShaderParameter> parameters;
    Rndr::Canvas::VertexLayout vertex_layout;  // Only populated for vertex stage.
    Rndr::Canvas::NumThreads num_threads;       // Only populated for compute stage.
};

SlangCompileResult CompileEntryPoint(const LoadedModule& mod, const Opal::StringUtf8& entry_point)
{
    Slang::ComPtr<slang::IEntryPoint> ep;
    SlangResult result = mod.module->findEntryPointByName(*entry_point, ep.writeRef());
    if (SLANG_FAILED(result))
    {
        Opal::StringUtf8 msg = Opal::StringUtf8("Failed to find entry point '") + entry_point + "'!";
        throw Rndr::GraphicsAPIException(result, msg.GetData());
    }

    slang::IComponentType* components[] = {mod.module, ep};
    Slang::ComPtr<slang::IComponentType> linked_program;
    Slang::ComPtr<ISlangBlob> diagnostics;
    result = mod.session->createCompositeComponentType(components, 2, linked_program.writeRef(), diagnostics.writeRef());
    if (SLANG_FAILED(result))
    {
        Opal::StringUtf8 msg = "Failed to link Slang program!";
        if (diagnostics != nullptr)
        {
            msg = msg + "\n" + static_cast<const char*>(diagnostics->getBufferPointer());
        }
        throw Rndr::GraphicsAPIException(result, msg.GetData());
    }

    SlangCompileResult out;

    result = linked_program->getEntryPointCode(0, 0, out.spirv.writeRef(), diagnostics.writeRef());
    if (SLANG_FAILED(result))
    {
        Opal::StringUtf8 msg = "Failed to get SPIR-V!";
        if (diagnostics != nullptr)
        {
            msg = msg + "\n" + static_cast<const char*>(diagnostics->getBufferPointer());
        }
        throw Rndr::GraphicsAPIException(result, msg.GetData());
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
            out.vertex_layout = ExtractVertexLayout(layout);
        }
        if (out.stage == ShaderStage::Compute)
        {
            slang::EntryPointReflection* ep_ref = layout->getEntryPointByIndex(0);
            SlangUInt thread_group_size[3];
            ep_ref->getComputeThreadGroupSize(3, thread_group_size);
            out.num_threads.x = static_cast<Rndr::u32>(thread_group_size[0]);
            out.num_threads.y = static_cast<Rndr::u32>(thread_group_size[1]);
            out.num_threads.z = static_cast<Rndr::u32>(thread_group_size[2]);
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// Entry point discovery.
// ---------------------------------------------------------------------------

struct EntryPointInfo
{
    Opal::StringUtf8 name;
    ShaderStage stage;
};

Opal::DynamicArray<EntryPointInfo> DiscoverEntryPoints(const LoadedModule& mod)
{
    Opal::DynamicArray<EntryPointInfo> entries;

    const SlangInt32 count = mod.module->getDefinedEntryPointCount();
    for (SlangInt32 i = 0; i < count; ++i)
    {
        Slang::ComPtr<slang::IEntryPoint> ep;
        mod.module->getDefinedEntryPoint(i, ep.writeRef());
        if (ep == nullptr)
        {
            continue;
        }

        // Create a composite to get reflection data for this entry point.
        slang::IComponentType* components[] = {mod.module, ep.get()};
        Slang::ComPtr<slang::IComponentType> linked;
        const SlangResult result = mod.session->createCompositeComponentType(components, 2, linked.writeRef(), nullptr);
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

Opal::StringUtf8 FindSingleEntryPoint(const Opal::DynamicArray<EntryPointInfo>& entries, ShaderStage target_stage,
                                       const char* stage_name)
{
    Opal::StringUtf8 found_name;
    int count = 0;
    for (Rndr::u64 i = 0; i < entries.GetSize(); ++i)
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
        throw Rndr::GraphicsAPIException(0, msg.GetData());
    }
    if (count > 1)
    {
        Opal::StringUtf8 msg = Opal::StringUtf8("Multiple ") + stage_name + " entry points found, expected exactly 1!";
        throw Rndr::GraphicsAPIException(0, msg.GetData());
    }

    return found_name;
}

// ---------------------------------------------------------------------------
// OpenGL shader and program creation.
// ---------------------------------------------------------------------------

GLuint CreateShaderFromSpirv(GLenum stage, const void* spirv_data, size_t spirv_size)
{
    const GLuint shader = glCreateShader(stage);
    if (shader == 0)
    {
        throw Rndr::GraphicsAPIException(0, "Failed to create GL shader!");
    }

    glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv_data, static_cast<GLsizei>(spirv_size));

    const GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        glDeleteShader(shader);
        throw Rndr::GraphicsAPIException(err, "Failed to upload SPIR-V binary!");
    }

    glSpecializeShader(shader, "main", 0, nullptr, nullptr);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE)
    {
        GLint log_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        Opal::StringUtf8 log;
        if (log_len > 0)
        {
            log.Resize(static_cast<Rndr::u64>(log_len));
            glGetShaderInfoLog(shader, log_len, &log_len, log.GetData());
        }
        glDeleteShader(shader);

        Opal::StringUtf8 msg = Opal::StringUtf8("Failed to specialize shader:\n") + log;
        throw Rndr::GraphicsAPIException(0, msg.GetData());
    }

    return shader;
}

GLuint LinkProgram(GLuint vertex_shader, GLuint fragment_shader)
{
    const GLuint program = glCreateProgram();
    if (program == 0)
    {
        throw Rndr::GraphicsAPIException(0, "Failed to create GL program!");
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE)
    {
        GLint log_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        Opal::StringUtf8 log;
        if (log_len > 0)
        {
            log.Resize(static_cast<Rndr::u64>(log_len));
            glGetProgramInfoLog(program, log_len, &log_len, log.GetData());
        }
        glDeleteProgram(program);

        Opal::StringUtf8 msg = Opal::StringUtf8("Failed to link shader program:\n") + log;
        throw Rndr::GraphicsAPIException(0, msg.GetData());
    }

    return program;
}

GLuint LinkProgram(GLuint shader)
{
    const GLuint program = glCreateProgram();
    if (program == 0)
    {
        throw Rndr::GraphicsAPIException(0, "Failed to create GL program!");
    }

    glAttachShader(program, shader);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE)
    {
        GLint log_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        Opal::StringUtf8 log;
        if (log_len > 0)
        {
            log.Resize(static_cast<Rndr::u64>(log_len));
            glGetProgramInfoLog(program, log_len, &log_len, log.GetData());
        }
        glDeleteProgram(program);

        Opal::StringUtf8 msg = Opal::StringUtf8("Failed to link shader program:\n") + log;
        throw Rndr::GraphicsAPIException(0, msg.GetData());
    }

    return program;
}

// ---------------------------------------------------------------------------
// Reflection merging.
// ---------------------------------------------------------------------------

bool IsTopLevelResource(const Rndr::Canvas::ShaderParameter& p)
{
    switch (p.category)
    {
        case Rndr::Canvas::ParameterCategory::Texture:
        case Rndr::Canvas::ParameterCategory::Sampler:
        case Rndr::Canvas::ParameterCategory::StorageBuffer:
            return true;
        case Rndr::Canvas::ParameterCategory::Uniform:
            // Uniform fields inside constant buffers have size > 0.
            // Top-level uniform buffer declarations have size == 0.
            return p.size == 0;
        default:
            return false;
    }
}

Rndr::Canvas::ShaderParameter CloneParameter(const Rndr::Canvas::ShaderParameter& p)
{
    Rndr::Canvas::ShaderParameter copy;
    copy.name = p.name.Clone();
    copy.binding_index = p.binding_index;
    copy.binding_space = p.binding_space;
    copy.offset = p.offset;
    copy.size = p.size;
    copy.category = p.category;
    return copy;
}

Opal::DynamicArray<Rndr::Canvas::ShaderParameter> MergeParameters(const Opal::DynamicArray<Rndr::Canvas::ShaderParameter>& vertex_params,
                                                                  const Opal::DynamicArray<Rndr::Canvas::ShaderParameter>& fragment_params)
{
    using Rndr::Canvas::ParameterCategory;
    using Rndr::Canvas::ShaderParameter;

    Opal::DynamicArray<ShaderParameter> merged;

    // Include vertex parameters, skip VaryingOutput (inter-stage).
    for (Rndr::u64 i = 0; i < vertex_params.GetSize(); ++i)
    {
        const ShaderParameter& p = vertex_params[i];
        if (p.category != ParameterCategory::VaryingOutput)
        {
            merged.PushBack(CloneParameter(p));
        }
    }

    // Include fragment parameters, skip VaryingInput (inter-stage), check for conflicts.
    for (Rndr::u64 i = 0; i < fragment_params.GetSize(); ++i)
    {
        const ShaderParameter& fp = fragment_params[i];
        if (fp.category == ParameterCategory::VaryingInput)
        {
            continue;
        }

        // Check name-based conflict/duplicate.
        bool duplicate = false;
        for (Rndr::u64 j = 0; j < merged.GetSize(); ++j)
        {
            const ShaderParameter& ep = merged[j];
            if (ep.name == fp.name)
            {
                if (ep.category != fp.category)
                {
                    Opal::StringUtf8 msg =
                        Opal::StringUtf8("Parameter '") + fp.name + "' has conflicting types in vertex and fragment stages!";
                    throw Rndr::GraphicsAPIException(0, msg.GetData());
                }
                duplicate = true;
                break;
            }
        }

        if (!duplicate)
        {
            // Check binding slot conflict for top-level resources.
            if (IsTopLevelResource(fp))
            {
                for (Rndr::u64 j = 0; j < merged.GetSize(); ++j)
                {
                    const ShaderParameter& ep = merged[j];
                    if (IsTopLevelResource(ep) && ep.binding_index == fp.binding_index && ep.binding_space == fp.binding_space &&
                        ep.category != fp.category)
                    {
                        throw Rndr::GraphicsAPIException(
                            0, "Binding slot conflict between vertex and fragment stages! Same binding slot used for different types.");
                    }
                }
            }
            merged.PushBack(CloneParameter(fp));
        }
    }

    return merged;
}

// ---------------------------------------------------------------------------
// Core build: compile both stages, merge reflection, link GL program.
// ---------------------------------------------------------------------------

struct ShaderBuildResult
{
    GLuint program = 0;
    Opal::StringUtf8 vertex_entry;
    Opal::StringUtf8 fragment_entry;
    Opal::DynamicArray<Rndr::Canvas::ShaderParameter> parameters;
    Rndr::Canvas::VertexLayout vertex_layout;
    Rndr::Canvas::NumThreads num_threads;
};

ShaderBuildResult BuildFromSingleSource(const Opal::StringUtf8& source)
{
    LoadedModule mod = LoadModule(source);
    Opal::DynamicArray<EntryPointInfo> entries = DiscoverEntryPoints(mod);

    // Count entry points by stage.
    int vertex_count = 0;
    int fragment_count = 0;
    int compute_count = 0;
    for (Rndr::u64 i = 0; i < entries.GetSize(); ++i)
    {
        switch (entries[i].stage)
        {
            case ShaderStage::Vertex:
                ++vertex_count;
                break;
            case ShaderStage::Fragment:
                ++fragment_count;
                break;
            case ShaderStage::Compute:
                ++compute_count;
                break;
            default:
                break;
        }
    }

    if (compute_count > 0 && (vertex_count > 0 || fragment_count > 0))
    {
        throw Rndr::GraphicsAPIException(0, "Shader source contains both compute and graphics entry points!");
    }

    // Compute path.
    if (compute_count > 0)
    {
        Opal::StringUtf8 cs_entry = FindSingleEntryPoint(entries, ShaderStage::Compute, "compute");
        SlangCompileResult cs_result = CompileEntryPoint(mod, cs_entry);
        if (cs_result.stage != ShaderStage::Compute)
        {
            throw Rndr::GraphicsAPIException(0, "Compute entry point does not have [shader(\"compute\")] annotation!");
        }

        const GLuint cs =
            CreateShaderFromSpirv(GL_COMPUTE_SHADER, cs_result.spirv->getBufferPointer(), cs_result.spirv->getBufferSize());
        GLuint program = 0;
        try
        {
            program = LinkProgram(cs);
        }
        catch (...)
        {
            glDeleteShader(cs);
            throw;
        }
        glDeleteShader(cs);

        ShaderBuildResult out;
        out.program = program;
        out.parameters = std::move(cs_result.parameters);
        out.num_threads = cs_result.num_threads;
        return out;
    }

    // Graphics path.
    Opal::StringUtf8 vs_entry = FindSingleEntryPoint(entries, ShaderStage::Vertex, "vertex");
    Opal::StringUtf8 fs_entry = FindSingleEntryPoint(entries, ShaderStage::Fragment, "fragment");

    SlangCompileResult vs_result = CompileEntryPoint(mod, vs_entry);
    if (vs_result.stage != ShaderStage::Vertex)
    {
        throw Rndr::GraphicsAPIException(0, "Vertex entry point does not have [shader(\"vertex\")] annotation!");
    }

    const SlangCompileResult fs_result = CompileEntryPoint(mod, fs_entry);
    if (fs_result.stage != ShaderStage::Fragment)
    {
        throw Rndr::GraphicsAPIException(0, "Fragment entry point does not have [shader(\"fragment\")] annotation!");
    }

    Opal::DynamicArray<Rndr::Canvas::ShaderParameter> merged = MergeParameters(vs_result.parameters, fs_result.parameters);

    const GLuint vs = CreateShaderFromSpirv(GL_VERTEX_SHADER, vs_result.spirv->getBufferPointer(), vs_result.spirv->getBufferSize());
    GLuint fs = 0;
    try
    {
        fs = CreateShaderFromSpirv(GL_FRAGMENT_SHADER, fs_result.spirv->getBufferPointer(), fs_result.spirv->getBufferSize());
    }
    catch (...)
    {
        glDeleteShader(vs);
        throw;
    }

    GLuint program = 0;
    try
    {
        program = LinkProgram(vs, fs);
    }
    catch (...)
    {
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    ShaderBuildResult out;
    out.program = program;
    out.vertex_entry = std::move(vs_entry);
    out.fragment_entry = std::move(fs_entry);
    out.parameters = std::move(merged);
    out.vertex_layout = std::move(vs_result.vertex_layout);
    return out;
}

ShaderBuildResult BuildFromTwoSources(const Opal::StringUtf8& vertex_source, const Opal::StringUtf8& fragment_source)
{
    LoadedModule vs_mod = LoadModule(vertex_source);
    Opal::DynamicArray<EntryPointInfo> vs_entries = DiscoverEntryPoints(vs_mod);
    Opal::StringUtf8 vs_entry = FindSingleEntryPoint(vs_entries, ShaderStage::Vertex, "vertex");

    LoadedModule fs_mod = LoadModule(fragment_source);
    Opal::DynamicArray<EntryPointInfo> fs_entries = DiscoverEntryPoints(fs_mod);
    Opal::StringUtf8 fs_entry = FindSingleEntryPoint(fs_entries, ShaderStage::Fragment, "fragment");

    SlangCompileResult vs_result = CompileEntryPoint(vs_mod, vs_entry);
    if (vs_result.stage != ShaderStage::Vertex)
    {
        throw Rndr::GraphicsAPIException(0, "Vertex entry point does not have [shader(\"vertex\")] annotation!");
    }

    const SlangCompileResult fs_result = CompileEntryPoint(fs_mod, fs_entry);
    if (fs_result.stage != ShaderStage::Fragment)
    {
        throw Rndr::GraphicsAPIException(0, "Fragment entry point does not have [shader(\"fragment\")] annotation!");
    }

    Opal::DynamicArray<Rndr::Canvas::ShaderParameter> merged = MergeParameters(vs_result.parameters, fs_result.parameters);

    const GLuint vs = CreateShaderFromSpirv(GL_VERTEX_SHADER, vs_result.spirv->getBufferPointer(), vs_result.spirv->getBufferSize());
    GLuint fs = 0;
    try
    {
        fs = CreateShaderFromSpirv(GL_FRAGMENT_SHADER, fs_result.spirv->getBufferPointer(), fs_result.spirv->getBufferSize());
    }
    catch (...)
    {
        glDeleteShader(vs);
        throw;
    }

    GLuint program = 0;
    try
    {
        program = LinkProgram(vs, fs);
    }
    catch (...)
    {
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    ShaderBuildResult out;
    out.program = program;
    out.vertex_entry = std::move(vs_entry);
    out.fragment_entry = std::move(fs_entry);
    out.parameters = std::move(merged);
    out.vertex_layout = std::move(vs_result.vertex_layout);
    return out;
}

}  // namespace

Rndr::Canvas::Shader Rndr::Canvas::Shader::FromSource(const Opal::StringUtf8& path)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSource");

    const Opal::StringUtf8 source = File::ReadEntireTextFile(path);
    if (source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Failed to read shader file or file is empty!");
    }

    return FromSourceInMemory(source);
}

Rndr::Canvas::Shader Rndr::Canvas::Shader::FromSourceInMemory(const Opal::StringUtf8& source)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSourceInMemory");

    if (source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Shader source is empty!");
    }

    ShaderBuildResult build = BuildFromSingleSource(source);

    Shader shader;
    shader.m_program = build.program;
    shader.m_vertex_source = source.Clone();
    shader.m_vertex_entry = std::move(build.vertex_entry);
    shader.m_fragment_entry = std::move(build.fragment_entry);
    shader.m_parameters = std::move(build.parameters);
    shader.m_vertex_layout = std::move(build.vertex_layout);
    shader.m_num_threads = build.num_threads;

    return shader;
}

Rndr::Canvas::Shader Rndr::Canvas::Shader::FromSources(const Opal::StringUtf8& vertex_path, const Opal::StringUtf8& fragment_path)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSources");

    const Opal::StringUtf8 vs_source = File::ReadEntireTextFile(vertex_path);
    if (vs_source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Failed to read vertex shader file or file is empty!");
    }

    const Opal::StringUtf8 fs_source = File::ReadEntireTextFile(fragment_path);
    if (fs_source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Failed to read fragment shader file or file is empty!");
    }

    return FromSourcesInMemory(vs_source, fs_source);
}

Rndr::Canvas::Shader Rndr::Canvas::Shader::FromSourcesInMemory(const Opal::StringUtf8& vertex_source,
                                                                const Opal::StringUtf8& fragment_source)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSourcesInMemory");

    if (vertex_source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Vertex shader source is empty!");
    }
    if (fragment_source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Fragment shader source is empty!");
    }

    ShaderBuildResult build = BuildFromTwoSources(vertex_source, fragment_source);

    Shader shader;
    shader.m_program = build.program;
    shader.m_vertex_source = vertex_source.Clone();
    shader.m_vertex_entry = std::move(build.vertex_entry);
    shader.m_fragment_source = fragment_source.Clone();
    shader.m_fragment_entry = std::move(build.fragment_entry);
    shader.m_parameters = std::move(build.parameters);
    shader.m_vertex_layout = std::move(build.vertex_layout);
    shader.m_num_threads = build.num_threads;

    return shader;
}

Rndr::Canvas::Shader::~Shader()
{
    Destroy();
}

Rndr::Canvas::Shader::Shader(Shader&& other) noexcept
    : m_program(other.m_program),
      m_vertex_source(std::move(other.m_vertex_source)),
      m_vertex_entry(std::move(other.m_vertex_entry)),
      m_fragment_source(std::move(other.m_fragment_source)),
      m_fragment_entry(std::move(other.m_fragment_entry)),
      m_parameters(std::move(other.m_parameters)),
      m_vertex_layout(std::move(other.m_vertex_layout)),
      m_num_threads(other.m_num_threads)
{
    other.m_program = 0;
    other.m_num_threads = {};
}

Rndr::Canvas::Shader& Rndr::Canvas::Shader::operator=(Shader&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_program = other.m_program;
        m_vertex_source = std::move(other.m_vertex_source);
        m_vertex_entry = std::move(other.m_vertex_entry);
        m_fragment_source = std::move(other.m_fragment_source);
        m_fragment_entry = std::move(other.m_fragment_entry);
        m_parameters = std::move(other.m_parameters);
        m_vertex_layout = std::move(other.m_vertex_layout);
        m_num_threads = other.m_num_threads;
        other.m_program = 0;
        other.m_num_threads = {};
    }
    return *this;
}

Rndr::Canvas::Shader Rndr::Canvas::Shader::Clone() const
{
    if (!IsValid())
    {
        return {};
    }

    if (m_fragment_source.IsEmpty())
    {
        return FromSourceInMemory(m_vertex_source);
    }
    return FromSourcesInMemory(m_vertex_source, m_fragment_source);
}

void Rndr::Canvas::Shader::Destroy()
{
    if (m_program != 0)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_parameters.Clear();
    m_vertex_layout = VertexLayout();
    m_num_threads = {};
}

bool Rndr::Canvas::Shader::IsValid() const
{
    return m_program != 0;
}

Rndr::u32 Rndr::Canvas::Shader::GetNativeHandle() const
{
    return m_program;
}

const Opal::DynamicArray<Rndr::Canvas::ShaderParameter>& Rndr::Canvas::Shader::GetParameters() const
{
    return m_parameters;
}

const Rndr::Canvas::ShaderParameter* Rndr::Canvas::Shader::FindParameter(const Opal::StringUtf8& name) const
{
    for (u64 i = 0; i < m_parameters.GetSize(); ++i)
    {
        if (m_parameters[i].name == name)
        {
            return &m_parameters[i];
        }
    }
    return nullptr;
}

const Rndr::Canvas::VertexLayout& Rndr::Canvas::Shader::GetVertexLayout() const
{
    return m_vertex_layout;
}

const Rndr::Canvas::NumThreads& Rndr::Canvas::Shader::GetNumThreads() const
{
    return m_num_threads;
}
