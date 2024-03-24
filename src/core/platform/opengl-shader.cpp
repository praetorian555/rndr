#include "rndr/core/platform/opengl-shader.h"

#include <glad/glad.h>

#include "core/platform/opengl-helpers.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/platform/opengl-graphics-context.h"
#include "rndr/utility/cpu-tracer.h"

namespace
{
void GenerateDefinesShaderCode(const Rndr::Array<Rndr::String>& defines, Rndr::String& out_shader_code)
{
    for (const Rndr::String& define : defines)
    {
        out_shader_code += "#define " + define + "\n";
    }
}
}  // namespace

Rndr::Shader::Shader(const GraphicsContext& graphics_context, const ShaderDesc& desc) : m_desc(desc)
{
    RNDR_TRACE_SCOPED(Create Shader);

    RNDR_UNUSED(graphics_context);
    const GLenum shader_type = Rndr::FromShaderTypeToOpenGL(desc.type);
    m_native_shader = glCreateShader(shader_type);
    RNDR_ASSERT_OPENGL();
    if (m_native_shader == k_invalid_opengl_object)
    {
        return;
    }
    String defines_shader_code;
    GenerateDefinesShaderCode(desc.defines, defines_shader_code);
    String final_shader_code = desc.source;
    if (!defines_shader_code.empty())
    {
        size_t pos = desc.source.find("#version");
        RNDR_ASSERT(pos != String::npos);
        pos = desc.source.find('\n', pos);
        RNDR_ASSERT(pos != String::npos);
        final_shader_code.insert(pos + 1, defines_shader_code);
    }
    const char* final_shader_code_c_str = final_shader_code.c_str();
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
