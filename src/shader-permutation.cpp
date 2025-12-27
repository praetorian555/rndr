#include "rndr/shader-permutation.hpp"

Rndr::ShaderPermutation::ShaderPermutation(Opal::Ref<GraphicsContext> graphics_context, const ShaderDesc& shader_desc)
    : m_shader_desc(shader_desc), m_graphics_context(std::move(graphics_context))
{
    m_shader = Shader(m_graphics_context, shader_desc);
    if (!m_shader.IsValid())
    {
        throw Opal::Exception("Failed to create shader permutation!");
    }
}
Rndr::ShaderPermutation::~ShaderPermutation()
{
    Destroy();
}

Rndr::ShaderPermutation::ShaderPermutation(ShaderPermutation&& other) noexcept
    : m_shader_desc(std::move(other.m_shader_desc)), m_graphics_context(std::move(other.m_graphics_context)), m_shader(std::move(other.m_shader))
{
}

Rndr::ShaderPermutation& Rndr::ShaderPermutation::operator=(ShaderPermutation&& other) noexcept
{
    Destroy();
    m_shader = std::move(other.m_shader);
    m_graphics_context = std::move(other.m_graphics_context);
    m_shader_desc = std::move(other.m_shader_desc);
    return *this;
}

void Rndr::ShaderPermutation::Destroy()
{
    m_shader.Destroy();
}

bool Rndr::ShaderPermutation::operator==(const ShaderPermutation& other) const
{
    if (m_shader_desc.type != other.m_shader_desc.type)
    {
        return false;
    }
    if (m_shader_desc.source != other.m_shader_desc.source)
    {
        return false;
    }
    if (m_shader_desc.defines != other.m_shader_desc.defines)
    {
        return false;
    }
    return true;
}

bool Rndr::ShaderPermutation::IsSameShader(const ShaderPermutation& other) const
{
    if (m_shader_desc.type != other.m_shader_desc.type)
    {
        return false;
    }
    if (m_shader_desc.source != other.m_shader_desc.source)
    {
        return false;
    }
    return true;
}
