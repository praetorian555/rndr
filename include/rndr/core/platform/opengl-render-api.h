#pragma once

#include "rndr/core/base.h"

#if RNDR_OPENGL

#include "rndr/core/bitmap.h"
#include "rndr/core/graphics-types.h"
#include "rndr/core/platform/opengl-command-list.h"
#include "rndr/core/platform/opengl-forward-def.h"

namespace Rndr
{

class SwapChain;
class Shader;
class Pipeline;
class Buffer;
class Image;
class GraphicsContext;

/**
 * Represents a series of virtual frame buffers used by the graphics card and the OS to render to
 * the screen. Primary use is to present the rendered image to the screen.
 */
class SwapChain
{
public:
    /**
     * Default constructor. Creates an invalid swap chain.
     */
    SwapChain() = default;

    /**
     * Creates a new swap chain.
     * @param graphics_context The graphics context to create the swap chain with.
     * @param desc The description of the swap chain to create.
     */
    SwapChain(const GraphicsContext& graphics_context, const SwapChainDesc& desc);

    ~SwapChain() = default;
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;
    SwapChain(SwapChain&& other) noexcept = default;
    SwapChain& operator=(SwapChain&& other) noexcept = default;

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const SwapChainDesc& GetDesc() const;

    bool SetSize(int32_t width, int32_t height);
    bool SetVerticalSync(bool vertical_sync);

private:
    SwapChainDesc m_desc;
};

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

/**
 * Represents a linear array of memory on the GPU.
 */
class Buffer
{
public:
    /**
     * Default constructor. Creates an invalid buffer.
     */
    Buffer() = default;

    /**
     * Create a new buffer.
     * @param graphics_context The graphics context to create the buffer with.
     * @param desc The description of the buffer to create.
     * @param init_data The initial data to fill the buffer with. If empty, the buffer will be filled with zeros. Default is empty.
     */
    Buffer(const GraphicsContext& graphics_context, const BufferDesc& desc, const ConstByteSpan& init_data = ConstByteSpan{});

    ~Buffer();
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const BufferDesc& GetDesc() const;
    [[nodiscard]] GLuint GetNativeBuffer() const;

private:
    BufferDesc m_desc;
    GLuint m_native_buffer = k_invalid_opengl_object;
};

}  // namespace Rndr

#endif  // RNDR_OPENGL
