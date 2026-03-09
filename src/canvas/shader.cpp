#include "rndr/canvas/shader.hpp"

#include <cstdio>

#include "glad/glad.h"
#include "slang/slang.h"
#include "slang/slang-com-ptr.h"

#include "rndr/exception.hpp"
#include "rndr/trace.hpp"

namespace
{

Opal::StringUtf8 ReadTextFile(const Opal::StringUtf8& path)
{
    FILE* file = nullptr;
#if defined(_MSC_VER)
    fopen_s(&file, path.GetData(), "rb");
#else
    file = fopen(path.GetData(), "rb");
#endif
    if (file == nullptr)
    {
        return {};
    }

    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    Opal::StringUtf8 contents;
    contents.Resize(static_cast<Rndr::u64>(size));
    fread(contents.GetData(), 1, static_cast<size_t>(size), file);
    fclose(file);

    return contents;
}

Rndr::Canvas::ShaderType FromSlangStage(SlangStage stage)
{
    switch (stage)
    {
    case SLANG_STAGE_VERTEX:
        return Rndr::Canvas::ShaderType::Vertex;
    case SLANG_STAGE_FRAGMENT:
        return Rndr::Canvas::ShaderType::Fragment;
    case SLANG_STAGE_COMPUTE:
        return Rndr::Canvas::ShaderType::Compute;
    default:
        return Rndr::Canvas::ShaderType::EnumCount;
    }
}

GLenum ToGLShaderType(Rndr::Canvas::ShaderType type)
{
    switch (type)
    {
    case Rndr::Canvas::ShaderType::Vertex:
        return GL_VERTEX_SHADER;
    case Rndr::Canvas::ShaderType::Fragment:
        return GL_FRAGMENT_SHADER;
    case Rndr::Canvas::ShaderType::Compute:
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
    slang::TypeReflection::Kind kind = type_layout->getType()->getKind();
    switch (kind)
    {
    case slang::TypeReflection::Kind::SamplerState:
        return Rndr::Canvas::ParameterCategory::Sampler;
    case slang::TypeReflection::Kind::Resource:
    {
        SlangResourceAccess access = type_layout->getType()->getResourceAccess();
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
    slang::ParameterCategory category = param->getCategory();
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
    Rndr::Canvas::ParameterCategory category = FromSlangCategory(param);
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
    out_params.PushBack(std::move(sp));

    // For uniform buffers, also extract individual fields.
    if (category == Rndr::Canvas::ParameterCategory::Uniform)
    {
        slang::TypeLayoutReflection* type_layout = param->getTypeLayout();
        if (type_layout != nullptr)
        {
            slang::TypeReflection::Kind kind = type_layout->getType()->getKind();
            if (kind == slang::TypeReflection::Kind::ConstantBuffer || kind == slang::TypeReflection::Kind::ParameterBlock)
            {
                ExtractUniformFields(type_layout, binding_index, binding_space, out_params);
            }
        }
    }
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

struct SlangCompileResult
{
    Slang::ComPtr<ISlangBlob> spirv;
    Rndr::Canvas::ShaderType type = Rndr::Canvas::ShaderType::EnumCount;
    Opal::DynamicArray<Rndr::Canvas::ShaderParameter> parameters;
};

SlangCompileResult CompileSlangToSpirv(const Opal::StringUtf8& source, const Opal::StringUtf8& entry_point)
{
    Slang::ComPtr<slang::IGlobalSession> global_session;
    SlangResult result = slang::createGlobalSession(global_session.writeRef());
    if (SLANG_FAILED(result))
    {
        throw Rndr::GraphicsAPIException(result, "Failed to create Slang global session!");
    }

    slang::TargetDesc target_desc = {};
    target_desc.format = SLANG_SPIRV;
    target_desc.profile = global_session->findProfile("spirv_1_5");
    target_desc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

    slang::SessionDesc session_desc = {};
    session_desc.targets = &target_desc;
    session_desc.targetCount = 1;

    Slang::ComPtr<slang::ISession> session;
    result = global_session->createSession(session_desc, session.writeRef());
    if (SLANG_FAILED(result))
    {
        throw Rndr::GraphicsAPIException(result, "Failed to create Slang session!");
    }

    Slang::ComPtr<ISlangBlob> diagnostics;
    slang::IModule* module = session->loadModuleFromSourceString("canvas_shader", "canvas_shader.slang", source.GetData(),
                                                                 diagnostics.writeRef());
    if (module == nullptr)
    {
        Opal::StringUtf8 msg = "Failed to load Slang module!";
        if (diagnostics)
        {
            msg = msg + "\n" + static_cast<const char*>(diagnostics->getBufferPointer());
        }
        throw Rndr::GraphicsAPIException(0, msg.GetData());
    }

    Slang::ComPtr<slang::IEntryPoint> ep;
    result = module->findEntryPointByName(entry_point.GetData(), ep.writeRef());
    if (SLANG_FAILED(result))
    {
        Opal::StringUtf8 msg = Opal::StringUtf8("Failed to find entry point '") + entry_point + "'!";
        throw Rndr::GraphicsAPIException(result, msg.GetData());
    }

    slang::IComponentType* components[] = {module, ep};
    Slang::ComPtr<slang::IComponentType> linked_program;
    result = session->createCompositeComponentType(components, 2, linked_program.writeRef(), diagnostics.writeRef());
    if (SLANG_FAILED(result))
    {
        Opal::StringUtf8 msg = "Failed to link Slang program!";
        if (diagnostics)
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
        if (diagnostics)
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
            out.type = FromSlangStage(ep_reflection->getStage());
        }
        ExtractParameters(layout, out.parameters);
    }

    return out;
}

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

}  // namespace

Rndr::Canvas::Shader Rndr::Canvas::Shader::FromSource(const Opal::StringUtf8& path, const Opal::StringUtf8& entry_point)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSource");

    const Opal::StringUtf8 source = ReadTextFile(path);
    if (source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Failed to read shader file or file is empty!");
    }

    return FromSourceInMemory(source, entry_point);
}

Rndr::Canvas::Shader Rndr::Canvas::Shader::FromSourceInMemory(const Opal::StringUtf8& source, const Opal::StringUtf8& entry_point)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSourceInMemory");

    if (source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Shader source is empty!");
    }

    SlangCompileResult compile_result = CompileSlangToSpirv(source, entry_point);
    if (compile_result.type == ShaderType::EnumCount)
    {
        throw GraphicsAPIException(0, "Failed to deduce shader stage from entry point!");
    }

    Shader shader;
    shader.m_source = source.Clone();
    shader.m_entry_point = entry_point.Clone();
    shader.m_type = compile_result.type;
    shader.m_parameters = std::move(compile_result.parameters);
    shader.m_shader = CreateShaderFromSpirv(ToGLShaderType(compile_result.type), compile_result.spirv->getBufferPointer(),
                                            compile_result.spirv->getBufferSize());

    return shader;
}

Rndr::Canvas::Shader::~Shader()
{
    Destroy();
}

Rndr::Canvas::Shader::Shader(Shader&& other) noexcept
    : m_shader(other.m_shader),
      m_type(other.m_type),
      m_source(std::move(other.m_source)),
      m_entry_point(std::move(other.m_entry_point)),
      m_parameters(std::move(other.m_parameters))
{
    other.m_shader = 0;
    other.m_type = ShaderType::EnumCount;
}

Rndr::Canvas::Shader& Rndr::Canvas::Shader::operator=(Shader&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_shader = other.m_shader;
        m_type = other.m_type;
        m_source = std::move(other.m_source);
        m_entry_point = std::move(other.m_entry_point);
        m_parameters = std::move(other.m_parameters);
        other.m_shader = 0;
        other.m_type = ShaderType::EnumCount;
    }
    return *this;
}

Rndr::Canvas::Shader Rndr::Canvas::Shader::Clone() const
{
    if (!IsValid())
    {
        return {};
    }

    return FromSourceInMemory(m_source, m_entry_point);
}

void Rndr::Canvas::Shader::Destroy()
{
    if (m_shader != 0)
    {
        glDeleteShader(m_shader);
        m_shader = 0;
    }
    m_type = ShaderType::EnumCount;
    m_parameters.Clear();
}

bool Rndr::Canvas::Shader::IsValid() const
{
    return m_shader != 0;
}

Rndr::u32 Rndr::Canvas::Shader::GetNativeHandle() const
{
    return m_shader;
}

Rndr::Canvas::ShaderType Rndr::Canvas::Shader::GetType() const
{
    return m_type;
}

const Opal::DynamicArray<Rndr::Canvas::ShaderParameter>& Rndr::Canvas::Shader::GetParameters() const
{
    return m_parameters;
}

const Rndr::Canvas::ShaderParameter* Rndr::Canvas::Shader::FindParameter(const Opal::StringUtf8& name) const
{
    for (const ShaderParameter& param : m_parameters)
    {
        if (param.name == name)
        {
            return &param;
        }
    }
    return nullptr;
}
