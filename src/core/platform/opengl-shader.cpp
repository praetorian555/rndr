#include "rndr/core/platform/opengl-shader.h"

#include <glad/glad.h>

#include "core/platform/opengl-helpers.h"
#include "rndr/core/platform/opengl-graphics-context.h"
#include "rndr/core/trace.h"

namespace
{
void GenerateDefinesShaderCode(const Opal::Array<Opal::StringUtf8>& defines, Opal::StringUtf8& out_shader_code)
{
    for (const Opal::StringUtf8& define : defines)
    {
        out_shader_code += u8"#define " + define + u8"\n";
    }
}
}  // namespace

Rndr::Shader::Shader(const GraphicsContext& graphics_context, const ShaderDesc& desc) : m_desc(desc)
{
    RNDR_CPU_EVENT_SCOPED("Create Shader");

    RNDR_UNUSED(graphics_context);
    const GLenum shader_type = Rndr::FromShaderTypeToOpenGL(desc.type);
    m_native_shader = glCreateShader(shader_type);
    RNDR_ASSERT_OPENGL();
    if (m_native_shader == k_invalid_opengl_object)
    {
        return;
    }
    Opal::StringUtf8 defines_shader_code;
    GenerateDefinesShaderCode(desc.defines, defines_shader_code);
    Opal::StringUtf8 final_shader_code = desc.source;
    if (!defines_shader_code.IsEmpty())
    {
        u64 find_result = Opal::Find(desc.source, u8"#version");
        if (find_result == Opal::StringUtf8::k_npos)
        {
            find_result = 0;
        }
        else
        {
            find_result = Opal::Find(desc.source, '\n', find_result);
        }
        final_shader_code.Insert(find_result, defines_shader_code);
    }
    const c* final_shader_code_c_str = reinterpret_cast<const c*>(final_shader_code.GetData());
    glShaderSource(m_native_shader, 1, &final_shader_code_c_str, nullptr);
    RNDR_ASSERT_OPENGL();
    glCompileShader(m_native_shader);
    RNDR_ASSERT_OPENGL();
}

Rndr::Shader::~Shader()
{
    Destroy();
}

Rndr::Shader::Shader(Rndr::Shader&& other) noexcept : m_desc(std::move(other.m_desc)), m_native_shader(other.m_native_shader)
{
    other.m_native_shader = k_invalid_opengl_object;
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

void Rndr::Shader::Destroy()
{
    if (m_native_shader != k_invalid_opengl_object)
    {
        glDeleteShader(m_native_shader);
        RNDR_ASSERT_OPENGL();
        m_native_shader = k_invalid_opengl_object;
    }
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
