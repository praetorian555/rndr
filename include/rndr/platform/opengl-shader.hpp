#pragma once

#include "rndr/definitions.hpp"
#include "rndr/error-codes.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/platform/opengl-forward-def.hpp"

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
     * Creates and compiles the shader.
     * @param graphics_context The graphics context to create the shader with.
     * @param desc The description of the shader to create.
     */
    Shader(const GraphicsContext& graphics_context, const ShaderDesc& desc);

    /**
     * Creates and compiles the shader.
     * @param graphics_context The graphics context to create the shader with.
     * @param desc The description of the shader to create.
     * @return ErrorCode::Success if the shader was created successfully. ErrorCode::InvalidArgument if the shader description is invalid.
     * ErrorCode::OutOfMemory if there was an error creating the shader. ErrorCode::GraphicsAPIError if one of the OpenGL calls failed.
     * ErrorCode::ShaderCompilationError if there was an error compiling the shader.
     */
    Rndr::ErrorCode Initialize(const GraphicsContext& graphics_context, const ShaderDesc& desc);

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
