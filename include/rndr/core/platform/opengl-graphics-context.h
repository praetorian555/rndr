#pragma once

#include "rndr/core/definitions.h"
#include "rndr/core/error-codes.h"
#include "rndr/core/graphics-types.h"

namespace Rndr
{

class SwapChain;
class Shader;
class Pipeline;
class Buffer;
class Image;
class Bitmap;
class FrameBuffer;

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
     * @return Returns true if the image was cleared successfully, false otherwise.
     */
    bool ClearColor(const Vector4f& color);

    /**
     * Clears the depth image in the bound frame buffer.
     * @param depth The depth value to clear the image to.
     * @return Returns true if the image was cleared successfully, false otherwise.
     */
    bool ClearDepth(float depth);

    /**
     * Clears the stencil image in the bound frame buffer.
     * @param stencil The stencil value to clear the image to.
     * @return Returns true if the image was cleared successfully, false otherwise.
     */
    bool ClearStencil(i32 stencil);

    /**
     * Clears the color and depth images in the bound frame buffer.
     * @param color Color to clear the color image to.
     * @param depth Depth value to clear the depth image to. Default is 1.
     * @param stencil Stencil value to clear the stencil image to. Default is 0.
     * @return Returns true if the images were cleared successfully, false otherwise.
     */
    bool ClearAll(const Vector4f& color, float depth = 1.0f, i32 stencil = 0);

    /**
     * Binds a swap chain to the graphics pipeline. This will activate default frame buffer.
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
     * @param binding_index The binding index to bind the buffer to. Only relevant for constant and shader storage buffer. Default value is
     * -1.
     * @return Returns true if the buffer was bound successfully, false otherwise.
     */
    bool Bind(const Buffer& buffer, i32 binding_index = -1);

    /**
     * Binds an image to the graphics pipeline.
     * @param image The image to bind.
     * @param binding_index The binding index to bind the image to.
     * @return Returns true if the image was bound successfully, false otherwise.
     */
    bool Bind(const Image& image, i32 binding_index);

    /**
     * Binds one level of the image to the compute pipeline.
     * @param image The image to bind.
     * @param binding_index The binding index to bind the image to.
     * @param image_level The image level to bind.
     * @param access How the image will be accessed in the compute shader.
     * @return Returns true if the image was bound successfully, false otherwise.
     */
    bool BindImageForCompute(const Image& image, i32 binding_index, i32 image_level, ImageAccess access);

    /**
     * Binds a frame buffer to the graphics pipeline.
     * @param frame_buffer The frame buffer to bind.
     * @return Returns true if the frame buffer was bound successfully, false otherwise.
     */
    bool Bind(const FrameBuffer& frame_buffer);

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
     * @param wait_for_completion Whether or not to wait for the compute shader to finish executing before returning. Default is true.
     * @return Returns true if the dispatch was successful, false otherwise.
     */
    bool DispatchCompute(u32 block_count_x, u32 block_count_y, u32 block_count_z, bool wait_for_completion = true);

    /**
     * Updates the contents of a buffer.
     * @param buffer The buffer to update.
     * @param data The data to update the buffer with.
     * @param offset The offset into the buffer to update, measured in bytes.
     * @return In case of a success ErrorCode::Success is returned. If the buffer is not valid or it doesn't have Usage::Dynamic,
     * ErrorCode::InvalidArgument is returned. If the data is negative, zero or in combination with the offset exceeds the buffer size,
     * ErrorCode::OutOfBounds is returned. If offset is out of bounds of the buffer, ErrorCode::OutOfBounds is returned.
     */
    ErrorCode UpdateBuffer(const Buffer& buffer, const Opal::Span<const u8>& data, i64 offset = 0);

    /**
     * Reads the contents of a buffer.
     * @param buffer The buffer to read.
     * @param out_data Where to store read data.
     * @param offset From which byte to start reading. Default is 0. Offset should be between 0 and buffer size.
     * @param size How many bytes to read. If 0, reads the whole buffer. Default is 0. Size should be between 0 and buffer size.
     * @return Returns ErrorCode::Success if the read was successful. Returns ErrorCode::InvalidArgument if the buffer is not valid or the
     * buffer's usage is not ReadBack. Returns ErrorCode::OutOfBounds if the offset or size is out of bounds of the buffer.
     */
    ErrorCode ReadBuffer(const Buffer& buffer, Opal::Span<u8>& out_data, i32 offset = 0, i32 size = 0) const;

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
    bool Read(const Image& image, Bitmap& out_data, i32 level = 0) const;

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

private:
    GraphicsContextDesc m_desc;
    NativeDeviceContextHandle m_native_device_context = k_invalid_device_context_handle;
    NativeGraphicsContextHandle m_native_graphics_context = k_invalid_graphics_context_handle;

    Opal::Ref<const Pipeline> m_bound_pipeline;
};

}  // namespace Rndr
