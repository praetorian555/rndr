#pragma once

#include "rndr/core/base.h"

#if RNDR_OPENGL

#include "rndr/core/forward-def-opengl.h"
#include "rndr/core/graphics-types.h"

namespace Rndr
{

class SwapChain;
class Shader;
class Pipeline;
class InputLayout;

/**
 * Represents a graphics context. This is the main entry point for the graphics API. It is used to
 * create all other graphics objects as well as to submit commands to the GPU.
 */
class GraphicsContext
{
public:
    explicit GraphicsContext(const GraphicsContextDesc& desc);
    ~GraphicsContext();
    GraphicsContext(const GraphicsContext&) = delete;
    GraphicsContext& operator=(const GraphicsContext&) = delete;
    GraphicsContext(GraphicsContext&& other) noexcept;
    GraphicsContext& operator=(GraphicsContext&& other) noexcept;

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const GraphicsContextDesc& GetDesc() const;

    bool Bind(const SwapChain& swap_chain);
    bool Bind(const Pipeline& pipeline);

    bool Present(const SwapChain& swap_chain, bool vertical_sync);

    bool ClearColor(const math::Vector4& color);

    bool Draw(uint32_t vertex_count,
              uint32_t instance_count,
              uint32_t first_vertex = 0,
              uint32_t first_instance = 0);

private:
    GraphicsContextDesc m_desc;
    NativeDeviceContextHandle m_native_device_context = k_invalid_device_context_handle;
    NativeGraphicsContextHandle m_native_graphics_context = k_invalid_graphics_context_handle;
};

/**
 * Represents a series of virtual frame buffers used by the graphics card and the OS to render to
 * the screen. Primary use is to present the rendered image to the screen.
 */
class SwapChain
{
public:
    SwapChain(const GraphicsContext& graphics_context, const SwapChainDesc& desc);
    ~SwapChain() = default;
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;
    SwapChain(SwapChain&& other) noexcept = default;
    SwapChain& operator=(SwapChain&& other) noexcept = default;

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const SwapChainDesc& GetDesc() const;

    bool SetSize(int32_t width, int32_t height);

private:
    SwapChainDesc m_desc;
};

/**
 * Represents a program to be executed on a GPU.
 */
class Shader
{
public:
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

/**
 * Represents a state of the graphics pipeline. This includes the shaders, input layout, rasterizer
 * state, blend state, depth stencil state, and frame buffer.
 */
class Pipeline
{
public:
    Pipeline(const GraphicsContext& graphics_context, const PipelineDesc& desc);
    ~Pipeline();
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&& other) noexcept;
    Pipeline& operator=(Pipeline&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const PipelineDesc& GetDesc() const;
    [[nodiscard]] GLuint GetNativeShaderProgram() const;
    [[nodiscard]] GLuint GetNativeVertexArray() const;

private:
    PipelineDesc m_desc;
    GLuint m_native_shader_program = k_invalid_opengl_object;
    GLuint m_native_vertex_array = k_invalid_opengl_object;
};

}  // namespace Rndr

#endif  // RNDR_OPENGL
