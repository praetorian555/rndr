#include "rndr/platform/opengl-command-list.hpp"

#include <string>

#include "glad/glad.h"

#include "opengl-helpers.hpp"
#include "rndr/log.hpp"
#include "rndr/platform/opengl-graphics-context.hpp"
#include "rndr/trace.hpp"

Rndr::DrawVerticesMultiCommand::DrawVerticesMultiCommand(Opal::Ref<Buffer> commands_buffer, PrimitiveTopology primitive_topology,
                                                         Opal::ArrayView<DrawVerticesData> draws)
    : commands_buffer(commands_buffer), primitive_topology(primitive_topology)
{
    for (const DrawVerticesData& draw : draws)
    {
        draw_commands.PushBack(draw);
    }
}

Rndr::DrawIndicesMultiCommand::DrawIndicesMultiCommand(Opal::Ref<Buffer> commands_buffer, PrimitiveTopology primitive_topology,
                                                       Opal::ArrayView<DrawIndicesData> draws)
    : commands_buffer(commands_buffer), primitive_topology(primitive_topology)
{
    for (const DrawIndicesData& draw : draws)
    {
        draw_commands.PushBack(draw);
    }
}

Rndr::CommandList::CommandList(Rndr::GraphicsContext& graphics_context) : m_graphics_context(graphics_context) {}

bool Rndr::CommandList::CmdPresent(const SwapChain& swap_chain)
{
    m_commands.PushBack(PresentCommand{.swap_chain = Opal::Ref<const SwapChain>(swap_chain)});
    return true;
}

bool Rndr::CommandList::CmdClearColor(const Vector4f& color)
{
    m_commands.PushBack(ClearColorCommand{.color = color});
    return true;
}

bool Rndr::CommandList::CmdClearDepth(float depth)
{
    m_commands.PushBack(ClearDepthCommand{.depth = depth});
    return true;
}

bool Rndr::CommandList::CmdClearStencil(int32_t stencil)
{
    m_commands.PushBack(ClearStencilCommand{.stencil = stencil});
    return true;
}

bool Rndr::CommandList::CmdClearAll(const Rndr::Vector4f& color, float depth, int32_t stencil)
{
    m_commands.PushBack(ClearAllCommand{.color = color, .depth = depth, .stencil = stencil});
    return true;
}

void Rndr::CommandList::CmdBindSwapChainFrameBuffer(const Rndr::SwapChain& swap_chain)
{
    m_commands.PushBack(BindSwapChainCommand{.swap_chain = Opal::Ref<const SwapChain>(swap_chain)});
}

void Rndr::CommandList::CmdBindFrameBuffer(const class Rndr::FrameBuffer& frame_buffer)
{
    m_commands.PushBack(BindFrameBufferCommand{.frame_buffer = Opal::Ref<const FrameBuffer>(frame_buffer)});
}

void Rndr::CommandList::CmdBindPipeline(const Rndr::Pipeline& pipeline)
{
    m_commands.PushBack(BindPipelineCommand{.pipeline = Opal::Ref<const Pipeline>(pipeline)});
}

void Rndr::CommandList::CmdBindBuffer(const Rndr::Buffer& buffer, int32_t binding_index)
{
    m_commands.PushBack(BindBufferCommand{.buffer = Opal::Ref<const Buffer>(buffer), .binding_index = binding_index});
}

void Rndr::CommandList::CmdBindTexture(const Rndr::Texture& texture, int32_t binding_index)
{
    m_commands.PushBack(BindTextureCommand{.texture = Opal::Ref<const Texture>(texture), .binding_index = binding_index});
}

void Rndr::CommandList::CmdBindTextureForCompute(const Rndr::Texture& texture, int32_t binding_index, int32_t texture_level,
                                                 Rndr::TextureAccess access)
{
    m_commands.PushBack(BindTextureForComputeCommand{
        .texture = Opal::Ref<const Texture>(texture), .binding_index = binding_index, .texture_level = texture_level, .access = access});
}

void Rndr::CommandList::CmdDrawVertices(Rndr::PrimitiveTopology topology, int32_t vertex_count, int32_t instance_count,
                                        int32_t first_vertex)
{
    m_commands.PushBack(DrawVerticesCommand{
        .primitive_topology = topology, .vertex_count = vertex_count, .instance_count = instance_count, .first_vertex = first_vertex});
}

void Rndr::CommandList::CmdDrawIndices(Rndr::PrimitiveTopology topology, int32_t index_count, int32_t instance_count, int32_t first_index)
{
    m_commands.PushBack(DrawIndicesCommand{
        .primitive_topology = topology, .index_count = index_count, .instance_count = instance_count, .first_index = first_index});
}

void Rndr::CommandList::CmdDrawVerticesMulti(Opal::Ref<Buffer> commands_buffer, PrimitiveTopology topology,
                                             const Opal::ArrayView<DrawVerticesData>& draws)
{
    static_assert(sizeof(DrawVerticesData::vertex_count) == 4);
    static_assert(sizeof(DrawVerticesData::instance_count) == 4);
    static_assert(sizeof(DrawVerticesData::first_vertex) == 4);
    static_assert(sizeof(DrawVerticesData::base_instance) == 4);
    static_assert(sizeof(DrawVerticesData) == 16, "DrawVerticesData size is not 16 bytes");

    m_commands.PushBack(DrawVerticesMultiCommand(commands_buffer, topology, draws));
}

void Rndr::CommandList::CmdDrawIndicesMulti(Opal::Ref<Buffer> commands_buffer, PrimitiveTopology topology,
                                            const Opal::ArrayView<DrawIndicesData>& draws)
{
    static_assert(sizeof(DrawIndicesData::index_count) == 4);
    static_assert(sizeof(DrawIndicesData::instance_count) == 4);
    static_assert(sizeof(DrawIndicesData::first_index) == 4);
    static_assert(sizeof(DrawIndicesData::base_vertex) == 4);
    static_assert(sizeof(DrawIndicesData::base_instance) == 4);
    static_assert(sizeof(DrawIndicesData) == 20, "DrawIndicesData size is not 20 bytes");

    m_commands.PushBack(DrawIndicesMultiCommand(commands_buffer, topology, draws));
}

bool Rndr::CommandList::CmdDispatchCompute(uint32_t block_count_x, uint32_t block_count_y, uint32_t block_count_z, bool wait_for_completion)
{
    m_commands.PushBack(DispatchComputeCommand{.block_count_x = block_count_x,
                                               .block_count_y = block_count_y,
                                               .block_count_z = block_count_z,
                                               .wait_for_completion = wait_for_completion});
    return true;
}

bool Rndr::CommandList::CmdUpdateBuffer(const Rndr::Buffer& buffer, const Opal::ArrayView<const u8>& data, Rndr::i32 offset)
{
    m_commands.PushBack(
        UpdateBufferCommand{.buffer = Opal::Ref<const Buffer>(buffer), .data = {data.GetData(), data.GetSize()}, .offset = offset});
    return true;
}

void Rndr::CommandList::CmdBlitFrameBuffers(const FrameBuffer& dst, const FrameBuffer& src, const BlitFrameBufferDesc& desc)
{
    m_commands.PushBack(BlitFrameBuffersCommand{.src_frame_buffer = Opal::Ref(src), .dst_frame_buffer = Opal::Ref(dst), .blit_desc = desc});
}

void Rndr::CommandList::CmdBlitToSwapChain(const SwapChain& swap_chain, const FrameBuffer& src, const BlitFrameBufferDesc& desc)
{
    m_commands.PushBack(BlitToSwapChainCommand{.swap_chain = Opal::Ref(swap_chain), .src_frame_buffer = Opal::Ref(src), .blit_desc = desc});
}

void Rndr::CommandList::CmdPushMarker(Opal::StringUtf8 marker)
{
    m_commands.PushBack(PushMarkerCommand{.marker = std::move(marker)});
}

void Rndr::CommandList::CmdPopMarker()
{
    m_commands.PushBack(PopMarkerCommand{});
}

struct CommandExecutor
{
    Opal::Ref<Rndr::GraphicsContext> graphics_context;

    void operator()(const Rndr::PresentCommand& command) const { graphics_context->Present(command.swap_chain); }

    void operator()(const Rndr::ClearColorCommand& command) const { graphics_context->ClearColor(command.color); }

    void operator()(const Rndr::ClearDepthCommand& command) const { graphics_context->ClearDepth(command.depth); }

    void operator()(const Rndr::ClearStencilCommand& command) const { graphics_context->ClearStencil(command.stencil); }

    void operator()(const Rndr::ClearAllCommand& command) const
    {
        graphics_context->ClearAll(command.color, command.depth, command.stencil);
    }

    void operator()(const Rndr::BindSwapChainCommand& command) const { graphics_context->BindSwapChainFrameBuffer(command.swap_chain); }

    void operator()(const Rndr::BindFrameBufferCommand& command) const { graphics_context->BindFrameBuffer(command.frame_buffer); }

    void operator()(const Rndr::BindPipelineCommand& command) const { graphics_context->BindPipeline(command.pipeline); }

    void operator()(const Rndr::BindBufferCommand& command) const { graphics_context->BindBuffer(command.buffer, command.binding_index); }

    void operator()(const Rndr::BindTextureCommand& command) const
    {
        graphics_context->BindTexture(command.texture, command.binding_index);
    }

    void operator()(const Rndr::BindTextureForComputeCommand& command) const
    {
        graphics_context->BindTextureForCompute(command.texture, command.binding_index, command.texture_level, command.access);
    }

    void operator()(const Rndr::DrawVerticesCommand& command) const
    {
        graphics_context->DrawVertices(command.primitive_topology, command.vertex_count, command.instance_count, command.first_vertex);
    }

    void operator()(const Rndr::DrawIndicesCommand& command) const
    {
        graphics_context->DrawIndices(command.primitive_topology, command.index_count, command.instance_count, command.first_index);
    }

    void operator()(const Rndr::DrawVerticesMultiCommand& command) const
    {
        const GLuint native_buffer = command.commands_buffer->GetNativeBuffer();
        glNamedBufferSubData(native_buffer, 0, command.draw_commands.GetSize() * sizeof(Rndr::DrawVerticesData),
                             command.draw_commands.GetData());
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, native_buffer);
        RNDR_ASSERT_OPENGL();
        const GLenum topology = FromPrimitiveTopologyToOpenGL(command.primitive_topology);
        glMultiDrawArraysIndirect(topology, nullptr, static_cast<int32_t>(command.draw_commands.GetSize()), sizeof(Rndr::DrawVerticesData));
        RNDR_ASSERT_OPENGL();
    }

    void operator()(const Rndr::DrawIndicesMultiCommand& command) const
    {
        const GLuint native_buffer = command.commands_buffer->GetNativeBuffer();
        glNamedBufferSubData(native_buffer, 0, command.draw_commands.GetSize() * sizeof(Rndr::DrawIndicesData),
                             command.draw_commands.GetData());
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, command.commands_buffer->GetNativeBuffer());
        RNDR_ASSERT_OPENGL();
        const GLenum topology = FromPrimitiveTopologyToOpenGL(command.primitive_topology);
        glMultiDrawElementsIndirect(topology, GL_UNSIGNED_INT, nullptr, static_cast<int32_t>(command.draw_commands.GetSize()),
                                    sizeof(Rndr::DrawIndicesData));
        RNDR_ASSERT_OPENGL();
    }

    void operator()(const Rndr::UpdateBufferCommand& command) const
    {
        graphics_context->UpdateBuffer(command.buffer, Opal::AsBytes(command.data), command.offset);
    }

    void operator()(const Rndr::DispatchComputeCommand& command) const
    {
        graphics_context->DispatchCompute(command.block_count_x, command.block_count_y, command.block_count_z, command.wait_for_completion);
    }

    void operator()(const Rndr::BlitFrameBuffersCommand& command) const
    {
        graphics_context->BlitFrameBuffers(command.dst_frame_buffer, command.src_frame_buffer, command.blit_desc);
    }

    void operator()(const Rndr::BlitToSwapChainCommand& command) const
    {
        graphics_context->BlitToSwapChain(command.swap_chain, command.src_frame_buffer, command.blit_desc);
    }

    void operator()(const Rndr::PushMarkerCommand& command) const
    {
        RNDR_ASSERT(!command.marker.IsEmpty(), "Marker can't be empty!");
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, *command.marker);
    }

    void operator()(const Rndr::PopMarkerCommand&) const
    {
        glPopDebugGroup();
    }
};

void Rndr::CommandList::Execute()
{
    RNDR_CPU_EVENT_SCOPED("Submit Command List");

    for (const auto& command : m_commands)
    {
        std::visit(CommandExecutor{m_graphics_context}, command);
    }
}

void Rndr::CommandList::Reset()
{
    m_commands.Clear();
}
