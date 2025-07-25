#pragma once

#include <variant>

#include "opal/container/dynamic-array.h"
#include "opal/container/ref.h"

#include "rndr/definitions.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/error-codes.hpp"

#if RNDR_OPENGL

namespace Rndr
{

class GraphicsContext;

struct PresentCommand
{
    Opal::Ref<const class SwapChain> swap_chain;
};

struct ClearColorCommand
{
    Vector4f color;
};

struct ClearDepthCommand
{
    float depth;
};

struct ClearStencilCommand
{
    int32_t stencil;
};

struct ClearAllCommand
{
    Vector4f color;
    float depth;
    int32_t stencil;
};

struct BindPipelineCommand
{
    Opal::Ref<const class Pipeline> pipeline;
};

struct BindBufferCommand
{
    Opal::Ref<const class Buffer> buffer;
    int32_t binding_index;
};

struct BindTextureCommand
{
    Opal::Ref<const class Texture> texture;
    int32_t binding_index;
};

struct BindTextureForComputeCommand
{
    Opal::Ref<const class Texture> texture;
    int32_t binding_index;
    int32_t texture_level;
    TextureAccess access;
};

struct BindSwapChainCommand
{
    Opal::Ref<const class SwapChain> swap_chain;
};

struct BindFrameBufferCommand
{
    Opal::Ref<const class FrameBuffer> frame_buffer;
};

struct DrawVerticesCommand
{
    PrimitiveTopology primitive_topology;
    int32_t vertex_count;
    int32_t instance_count;
    int32_t first_vertex;
};

struct DrawIndicesCommand
{
    PrimitiveTopology primitive_topology;
    int32_t index_count;
    int32_t instance_count;
    int32_t first_index;
};

struct DrawVerticesMultiCommand
{
    PrimitiveTopology primitive_topology;
    uint32_t buffer_handle;
    uint32_t draw_count;

    DrawVerticesMultiCommand() = default;
    DrawVerticesMultiCommand(PrimitiveTopology primitive_topology, uint32_t buffer_handle, uint32_t draw_count);
    ~DrawVerticesMultiCommand();

    DrawVerticesMultiCommand(DrawVerticesMultiCommand& other) = delete;
    DrawVerticesMultiCommand& operator=(DrawVerticesMultiCommand& other) = delete;

    DrawVerticesMultiCommand(DrawVerticesMultiCommand&& other) noexcept;
    DrawVerticesMultiCommand& operator=(DrawVerticesMultiCommand&& other) noexcept;
};

struct UpdateBufferCommand
{
    Opal::Ref<const class Buffer> buffer;
    Opal::ArrayView<const u8> data;
    i32 offset;
};

struct DrawIndicesMultiCommand
{
    PrimitiveTopology primitive_topology;
    uint32_t buffer_handle;
    uint32_t draw_count;

    DrawIndicesMultiCommand() = default;
    DrawIndicesMultiCommand(PrimitiveTopology primitive_topology, uint32_t buffer_handle, uint32_t draw_count);
    ~DrawIndicesMultiCommand();

    DrawIndicesMultiCommand(DrawIndicesMultiCommand& other) = delete;
    DrawIndicesMultiCommand& operator=(DrawIndicesMultiCommand& other) = delete;

    DrawIndicesMultiCommand(DrawIndicesMultiCommand&& other) noexcept;
    DrawIndicesMultiCommand& operator=(DrawIndicesMultiCommand&& other) noexcept;
};

struct DispatchComputeCommand
{
    uint32_t block_count_x;
    uint32_t block_count_y;
    uint32_t block_count_z;
    bool wait_for_completion;
};

struct BlitFrameBuffersCommand
{
    Opal::Ref<const class FrameBuffer> src_frame_buffer;
    Opal::Ref<const class FrameBuffer> dst_frame_buffer;
    BlitFrameBufferDesc blit_desc;
};

struct BlitToSwapChainCommand
{
    Opal::Ref<const class SwapChain> swap_chain;
    Opal::Ref<const class FrameBuffer> src_frame_buffer;
    BlitFrameBufferDesc blit_desc;
};

using Command =
    std::variant<PresentCommand, ClearColorCommand, ClearDepthCommand, ClearStencilCommand, ClearAllCommand, BindSwapChainCommand,
                 BindPipelineCommand, BindBufferCommand, BindTextureCommand, BindTextureForComputeCommand, BindFrameBufferCommand,
                 DrawVerticesCommand, DrawIndicesCommand, DrawVerticesMultiCommand, DrawIndicesMultiCommand, DispatchComputeCommand,
                 UpdateBufferCommand, BlitFrameBuffersCommand, BlitToSwapChainCommand>;

/**
 * Represents a list of commands to be executed on the GPU.
 */
class CommandList
{
public:
    CommandList() = default;
    explicit CommandList(GraphicsContext& graphics_context);
    ~CommandList() = default;

    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;
    CommandList(CommandList&& other) noexcept = default;
    CommandList& operator=(CommandList&& other) noexcept = default;

    /**
     * Swaps the front and back buffers of the swap chain.
     * @param swap_chain The swap chain to present.
     * @return Returns true if the swap chain was presented successfully, false otherwise.
     */
    bool CmdPresent(const SwapChain& swap_chain);

    /**
     * Clears the color texture in the bound frame buffer.
     * @param color The color to clear the texture to.
     * @return Returns true if the texture was cleared successfully, false otherwise.
     */
    bool CmdClearColor(const Vector4f& color);

    /**
     * Clears the depth texture in the bound frame buffer.
     * @param depth The depth value to clear the texture to.
     * @return Returns true if the texture was cleared successfully, false otherwise.
     */
    bool CmdClearDepth(float depth);

    /**
     * Clears the stencil texture in the bound frame buffer.
     * @param stencil The stencil value to clear the texture to.
     * @return Returns true if the texture was cleared successfully, false otherwise.
     */
    bool CmdClearStencil(int32_t stencil);

    /**
     * Clears the color and depth textures in the bound frame buffer.
     * @param color Color to clear the color texture to.
     * @param depth Depth value to clear the depth texture to. Default is 1.
     * @param stencil Stencil value to clear the stencil texture to. Default is 0.
     * @return Returns true if the textures were cleared successfully, false otherwise.
     */
    bool CmdClearAll(const Vector4f& color, float depth = 1.0f, int32_t stencil = 0);

    /**
     * Binds a pipeline object to the graphics pipeline.
     * @param pipeline The pipeline to bind.
     */
    void CmdBindPipeline(const Pipeline& pipeline);

    /**
     * Binds a constant buffer to the graphics pipeline on a specified slot.
     * @param buffer The constant buffer to bind.
     * @param binding_index The binding index to bind the buffer to.
     */
    void CmdBindBuffer(const Buffer& buffer, int32_t binding_index);

    /**
     * Binds an texture to the graphics pipeline.
     * @param texture The texture to bind.
     * @param binding_index The binding index to bind the texture to.
     */
    void CmdBindTexture(const Texture& texture, int32_t binding_index);

    /**
     * Binds one level of the texture to the compute pipeline.
     * @param texture The texture to bind.
     * @param binding_index The binding index to bind the texture to.
     * @param texture_level The texture level to bind.
     * @param access How the texture will be accessed in the compute shader.
     */
    void CmdBindTextureForCompute(const Texture& texture, int32_t binding_index, int32_t texture_level, TextureAccess access);

    /**
     * Binds a buffer to the graphics pipeline.
     * @param frame_buffer The frame buffer to bind.
     */
    void CmdBindFrameBuffer(const class FrameBuffer& frame_buffer);

    /**
     * Binds a swap chain to the graphics pipeline.
     * @param swap_chain The swap chain to bind.
     * @return Returns true if the swap chain was bound successfully, false otherwise.
     */
    void CmdBindSwapChainFrameBuffer(const SwapChain& swap_chain);

    /**
     * Updates the contents of a buffer.
     * @param buffer The buffer to update.
     * @param data The data to update the buffer with.
     * @param offset The offset into the buffer to update.
     * @return Returns true if the buffer was updated successfully, false otherwise.
     */
    bool CmdUpdateBuffer(const Buffer& buffer, const Opal::ArrayView<const u8>& data, i32 offset = 0);

    /**
     * Draws primitives without use of index buffer. It will behave as if indices were specified
     * sequentially starting from 0.
     * @param topology The primitive topology to draw.
     * @param vertex_count The number of vertices to draw.
     * @param instance_count The number of instances to draw. By default this is 1.
     * @param first_vertex The index of the first vertex to draw. By default this is 0.
     * @return Returns true if the draw call was successful, false otherwise.
     */
    void CmdDrawVertices(PrimitiveTopology topology, int32_t vertex_count, int32_t instance_count = 1, int32_t first_vertex = 0);

    /**
     * Draws primitives using an index buffer.
     * @param topology The primitive topology to draw.
     * @param index_count The number of indices to draw.
     * @param instance_count The number of instances to draw. By default this is 1.
     * @param first_index The index of the first index to draw. By default this is 0.
     * @return Returns true if the draw call was successful, false otherwise.
     */
    void CmdDrawIndices(PrimitiveTopology topology, int32_t index_count, int32_t instance_count = 1, int32_t first_index = 0);

    /**
     * Issue multiple draw vertices calls.
     * @param pipeline The pipeline to use.
     * @param topology The primitive topology to draw.
     * @param draws The draw vertices data.
     */
    void CmdDrawVerticesMulti(const Pipeline& pipeline, PrimitiveTopology topology, const Opal::ArrayView<DrawVerticesData>& draws);

    /**
     * Issue multiple draw indices calls.
     * @param pipeline The pipeline to use.
     * @param topology The primitive topology to draw.
     * @param draws The draw indices data.
     */
    void CmdDrawIndicesMulti(const Pipeline& pipeline, PrimitiveTopology topology, const Opal::ArrayView<DrawIndicesData>& draws);

    /**
     * Dispatches a compute shader.
     * @param block_count_x Number of blocks in the x dimension.
     * @param block_count_y Number of blocks in the y dimension.
     * @param block_count_z Number of blocks in the z dimension.
     * @param wait_for_completion Whether to wait for the compute shader to finish executing before returning. Default is true.
     * @return Returns true if the dispatch was successful, false otherwise.
     */
    bool CmdDispatchCompute(uint32_t block_count_x, uint32_t block_count_y, uint32_t block_count_z, bool wait_for_completion = true);

    /**
     * Copy attachments from the 'src' frame buffer to the 'dst' frame buffer.
     * @param dst Destination frame buffer. Does not need to be bound beforehand.
     * @param src Source frame buffer. Does not need to be bound beforehand.
     * @param desc Describes the rules for copying.
     */
    void CmdBlitFrameBuffers(const FrameBuffer& dst, const FrameBuffer& src, const BlitFrameBufferDesc& desc);

    /**
     * Copy attachments from the 'src' frame buffer to the swap chain's frame buffer.
     * @param swap_chain The swap chain to copy to.
     * @param src Source frame buffer. Does not need to be bound beforehand.
     * @param desc Describes the rules for copying.
     */
    void CmdBlitToSwapChain(const SwapChain& swap_chain, const FrameBuffer& src, const BlitFrameBufferDesc& desc);

    /**
     * Submits the command list to the GPU.
     */
    void Execute();

    /**
     * Clear command queue.
     */
    void Reset();

private:
    Opal::Ref<GraphicsContext> m_graphics_context;
    Opal::DynamicArray<Command> m_commands;
};

}  // namespace Rndr

#endif  // RNDR_OPENGL