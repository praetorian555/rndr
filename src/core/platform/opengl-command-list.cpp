#include "rndr/core/platform/opengl-command-list.h"

#include <glad/glad.h>

#include "core/platform/opengl-helpers.h"
#include "rndr/core/platform/opengl-graphics-context.h"
#include "rndr/utility/cpu-tracer.h"

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

bool Rndr::CommandList::Present(const SwapChain& swap_chain)
{
    m_commands.PushBack(PresentCommand{.swap_chain = Opal::Ref<const SwapChain>(swap_chain)});
    return true;
}

bool Rndr::CommandList::ClearColor(const Vector4f& color)
{
    m_commands.PushBack(ClearColorCommand{.color = color});
    return true;
}

bool Rndr::CommandList::ClearDepth(float depth)
{
    m_commands.PushBack(ClearDepthCommand{.depth = depth});
    return true;
}

bool Rndr::CommandList::ClearStencil(int32_t stencil)
{
    m_commands.PushBack(ClearStencilCommand{.stencil = stencil});
    return true;
}

bool Rndr::CommandList::ClearAll(const Rndr::Vector4f& color, float depth, int32_t stencil)
{
    m_commands.PushBack(ClearAllCommand{.color = color, .depth = depth, .stencil = stencil});
    return true;
}

void Rndr::CommandList::Bind(const Rndr::SwapChain& swap_chain)
{
    m_commands.PushBack(BindSwapChainCommand{.swap_chain = Opal::Ref<const SwapChain>(swap_chain)});
}

void Rndr::CommandList::Bind(const Rndr::Pipeline& pipeline)
{
    m_commands.PushBack(BindPipelineCommand{.pipeline = Opal::Ref<const Pipeline>(pipeline)});
}

void Rndr::CommandList::BindConstantBuffer(const Rndr::Buffer& buffer, int32_t binding_index)
{
    m_commands.PushBack(BindConstantBufferCommand{.constant_buffer = Opal::Ref<const Buffer>(buffer), .binding_index = binding_index});
}

void Rndr::CommandList::Bind(const Rndr::Image& image, int32_t binding_index)
{
    m_commands.PushBack(BindImageCommand{.image = Opal::Ref<const Image>(image), .binding_index = binding_index});
}

void Rndr::CommandList::DrawVertices(Rndr::PrimitiveTopology topology, int32_t vertex_count, int32_t instance_count, int32_t first_vertex)
{
    m_commands.PushBack(DrawVerticesCommand{
        .primitive_topology = topology, .vertex_count = vertex_count, .instance_count = instance_count, .first_vertex = first_vertex});
}

void Rndr::CommandList::DrawIndices(Rndr::PrimitiveTopology topology, int32_t index_count, int32_t instance_count, int32_t first_index)
{
    m_commands.PushBack(DrawIndicesCommand{
        .primitive_topology = topology, .index_count = index_count, .instance_count = instance_count, .first_index = first_index});
}

void Rndr::CommandList::DrawVerticesMulti(const Rndr::Pipeline& pipeline, Rndr::PrimitiveTopology topology,
                                          const Opal::Span<Rndr::DrawVerticesData>& draws)
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

    Bind(pipeline);
    m_commands.PushBack(DrawVerticesMultiCommand(topology, buffer_handle, static_cast<uint32_t>(draws.GetSize())));
}

void Rndr::CommandList::DrawIndicesMulti(const Rndr::Pipeline& pipeline, Rndr::PrimitiveTopology topology,
                                         const Opal::Span<Rndr::DrawIndicesData>& draws)
{
    static_assert(sizeof(DrawIndicesData::index_count) == 4);
    static_assert(sizeof(DrawIndicesData::instance_count) == 4);
    static_assert(sizeof(DrawIndicesData::first_index) == 4);
    static_assert(sizeof(DrawIndicesData::base_vertex) == 4);
    static_assert(sizeof(DrawIndicesData::base_instance) == 4);
    static_assert(sizeof(DrawIndicesData) == 20, "DrawIndicesData size is not 20 bytes");

    m_graphics_context->Bind(pipeline);

    GLuint buffer_handle = 0;
    glCreateBuffers(1, &buffer_handle);
    RNDR_ASSERT_OPENGL();
    glNamedBufferStorage(buffer_handle, draws.GetSize() * sizeof(DrawIndicesData), draws.GetData(), GL_DYNAMIC_STORAGE_BIT);
    RNDR_ASSERT_OPENGL();
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer_handle);
    RNDR_ASSERT_OPENGL();

    Bind(pipeline);
    m_commands.PushBack(DrawIndicesMultiCommand(topology, buffer_handle, static_cast<uint32_t>(draws.GetSize())));
}

bool Rndr::CommandList::UpdateBuffer(const Rndr::Buffer& buffer, const Opal::Span<const u8>& data, Rndr::i32 offset)
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

    void operator()(const Rndr::BindSwapChainCommand& command) const { graphics_context->Bind(command.swap_chain); }

    void operator()(const Rndr::BindPipelineCommand& command) const { graphics_context->Bind(command.pipeline); }

    void operator()(const Rndr::BindConstantBufferCommand& command) const
    {
        graphics_context->Bind(command.constant_buffer, command.binding_index);
    }

    void operator()(const Rndr::BindImageCommand& command) const { graphics_context->Bind(command.image, command.binding_index); }

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
};

void Rndr::CommandList::Submit()
{
    RNDR_TRACE_SCOPED(Submit Command List);

    for (const auto& command : m_commands)
    {
        std::visit(CommandExecutor{m_graphics_context}, command);
    }
}
