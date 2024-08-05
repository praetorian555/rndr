#pragma once

#include "rndr/definitions.h"
#include "rndr/graphics-types.h"
#include "rndr/platform/opengl-forward-def.h"

namespace Rndr
{

class GraphicsContext;

/**
 * Represents a program to be executed on a GPU.
 */
class Shader
{
public:
    /**
     * Default constructor. Creates an invalid shader.
     */
    Shader() = default;

    /**
     * Creates a new shader.
     * @param graphics_context The graphics context to create the shader with.
     * @param desc The description of the shader to create.
     */
    Shader(const GraphicsContext& graphics_context, const ShaderDesc& desc);

    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const ShaderDesc& GetDesc() const;
    [[nodiscard]] const GLuint GetNativeShader() const;

private:
    ShaderDesc m_desc;
    GLuint m_native_shader = k_invalid_opengl_object;
};

}  // namespace Rndr
