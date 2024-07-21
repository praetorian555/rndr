#pragma once

#include <variant>

#include "opal/container/array.h"
#include "opal/container/ref.h"

#include "rndr/core/definitions.h"
#include "rndr/core/graphics-types.h"

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

struct BindSwapChainCommand
{
    Opal::Ref<const class SwapChain> swap_chain;
};

struct BindPipelineCommand
{
    Opal::Ref<const class Pipeline> pipeline;
};

struct BindConstantBufferCommand
{
    Opal::Ref<const class Buffer> constant_buffer;
    int32_t binding_index;
};

struct BindImageCommand
{
    Opal::Ref<const class Image> image;
    int32_t binding_index;
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
    Opal::Span<const u8> data;
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

using Command = std::variant<PresentCommand, ClearColorCommand, ClearDepthCommand, ClearStencilCommand, ClearAllCommand,
                             BindSwapChainCommand, BindPipelineCommand, BindConstantBufferCommand, BindImageCommand, DrawVerticesCommand,
                             DrawIndicesCommand, DrawVerticesMultiCommand, DrawIndicesMultiCommand, UpdateBufferCommand>;

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
    bool ClearStencil(int32_t stencil);

    /**
     * Clears the color and depth images in the bound frame buffer.
     * @param color Color to clear the color image to.
     * @param depth Depth value to clear the depth image to. Default is 1.
     * @param stencil Stencil value to clear the stencil image to. Default is 0.
     * @return Returns true if the images were cleared successfully, false otherwise.
     */
    bool ClearAll(const Vector4f& color, float depth = 1.0f, int32_t stencil = 0);

    /**
     * Binds a swap chain to the graphics pipeline.
     * @param swap_chain The swap chain to bind.
     * @return Returns true if the swap chain was bound successfully, false otherwise.
     */
    void Bind(const SwapChain& swap_chain);

    /**
     * Binds a pipeline object to the graphics pipeline.
     * @param pipeline The pipeline to bind.
     * @return Returns true if the pipeline was bound successfully, false otherwise.
     */
    void Bind(const Pipeline& pipeline);

    /**
     * Binds a constant buffer to the graphics pipeline on a specified slot.
     * @param buffer The constant buffer to bind.
     * @param binding_index The binding index to bind the buffer to.
     * @return Returns true if the buffer was bound successfully, false otherwise.
     */
    void BindConstantBuffer(const Buffer& buffer, int32_t binding_index);

    /**
     * Binds an image to the graphics pipeline.
     * @param image The image to bind.
     * @param binding_index The binding index to bind the image to.
     * @return Returns true if the image was bound successfully, false otherwise.
     */
    void Bind(const Image& image, int32_t binding_index);

    /**
     * Binds one level of the image to the compute pipeline.
     * @param image The image to bind.
     * @param binding_index The binding index to bind the image to.
     * @param image_level The image level to bind.
     * @param access How the image will be accessed in the compute shader.
     * @return Returns true if the image was bound successfully, false otherwise.
     */
    bool BindImageForCompute(const Image& image, int32_t binding_index, int32_t image_level, ImageAccess access);

    /**
     * Updates the contents of a buffer.
     * @param buffer The buffer to update.
     * @param data The data to update the buffer with.
     * @param offset The offset into the buffer to update.
     * @return Returns true if the buffer was updated successfully, false otherwise.
     */
    bool UpdateBuffer(const Buffer& buffer, const Opal::Span<const u8>& data, i32 offset = 0);

    /**
     * Draws primitives without use of index buffer. It will behave as if indices were specified
     * sequentially starting from 0.
     * @param topology The primitive topology to draw.
     * @param vertex_count The number of vertices to draw.
     * @param instance_count The number of instances to draw. By default this is 1.
     * @param first_vertex The index of the first vertex to draw. By default this is 0.
     * @return Returns true if the draw call was successful, false otherwise.
     */
    void DrawVertices(PrimitiveTopology topology, int32_t vertex_count, int32_t instance_count = 1, int32_t first_vertex = 0);

    /**
     * Draws primitives using an index buffer.
     * @param topology The primitive topology to draw.
     * @param index_count The number of indices to draw.
     * @param instance_count The number of instances to draw. By default this is 1.
     * @param first_index The index of the first index to draw. By default this is 0.
     * @return Returns true if the draw call was successful, false otherwise.
     */
    void DrawIndices(PrimitiveTopology topology, int32_t index_count, int32_t instance_count = 1, int32_t first_index = 0);

    /**
     * Issue multiple draw vertices calls.
     * @param pipeline The pipeline to use.
     * @param topology The primitive topology to draw.
     * @param draws The draw vertices data.
     */
    void DrawVerticesMulti(const Pipeline& pipeline, PrimitiveTopology topology, const Opal::Span<DrawVerticesData>& draws);

    /**
     * Issue multiple draw indices calls.
     * @param pipeline The pipeline to use.
     * @param topology The primitive topology to draw.
     * @param draws The draw indices data.
     */
    void DrawIndicesMulti(const Pipeline& pipeline, PrimitiveTopology topology, const Opal::Span<DrawIndicesData>& draws);

    /**
     * Dispatches a compute shader.
     * @param block_count_x Number of blocks in the x dimension.
     * @param block_count_y Number of blocks in the y dimension.
     * @param block_count_z Number of blocks in the z dimension.
     * @param wait_for_completion Whether or not to wait for the compute shader to finish executing before returning. Default is true.
     * @return Returns true if the dispatch was successful, false otherwise.
     */
    bool DispatchCompute(uint32_t block_count_x, uint32_t block_count_y, uint32_t block_count_z, bool wait_for_completion = true);

    /**
     * Submits the command list to the GPU.
     */
    void Submit();

private:
    Opal::Ref<GraphicsContext> m_graphics_context;
    Opal::Array<Command> m_commands;
};

}  // namespace Rndr

#endif  // RNDR_OPENGL