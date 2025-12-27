#pragma once

#include "rndr/render-api.hpp"

namespace Rndr
{

class ShaderPermutation
{
public:
    ShaderPermutation() = default;
    ShaderPermutation(Opal::Ref<GraphicsContext> graphics_context, const ShaderDesc& shader_desc);
    ~ShaderPermutation();

    ShaderPermutation(const ShaderPermutation&) = delete;
    ShaderPermutation& operator=(const ShaderPermutation&) = delete;

    ShaderPermutation(ShaderPermutation&&) noexcept;
    ShaderPermutation& operator=(ShaderPermutation&&) noexcept;

    void Destroy();

    [[nodiscard]] const ShaderDesc& GetShaderDesc() const { return m_shader_desc; }
    Opal::Ref<Shader> GetShader() { return Opal::Ref{m_shader}; }

    [[nodiscard]] bool IsValid() const { return m_shader.IsValid(); }

    // Checks type, source and defines
    [[nodiscard]] bool operator==(const ShaderPermutation& other) const;
    // Checks only the type and source, but ignores defines
    [[nodiscard]] bool IsSameShader(const ShaderPermutation& other) const;

private:
    ShaderDesc m_shader_desc;
    Opal::Ref<GraphicsContext> m_graphics_context;
    Shader m_shader;
};

}