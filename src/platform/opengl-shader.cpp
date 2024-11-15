#include "rndr/platform/opengl-shader.h"

#include "glad/glad.h"

#include "opal/container/in-place-array.h"

#include "opengl-helpers.h"
#include "rndr/log.h"
#include "rndr/platform/opengl-graphics-context.h"
#include "rndr/trace.h"

namespace
{
void GenerateDefinesShaderCode(const Opal::DynamicArray<Opal::StringUtf8>& defines, Opal::StringUtf8& out_shader_code)
{
    for (const Opal::StringUtf8& define : defines)
    {
        out_shader_code += "#define " + define + "\n";
    }
}
}  // namespace

Rndr::Shader::Shader(const GraphicsContext& graphics_context, const ShaderDesc& desc) : m_desc(desc)
{
    const ErrorCode err = Initialize(graphics_context, desc);
    if (err != ErrorCode::Success)
    {
        Destroy();
        return;
    }
}

Rndr::Shader::Shader(Rndr::Shader&& other) noexcept : m_desc(std::move(other.m_desc)), m_native_shader(other.m_native_shader)
{
    other.m_native_shader = k_invalid_opengl_object;
}

Rndr::ErrorCode Rndr::Shader::Initialize(const Rndr::GraphicsContext& graphics_context, const Rndr::ShaderDesc& desc)
{
    RNDR_UNUSED(graphics_context);

    if (desc.type >= Rndr::ShaderType::EnumCount)
    {
        RNDR_LOG_ERROR("Invalid shader type!");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (desc.source.IsEmpty())
    {
        RNDR_LOG_ERROR("Shader source is empty!");
        return Rndr::ErrorCode::InvalidArgument;
    }

    m_desc = desc;

    const GLenum shader_type = Rndr::FromShaderTypeToOpenGL(desc.type);
    m_native_shader = glCreateShader(shader_type);
    if (m_native_shader == k_invalid_opengl_object)
    {
        RNDR_LOG_ERROR("Failed to create shader!");
        return ErrorCode::OutOfMemory;
    }

    Opal::StringUtf8 final_shader_code = desc.source;
    // If there are defines, insert them at the beginning of the shader source, after the version.
    if (!m_desc.defines.IsEmpty())
    {
        Opal::StringUtf8 defines_shader_code;
        GenerateDefinesShaderCode(desc.defines, defines_shader_code);
        u64 find_result = Opal::Find(desc.source, "#version");
        if (find_result == Opal::StringUtf8::k_npos)
        {
            find_result = 0;
        }
        else
        {
            find_result = Opal::Find(desc.source, '\n', find_result);
            RNDR_ASSERT(find_result != Opal::StringUtf8::k_npos);
            find_result += 1;
        }
        final_shader_code.Insert(find_result, defines_shader_code);
    }

    const char8* final_shader_code_c_str = reinterpret_cast<const char8*>(final_shader_code.GetData());
    glShaderSource(m_native_shader, 1, &final_shader_code_c_str, nullptr);
    RNDR_GL_VERIFY("Failed to set shader source!", Destroy());
    glCompileShader(m_native_shader);
    RNDR_GL_VERIFY("Failed to compile shader source!", Destroy());
    GLint is_compiled = 0;
    glGetShaderiv(m_native_shader, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == GL_FALSE)
    {
        constexpr size_t k_error_log_size = 1024;
        Opal::InPlaceArray<GLchar, k_error_log_size> error_log;
        GLint log_length = 0;
        glGetShaderiv(m_native_shader, GL_INFO_LOG_LENGTH, &log_length);
        glGetShaderInfoLog(m_native_shader, log_length, &log_length, error_log.GetData());
        RNDR_LOG_ERROR("Failed to compile shader:\n%s", error_log);
        Destroy();
        return ErrorCode::ShaderCompilationError;
    }
    return ErrorCode::Success;
}

Rndr::Shader::~Shader()
{
    Destroy();
}

void Rndr::Shader::Destroy()
{
    if (m_native_shader != k_invalid_opengl_object)
    {
        glDeleteShader(m_native_shader);
        RNDR_ASSERT_OPENGL();
        m_native_shader = k_invalid_opengl_object;
    }
}

Rndr::Shader& Rndr::Shader::operator=(Rndr::Shader&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_native_shader = other.m_native_shader;
        other.m_native_shader = k_invalid_opengl_object;
    }
    return *this;
}

bool Rndr::Shader::IsValid() const
{
    return m_native_shader != k_invalid_opengl_object;
}

const Rndr::ShaderDesc& Rndr::Shader::GetDesc() const
{
    return m_desc;
}
const GLuint Rndr::Shader::GetNativeShader() const
{
    return m_native_shader;
}
