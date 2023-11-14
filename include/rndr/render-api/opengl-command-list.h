#pragma once

#include <variant>

#include "rndr/core/containers/ref.h"
#include "rndr/core/definitions.h"
#include "rndr/core/graphics-types.h"

#if RNDR_OPENGL

namespace Rndr
{

struct PresentCommand
{
    Ref<const class SwapChain> swap_chain;
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
    Ref<const class SwapChain> swap_chain;
};

struct BindPipelineCommand
{
    Ref<const class Pipeline> pipeline;
};

struct BindConstantBufferCommand
{
    Ref<const class Buffer> constant_buffer;
    int32_t binding_index;
};

struct BindImageCommand
{
    Ref<const class Image> image;
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
                             DrawIndicesCommand, DrawVerticesMultiCommand, DrawIndicesMultiCommand>;

}  // namespace Rndr

#endif  // RNDR_OPENGL