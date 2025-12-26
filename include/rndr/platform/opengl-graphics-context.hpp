#pragma once

#include "opengl-command-list.hpp"
#include "rndr/error-codes.hpp"
#include "rndr/exception.hpp"
#include "rndr/graphics-types.hpp"

namespace Rndr
{

class SwapChain;
class Shader;
class Pipeline;
class Buffer;
class Texture;
class Bitmap;
class FrameBuffer;

/**
 * Represents a graphics context. This is the main entry point for the graphics API. It is used to
 * create all other graphics objects as well as to submit commands to the GPU.
 */
class GraphicsContext
{
public:
    explicit GraphicsContext(const GraphicsContextDesc& desc = {});
    ~GraphicsContext();
    GraphicsContext(const GraphicsContext&) = delete;
    GraphicsContext& operator=(const GraphicsContext&) = delete;
    GraphicsContext(GraphicsContext&& other) noexcept;
    GraphicsContext& operator=(GraphicsContext&& other) noexcept;

    ErrorCode Init(const GraphicsContextDesc& desc = {});
    void Destroy();

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const GraphicsContextDesc& GetDesc() const;

    /**
     * Swaps the front and back buffers of the swap chain.
     * @param swap_chain The swap chain to present.
     * @return Returns true if the swap chain was presented successfully, false otherwise.
     */
    bool Present(const SwapChain& swap_chain);

    /**
     * Clears the color image in the bound frame buffer.
     * @param color The color to clear the image to.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     */
    void ClearColor(const Vector4f& color);

    /**
     * Clears the depth image in the bound frame buffer.
     * @param depth The depth value to clear the image to.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     */
    void ClearDepth(float depth);

    /**
     * Clears the stencil image in the bound frame buffer.
     * @param stencil The stencil value to clear the image to.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     */
    void ClearStencil(i32 stencil);

    /**
     * Clears the color and depth images in the bound frame buffer.
     * @param color Color to clear the color image to.
     * @param depth Depth value to clear the depth image to. Default is 1.
     * @param stencil Stencil value to clear the stencil image to. Default is 0.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     */
    void ClearAll(const Vector4f& color, float depth = 1.0f, i32 stencil = 0);

    /**
     * Binds a pipeline object to the graphics pipeline.
     * @param pipeline The pipeline to bind.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException when the pipeline object argument is not valid.
     */
    void BindPipeline(const Pipeline& pipeline);

    /**
     * Binds a buffer to the graphics pipeline.
     * @param buffer The buffer to bind.
     * @param binding_index The binding index to bind the buffer to. Only relevant for constant and shader storage buffer.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException if the buffer object is not valid or buffer type is not Constant or ShaderStorage.
     */
    void BindBuffer(const Buffer& buffer, i32 binding_index);

    /**
     * Binds a texture to the graphics pipeline.
     * @param image The texture to bind.
     * @param binding_index The binding index to bind the texture to.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException if the texture object is not valid.
     */
    void BindTexture(const Texture& image, i32 binding_index);

    /**
     * Binds one level of the texture to the compute pipeline.
     * @param texture The texture to bind.
     * @param binding_index The binding index to bind the texture to.
     * @param texture_level The texture level to bind.
     * @param access How the texture will be accessed in the compute shader.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException if the texture object is not valid.
     */
    void BindTextureForCompute(const Texture& texture, i32 binding_index, i32 texture_level, TextureAccess access);

    /**
     * Binds a frame buffer to the graphics pipeline.
     * @param frame_buffer The frame buffer to bind.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException if the frame buffer object is not valid.
     */
    void BindFrameBuffer(const FrameBuffer& frame_buffer);

    /**
     * Binds a swap chain frame buffer to the graphics pipeline. This is a default frame buffer that is created by the swap chain.
     * @param swap_chain The swap chain to bind.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException if the swap chain object is not valid.
     */
    void BindSwapChainFrameBuffer(const SwapChain& swap_chain);

    /**
     * Draws primitives without use of index buffer. It will behave as if indices were specified
     * sequentially starting from 0.
     * @param topology The primitive topology to draw.
     * @param vertex_count The number of vertices to draw.
     * @param instance_count The number of instances to draw. By default this is 1.
     * @param first_vertex The index of the first vertex to draw. By default this is 0.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     */
    void DrawVertices(PrimitiveTopology topology, i32 vertex_count, i32 instance_count = 1, i32 first_vertex = 0);

    /**
     * Draws primitives using an index buffer.
     * @param topology The primitive topology to draw.
     * @param index_count The number of indices to draw.
     * @param instance_count The number of instances to draw. By default, this is 1.
     * @param first_index The index of the first index to draw. By default, this is 0.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     */
    void DrawIndices(PrimitiveTopology topology, i32 index_count, i32 instance_count = 1, i32 first_index = 0);

    /**
     * Dispatches a compute shader.
     * @param block_count_x Number of blocks in the x dimension.
     * @param block_count_y Number of blocks in the y dimension.
     * @param block_count_z Number of blocks in the z dimension.
     * @param wait_for_completion Whether to wait for the compute shader to finish executing before returning. Default is true.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     */
    void DispatchCompute(u32 block_count_x, u32 block_count_y, u32 block_count_z, bool wait_for_completion = true);

    /**
     * Updates the contents of a buffer.
     * @param buffer The buffer to update.
     * @param data The data to update the buffer with.
     * @param offset The offset into the buffer to update, measured in bytes.
     * @note Empty array will do nothing and it will return ErrorCode::Success.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException if the buffer object is not valid or its usage is not set to Usage::Dynamic.
     * @throw Opal::OutOfBoundsException when offset value is out of bounds of the buffer or if the data size is negative or in combination
     * with offset exceeds buffer size.
     */
    void UpdateBuffer(const Buffer& buffer, const Opal::ArrayView<const u8>& data, i64 offset = 0);

    /**
     * Reads the contents of a buffer.
     * @param buffer The buffer to read.
     * @param out_data Where to store read data.
     * @param offset From which byte to start reading. Default is 0. Offset should be between 0 and buffer size.
     * @param size How many bytes to read. If 0, reads the whole buffer. Default is 0. Size should be between 0 and buffer size.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException if the buffer object is not valid or its usage is not set to Usage::Readback.
     * @throw Opal::OutOfBoundsException when offset value is out of bounds of the buffer or if the read size is negative or in combination
     * with offset exceeds buffer size.
     */
    void ReadBuffer(const Buffer& buffer, Opal::ArrayView<u8>& out_data, i32 offset = 0, i32 size = 0) const;

    /**
     * Copies the contents of one buffer to another. Source and destination buffers can be the same, but the ranges must not overlap.
     * @param dst_buffer Buffer to copy to.
     * @param src_buffer Buffer to copy from.
     * @param dst_offset Offset into the destination buffer to copy to. Default is 0. Must be between 0 and destination buffer size.
     * @param src_offset Offset into the source buffer to copy from. Default is 0. Must be between 0 and source buffer size.
     * @param size How many bytes to copy. If 0, copies the whole source buffer. Default is 0. Must be between 0 and the minimum of
     * destination and source buffer sizes.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException if one of the buffer objects is invalid or they both point to the same buffer object and the
     * source and destination ranges overlap.
     * @throw Opal::OutOfBoundsException when the offsets or the source and destination sizes are out of bounds.
     */
    void CopyBuffer(const Buffer& dst_buffer, const Buffer& src_buffer, i32 dst_offset = 0, i32 src_offset = 0, i32 size = 0);

    /**
     * Read the contents of an image.
     * @param image The image to read.
     * @param out_data Where to store read data.
     * @param level Which mip level to read. Default is 0.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException when texture object is invalid.
     */
    void Read(const Texture& image, Bitmap& out_data, i32 level = 0) const;

    /**
     * Read the contents of a swap chain color buffer.
     * @param swap_chain The swap chain to read.
     * @param out_bitmap Where to store read data.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     */
    void ReadSwapChainColor(const SwapChain& swap_chain, Bitmap& out_bitmap);

    /**
     * Read the contents of a swap chain depth stencil buffer.
     * @param swap_chain The swap chain to read.
     * @param out_bitmap Where to store read data.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     */
    void ReadSwapChainDepthStencil(const SwapChain& swap_chain, Bitmap& out_bitmap);

    /**
     * Clears the specified color attachment with a given color in the given frame buffer.
     * @param frame_buffer The frame buffer to clear.
     * @param color_attachment_index The index of the color attachment to clear.
     * @param color The color to clear the attachment to.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException when frame buffer object is invalid.
     * @throw Opal::OutOfBoundsException when color attachment index is out of bounds.
     */
    void ClearFrameBufferColorAttachment(const FrameBuffer& frame_buffer, i32 color_attachment_index, const Vector4f& color);

    /**
     * Clears the depth and stencil attachments with given values in the given frame buffer.
     * @param frame_buffer The frame buffer to clear.
     * @param depth The depth value to clear the attachment to.
     * @param stencil The stencil value to clear the attachment to.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException when frame buffer object is invalid.
     */
    void ClearFrameBufferDepthStencilAttachment(const FrameBuffer& frame_buffer, f32 depth, i32 stencil);

    /**
     * Copy attachments from the 'src' frame buffer to the 'dst' frame buffer.
     * @param dst Destination frame buffer. Does not need to be bound beforehand.
     * @param src Source frame buffer. Does not need to be bound beforehand.
     * @param desc Describes the rules for copying.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException when destination or source frame buffer objects are invalid, or if we are copying depth and
     * stencil attachments but filtering is not set to ImageFilter::Nearest.
     */
    void BlitFrameBuffers(const FrameBuffer& dst, const FrameBuffer& src, const BlitFrameBufferDesc& desc);

    /**
     * Copy attachments from the 'src' frame buffer to the swap chain's frame buffer.
     * @param swap_chain The swap chain to copy to.
     * @param src Source frame buffer. Does not need to be bound beforehand.
     * @param desc Describes the rules for copying.
     * @throw GraphicsAPIException when there is an error in the underlying graphics API.
     * @throw Opal::InvalidArgumentException when source frame buffer object is invalid, when swap chain object is invalid or if we are
     * copying depth and stencil attachments but filtering is not set to ImageFilter::Nearest.
     */
    void BlitToSwapChain(const SwapChain& swap_chain, const FrameBuffer& src, const BlitFrameBufferDesc& desc);

    /**
     * Submits commands in the command list to the graphics API.
     * @param command_list Command list to submit.
     */
    void SubmitCommandList(CommandList& command_list);

private:
    GraphicsContextDesc m_desc;
    NativeDeviceContextHandle m_native_device_context = k_invalid_device_context_handle;
    NativeGraphicsContextHandle m_native_graphics_context = k_invalid_graphics_context_handle;

    Opal::Ref<const Pipeline> m_bound_pipeline;
};

}  // namespace Rndr
