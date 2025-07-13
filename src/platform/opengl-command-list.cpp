#include "rndr/platform/opengl-command-list.hpp"

#include "glad/glad.h"

#include "opengl-helpers.hpp"
#include "rndr/platform/opengl-graphics-context.hpp"
#include "rndr/trace.hpp"

Rndr::DrawVerticesMultiCommand::DrawVerticesMultiCommand(PrimitiveTopology primitive_topology, uint32_t buffer_handle, uint32_t draw_count)
    : primitive_topology(primitive_topology), buffer_handle(buffer_handle), draw_count(draw_count)
{
}

Rndr::DrawVerticesMultiCommand::~DrawVerticesMultiCommand()
{
    glDeleteBuffers(1, &buffer_handle);
    RNDR_ASSERT_OPENGL();
}

Rndr::DrawVerticesMultiCommand::DrawVerticesMultiCommand(DrawVerticesMultiCommand&& other) noexcept
    : primitive_topology(other.primitive_topology), buffer_handle(other.buffer_handle), draw_count(other.draw_count)
{
    other.buffer_handle = 0;
    other.draw_count = 0;
}

Rndr::DrawVerticesMultiCommand& Rndr::DrawVerticesMultiCommand::operator=(DrawVerticesMultiCommand&& other) noexcept
{
    primitive_topology = other.primitive_topology;
    glDeleteBuffers(1, &buffer_handle);
    RNDR_ASSERT_OPENGL();
    buffer_handle = other.buffer_handle;
    draw_count = other.draw_count;
    other.buffer_handle = 0;
    other.draw_count = 0;
    return *this;
}

Rndr::DrawIndicesMultiCommand::DrawIndicesMultiCommand(Rndr::PrimitiveTopology primitive_topology, uint32_t buffer_handle,
                                                       uint32_t draw_count)
    : primitive_topology(primitive_topology), buffer_handle(buffer_handle), draw_count(draw_count)
{
}

Rndr::DrawIndicesMultiCommand::~DrawIndicesMultiCommand()
{
    glDeleteBuffers(1, &buffer_handle);
    RNDR_ASSERT_OPENGL();
}

Rndr::DrawIndicesMultiCommand::DrawIndicesMultiCommand(Rndr::DrawIndicesMultiCommand&& other) noexcept
    : primitive_topology(other.primitive_topology), buffer_handle(other.buffer_handle), draw_count(other.draw_count)
{
    other.buffer_handle = 0;
    other.draw_count = 0;
}

Rndr::DrawIndicesMultiCommand& Rndr::DrawIndicesMultiCommand::operator=(Rndr::DrawIndicesMultiCommand&& other) noexcept
{
    primitive_topology = other.primitive_topology;
    glDeleteBuffers(1, &buffer_handle);
    RNDR_ASSERT_OPENGL();
    buffer_handle = other.buffer_handle;
    draw_count = other.draw_count;
    other.buffer_handle = 0;
    other.draw_count = 0;
    return *this;
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

void Rndr::CommandList::CmdDrawVertices(Rndr::PrimitiveTopology topology, int32_t vertex_count, int32_t instance_count, int32_t first_vertex)
{
    m_commands.PushBack(DrawVerticesCommand{
        .primitive_topology = topology, .vertex_count = vertex_count, .instance_count = instance_count, .first_vertex = first_vertex});
}

void Rndr::CommandList::CmdDrawIndices(Rndr::PrimitiveTopology topology, int32_t index_count, int32_t instance_count, int32_t first_index)
{
    m_commands.PushBack(DrawIndicesCommand{
        .primitive_topology = topology, .index_count = index_count, .instance_count = instance_count, .first_index = first_index});
}

void Rndr::CommandList::CmdDrawVerticesMulti(const Rndr::Pipeline& pipeline, Rndr::PrimitiveTopology topology,
                                          const Opal::ArrayView<Rndr::DrawVerticesData>& draws)
{
    static_assert(sizeof(DrawVerticesData::vertex_count) == 4);
    static_assert(sizeof(DrawVerticesData::instance_count) == 4);
    static_assert(sizeof(DrawVerticesData::first_vertex) == 4);
    static_assert(sizeof(DrawVerticesData::base_instance) == 4);
    static_assert(sizeof(DrawVerticesData) == 16, "DrawVerticesData size is not 16 bytes");

    GLuint buffer_handle = 0;
    glCreateBuffers(1, &buffer_handle);
    RNDR_ASSERT_OPENGL();
    glNamedBufferStorage(buffer_handle, draws.GetSize() * sizeof(DrawVerticesData), draws.GetData(), GL_DYNAMIC_STORAGE_BIT);
    RNDR_ASSERT_OPENGL();

    CmdBindPipeline(pipeline);
    m_commands.PushBack(DrawVerticesMultiCommand(topology, buffer_handle, static_cast<uint32_t>(draws.GetSize())));
}

void Rndr::CommandList::CmdDrawIndicesMulti(const Rndr::Pipeline& pipeline, Rndr::PrimitiveTopology topology,
                                         const Opal::ArrayView<Rndr::DrawIndicesData>& draws)
{
    static_assert(sizeof(DrawIndicesData::index_count) == 4);
    static_assert(sizeof(DrawIndicesData::instance_count) == 4);
    static_assert(sizeof(DrawIndicesData::first_index) == 4);
    static_assert(sizeof(DrawIndicesData::base_vertex) == 4);
    static_assert(sizeof(DrawIndicesData::base_instance) == 4);
    static_assert(sizeof(DrawIndicesData) == 20, "DrawIndicesData size is not 20 bytes");

    m_graphics_context->BindPipeline(pipeline);

    GLuint buffer_handle = 0;
    glCreateBuffers(1, &buffer_handle);
    RNDR_ASSERT_OPENGL();
    glNamedBufferStorage(buffer_handle, draws.GetSize() * sizeof(DrawIndicesData), draws.GetData(), GL_DYNAMIC_STORAGE_BIT);
    RNDR_ASSERT_OPENGL();
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer_handle);
    RNDR_ASSERT_OPENGL();

    CmdBindPipeline(pipeline);
    m_commands.PushBack(DrawIndicesMultiCommand(topology, buffer_handle, static_cast<uint32_t>(draws.GetSize())));
}

bool Rndr::CommandList::CmdDispatchCompute(uint32_t block_count_x, uint32_t block_count_y, uint32_t block_count_z, bool wait_for_completion)
{
    m_commands.PushBack(DispatchComputeCommand{
        .block_count_x = block_count_x, .block_count_y = block_count_y, .block_count_z = block_count_z, .wait_for_completion = wait_for_completion});
    return true;
}

bool Rndr::CommandList::CmdUpdateBuffer(const Rndr::Buffer& buffer, const Opal::ArrayView<const u8>& data, Rndr::i32 offset)
{
    m_commands.PushBack(UpdateBufferCommand{.buffer = Opal::Ref<const Buffer>(buffer), .data = data, .offset = offset});
    return true;
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
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, command.buffer_handle);
        RNDR_ASSERT_OPENGL();
        const GLenum topology = FromPrimitiveTopologyToOpenGL(command.primitive_topology);
        glMultiDrawArraysIndirect(topology, nullptr, static_cast<int32_t>(command.draw_count), sizeof(Rndr::DrawVerticesData));
        RNDR_ASSERT_OPENGL();
    }

    void operator()(const Rndr::DrawIndicesMultiCommand& command) const
    {
        const GLenum topology = FromPrimitiveTopologyToOpenGL(command.primitive_topology);
        glMultiDrawElementsIndirect(topology, GL_UNSIGNED_INT, nullptr, static_cast<int32_t>(command.draw_count),
                                    sizeof(Rndr::DrawIndicesData));
        RNDR_ASSERT_OPENGL();
    }

    void operator()(const Rndr::UpdateBufferCommand& command) const
    {
        graphics_context->UpdateBuffer(command.buffer, command.data, command.offset);
    }

    void operator()(const Rndr::DispatchComputeCommand& command) const
    {
        graphics_context->DispatchCompute(command.block_count_x, command.block_count_y, command.block_count_z, command.wait_for_completion);
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
