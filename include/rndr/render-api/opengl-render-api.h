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
class Buffer;
class Image;

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

    /**
     * Binds a swap chain to the graphics pipeline.
     * @param swap_chain The swap chain to bind.
     * @return Returns true if the swap chain was bound successfully, false otherwise.
     */
    bool Bind(const SwapChain& swap_chain);

    /**
     * Binds a pipeline object to the graphics pipeline.
     * @param pipeline The pipeline to bind.
     * @return Returns true if the pipeline was bound successfully, false otherwise.
     */
    bool Bind(const Pipeline& pipeline);

    /**
     * Binds a buffer to the graphics pipeline.
     * @param buffer The buffer to bind.
     * @param binding_index The binding index to bind the buffer to.
     * @return Returns true if the buffer was bound successfully, false otherwise.
     */
    bool Bind(const Buffer& buffer, int32_t binding_index);

    /**
     * Binds an image to the graphics pipeline.
     * @param image The image to bind.
     * @return Returns true if the image was bound successfully, false otherwise.
     */
    bool Bind(const Image& image);

    /**
     * Swaps the front and back buffers of the swap chain.
     * @param swap_chain The swap chain to present.
     * @param vertical_sync Whether to wait for the vertical blank before presenting.
     * @return Returns true if the swap chain was presented successfully, false otherwise.
     */
    bool Present(const SwapChain& swap_chain, bool vertical_sync);

    /**
     * Clears the color image in the bound frame buffer.
     * @param color The color to clear the image to.
     * @return Returns true if the image was cleared successfully, false otherwise.
     */
    bool ClearColor(const math::Vector4& color);

    /**
     * Clears the color and depth images in the bound frame buffer.
     * @param color Color to clear the color image to.
     * @param depth Depth value to clear the depth image to. Default is 1.
     * @return Returns true if the images were cleared successfully, false otherwise.
     */
    bool ClearColorAndDepth(const math::Vector4& color, real depth = MATH_REALC(1.0));

    /**
     * Draws triangles.
     * @param vertex_count The number of vertices to draw.
     * @param instance_count The number of instances to draw.
     * @param first_vertex The index of the first vertex to draw.
     * @param first_instance The index of the first instance to draw.
     * @return Returns true if the draw call was successful, false otherwise.
     */
    bool Draw(uint32_t vertex_count,
              uint32_t instance_count,
              uint32_t first_vertex = 0,
              uint32_t first_instance = 0);

    /**
     * Updates the contents of a buffer.
     * @param buffer The buffer to update.
     * @param data The data to update the buffer with.
     * @param offset The offset into the buffer to update.
     * @return Returns true if the buffer was updated successfully, false otherwise.
     */
    bool Update(Buffer& buffer, const ByteSpan& data, uint32_t offset = 0);

    /**
     * Reads the contents of a swap chain color image.
     * @param swap_chain The swap chain to read.
     * @return Returns the image data.
     */
    [[nodiscard]] struct CPUImage ReadSwapChain(const SwapChain& swap_chain);

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

/**
 * Represents a linear array of memory on the GPU.
 */
class Buffer
{
public:
    Buffer(const GraphicsContext& graphics_context,
           const BufferDesc& desc,
           const ByteSpan& init_data = ByteSpan{});
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

/**
 * Represents a texture on the GPU.
 */
class Image
{
public:
    /**
     * Creates a new image. Only creates Image2D so any other type will result in invalid image.
     * @param graphics_context The graphics context to create the image with.
     * @param desc The description of the image to create.
     * @param init_data The initial data to fill the image with. Default is empty.
     */
    Image(const GraphicsContext& graphics_context,
          const ImageDesc& desc,
          const ByteSpan& init_data = ByteSpan{});

    /**
     * Creates a new image from a CPU image. Only creates Image2D so any other type will result in
     * invalid image.
     * @param graphics_context The graphics context to create the image with.
     * @param cpu_image The CPU image to create the image with.
     * @param use_mips Whether or not to use mips for the image.
     * @param sampler_desc The sampler description to use for the image.
     */
    Image(const GraphicsContext& graphics_context,
          struct CPUImage& cpu_image,
          bool use_mips,
          const SamplerDesc& sampler_desc);

    ~Image();
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const ImageDesc& GetDesc() const;
    [[nodiscard]] GLuint GetNativeTexture() const;

private:
    ImageDesc m_desc;
    GLuint m_native_texture = k_invalid_opengl_object;
};

}  // namespace Rndr

#endif  // RNDR_OPENGL
