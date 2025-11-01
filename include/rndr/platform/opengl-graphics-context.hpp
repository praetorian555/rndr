#pragma once

#include "opengl-command-list.hpp"
#include "rndr/definitions.hpp"
#include "rndr/error-codes.hpp"
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
    GraphicsContext() = default;
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
     * @return Returns ErrorCode::Success if the color buffer has been cleared, ErrorCode::GraphicsAPIError otherwise.
     */
    ErrorCode ClearColor(const Vector4f& color);

    /**
     * Clears the depth image in the bound frame buffer.
     * @param depth The depth value to clear the image to.
     * @return Returns ErrorCode::Success if the depth buffer has been cleared, ErrorCode::GraphicsAPIError otherwise.
     */
    ErrorCode ClearDepth(float depth);

    /**
     * Clears the stencil image in the bound frame buffer.
     * @param stencil The stencil value to clear the image to.
     * @return Returns ErrorCode::Success if the stencil buffer has been cleared, ErrorCode::GraphicsAPIError otherwise.
     */
    ErrorCode ClearStencil(i32 stencil);

    /**
     * Clears the color and depth images in the bound frame buffer.
     * @param color Color to clear the color image to.
     * @param depth Depth value to clear the depth image to. Default is 1.
     * @param stencil Stencil value to clear the stencil image to. Default is 0.
     * @return Returns ErrorCode::Success if the color, depth and stencil buffers have been cleared, ErrorCode::GraphicsAPIError otherwise.
     */
    ErrorCode ClearAll(const Vector4f& color, float depth = 1.0f, i32 stencil = 0);

    /**
     * Binds a pipeline object to the graphics pipeline.
     * @param pipeline The pipeline to bind.
     * @return Returns ErrorCode::Success if the pipeline was bound successfully. Returns ErrorCode::InvalidArgument if the pipeline is not
     * valid. Returns ErrorCode::GraphicsAPIError if OpenGL failed to bind the pipeline.
     */
    ErrorCode BindPipeline(const Pipeline& pipeline);

    /**
     * Binds a buffer to the graphics pipeline.
     * @param buffer The buffer to bind.
     * @param binding_index The binding index to bind the buffer to. Only relevant for constant and shader storage buffer.
     * @return Returns ErrorCode::Success if the buffer was bound successfully. Returns ErrorCode::InvalidArgument if the buffer is not
     * valid. Returns ErrorCode::GraphicsAPIError if OpenGL failed to bind the buffer.
     */
    ErrorCode BindBuffer(const Buffer& buffer, i32 binding_index);

    /**
     * Binds a texture to the graphics pipeline.
     * @param image The texture to bind.
     * @param binding_index The binding index to bind the texture to.
     * @return Returns ErrorCode::Success if the texture was bound successfully. Returns ErrorCode::InvalidArgument if the texture is not
     * valid. Returns ErrorCode::GraphicsAPIError if OpenGL failed to bind the texture.
     */
    ErrorCode BindTexture(const Texture& image, i32 binding_index);

    /**
     * Binds one level of the texture to the compute pipeline.
     * @param texture The texture to bind.
     * @param binding_index The binding index to bind the texture to.
     * @param texture_level The texture level to bind.
     * @param access How the texture will be accessed in the compute shader.
     * @return Returns ErrorCode::Success if the texture was bound successfully. Returns ErrorCode::InvalidArgument if the texture is not
     * valid. Returns ErrorCode::GraphicsAPIError if OpenGL failed to bind the texture.
     */
    ErrorCode BindTextureForCompute(const Texture& texture, i32 binding_index, i32 texture_level, TextureAccess access);

    /**
     * Binds a frame buffer to the graphics pipeline.
     * @param frame_buffer The frame buffer to bind.
     * @return Returns ErrorCode::Success if the frame buffer was bound successfully. Returns ErrorCode::InvalidArgument if the frame buffer
     * is not valid. Returns ErrorCode::GraphicsAPIError if OpenGL failed to bind the frame buffer.
     */
    ErrorCode BindFrameBuffer(const FrameBuffer& frame_buffer);

    /**
     * Binds a swap chain frame buffer to the graphics pipeline. This is a default frame buffer that is created by the swap chain.
     * @param swap_chain The swap chain to bind.
     * @return Returns ErrorCode::Success if the swap chain frame buffer was bound successfully. Returns ErrorCode::InvalidArgument if the
     * swap chain is not valid. Returns ErrorCode::GraphicsAPIError if OpenGL failed to bind the swap chain frame buffer.
     */
    ErrorCode BindSwapChainFrameBuffer(const SwapChain& swap_chain);

    /**
     * Draws primitives without use of index buffer. It will behave as if indices were specified
     * sequentially starting from 0.
     * @param topology The primitive topology to draw.
     * @param vertex_count The number of vertices to draw.
     * @param instance_count The number of instances to draw. By default this is 1.
     * @param first_vertex The index of the first vertex to draw. By default this is 0.
     * @return Returns true if the draw call was successful, false otherwise.
     */
    bool DrawVertices(PrimitiveTopology topology, i32 vertex_count, i32 instance_count = 1, i32 first_vertex = 0);

    /**
     * Draws primitives using an index buffer.
     * @param topology The primitive topology to draw.
     * @param index_count The number of indices to draw.
     * @param instance_count The number of instances to draw. By default this is 1.
     * @param first_index The index of the first index to draw. By default this is 0.
     * @return Returns true if the draw call was successful, false otherwise.
     */
    bool DrawIndices(PrimitiveTopology topology, i32 index_count, i32 instance_count = 1, i32 first_index = 0);

    /**
     * Dispatches a compute shader.
     * @param block_count_x Number of blocks in the x dimension.
     * @param block_count_y Number of blocks in the y dimension.
     * @param block_count_z Number of blocks in the z dimension.
     * @param wait_for_completion Whether to wait for the compute shader to finish executing before returning. Default is true.
     * @return Returns true if the dispatch was successful, false otherwise.
     */
    bool DispatchCompute(u32 block_count_x, u32 block_count_y, u32 block_count_z, bool wait_for_completion = true);

    /**
     * Updates the contents of a buffer.
     * @param buffer The buffer to update.
     * @param data The data to update the buffer with.
     * @param offset The offset into the buffer to update, measured in bytes.
     * @return In case of a success ErrorCode::Success is returned. If the buffer is not valid, or it doesn't have Usage::Dynamic,
     * ErrorCode::InvalidArgument is returned. If the data is negative or in combination with the offset exceeds the buffer size,
     * ErrorCode::OutOfBounds is returned. If offset is out of bounds of the buffer, ErrorCode::OutOfBounds is returned.
     * @note Empty array will do nothing and it will return ErrorCode::Success.
     */
    ErrorCode UpdateBuffer(const Buffer& buffer, const Opal::ArrayView<const u8>& data, i64 offset = 0);

    /**
     * Reads the contents of a buffer.
     * @param buffer The buffer to read.
     * @param out_data Where to store read data.
     * @param offset From which byte to start reading. Default is 0. Offset should be between 0 and buffer size.
     * @param size How many bytes to read. If 0, reads the whole buffer. Default is 0. Size should be between 0 and buffer size.
     * @return Returns ErrorCode::Success if the read was successful. Returns ErrorCode::InvalidArgument if the buffer is not valid or the
     * buffer's usage is not ReadBack. Returns ErrorCode::OutOfBounds if the offset or size is out of bounds of the buffer.
     */
    ErrorCode ReadBuffer(const Buffer& buffer, Opal::ArrayView<u8>& out_data, i32 offset = 0, i32 size = 0) const;

    /**
     * Copies the contents of one buffer to another. Source and destination buffers can be the same, but the ranges must not overlap.
     * @param dst_buffer Buffer to copy to.
     * @param src_buffer Buffer to copy from.
     * @param dst_offset Offset into the destination buffer to copy to. Default is 0. Must be between 0 and destination buffer size.
     * @param src_offset Offset into the source buffer to copy from. Default is 0. Must be between 0 and source buffer size.
     * @param size How many bytes to copy. If 0, copies the whole source buffer. Default is 0. Must be between 0 and the minimum of
     * destination and source buffer sizes.
     * @return Returns ErrorCode::Success if the copy was successful. Returns ErrorCode::InvalidArgument if the source or destination buffer
     * are not valid. Returns ErrorCode::OutOfBounds if the offsets or size are out of bounds of the source or destination buffer.
     */
    ErrorCode CopyBuffer(const Buffer& dst_buffer, const Buffer& src_buffer, i32 dst_offset = 0, i32 src_offset = 0, i32 size = 0);

    /**
     * Read the contents of an image.
     * @param image The image to read.
     * @param out_data Where to store read data.
     * @param level Which mip level to read. Default is 0.
     */
    bool Read(const Texture& image, Bitmap& out_data, i32 level = 0) const;

    /**
     * Read the contents of a swap chain color buffer.
     * @param swap_chain The swap chain to read.
     * @param out_bitmap Where to store read data.
     * @return Returns true if the read was successful, false otherwise.
     */
    [[nodiscard]] bool ReadSwapChainColor(const SwapChain& swap_chain, Bitmap& out_bitmap);

    /**
     * Read the contents of a swap chain depth stencil buffer.
     * @param swap_chain The swap chain to read.
     * @param out_bitmap Where to store read data.
     * @return Returns true if the read was successful, false otherwise.
     */
    [[nodiscard]] bool ReadSwapChainDepthStencil(const SwapChain& swap_chain, Bitmap& out_bitmap);

    /**
     * Clears the specified color attachment with a given color in the given frame buffer.
     * @param frame_buffer The frame buffer to clear.
     * @param color_attachment_index The index of the color attachment to clear.
     * @param color The color to clear the attachment to.
     * @return Returns ErrorCode::Success if the color attachment was cleared successfully. Returns ErrorCode::InvalidArgument if the frame
     * buffer is not valid. Returns ErrorCode::OutOfBounds if the color attachment index is out of bounds. Returns
     * ErrorCode::GraphicsAPIError if OpenGL failed to clear the color attachment.
     */
    ErrorCode ClearFrameBufferColorAttachment(const FrameBuffer& frame_buffer, i32 color_attachment_index, const Vector4f& color);

    /**
     * Clears the depth and stencil attachments with given values in the given frame buffer.
     * @param frame_buffer The frame buffer to clear.
     * @param depth The depth value to clear the attachment to.
     * @param stencil The stencil value to clear the attachment to.
     * @return Returns ErrorCode::Success if the depth and stencil attachments were cleared successfully. Returns ErrorCode::InvalidArgument
     * if the frame buffer is not valid. Returns ErrorCode::GraphicsAPIError if OpenGL failed to clear the depth and stencil attachments.
     */
    ErrorCode ClearFrameBufferDepthStencilAttachment(const FrameBuffer& frame_buffer, f32 depth, i32 stencil);

    /**
     * Copy attachments from the 'src' frame buffer to the 'dst' frame buffer.
     * @param dst Destination frame buffer. Does not need to be bound beforehand.
     * @param src Source frame buffer. Does not need to be bound beforehand.
     * @param desc Describes the rules for copying.
     * @return Returns ErrorCode::Success if copying is performed successfully. Returns ErrorCode::InvalidArgument if one of the frame
     * buffers is not valid. In case that we want to copy depth or stencil attachments and specified linear interpolation return the
     * ErrorCode::InvalidArgument. Returns ErrorCode::GraphicsAPIError if the error happens on the graphics API side.
     */
    ErrorCode BlitFrameBuffers(const FrameBuffer& dst, const FrameBuffer& src, const BlitFrameBufferDesc& desc);

    /**
     * Copy attachments from the 'src' frame buffer to the swap chain's frame buffer.
     * @param swap_chain The swap chain to copy to.
     * @param src Source frame buffer. Does not need to be bound beforehand.
     * @param desc Describes the rules for copying.
     * @return Returns ErrorCode::Success if copying is performed successfully. Returns ErrorCode::InvalidArgument if the source frame
     * buffer is not valid or the swap chain is not valid. In case that we want to copy depth or stencil attachments and specified linear
     * interpolation return the ErrorCode::InvalidArgument. Returns ErrorCode::GraphicsAPIError if the error happens on the graphics API side.
     */
    ErrorCode BlitToSwapChain(const SwapChain& swap_chain, const FrameBuffer& src, const BlitFrameBufferDesc& desc);

    void SubmitCommandList(Rndr::CommandList& command_list);

private:
    GraphicsContextDesc m_desc;
    NativeDeviceContextHandle m_native_device_context = k_invalid_device_context_handle;
    NativeGraphicsContextHandle m_native_graphics_context = k_invalid_graphics_context_handle;

    Opal::Ref<const Pipeline> m_bound_pipeline;
};

}  // namespace Rndr
