#include "rndr/core/platform/opengl-command-list.h"

#include <glad/glad.h>

Rndr::DrawVerticesMultiCommand::DrawVerticesMultiCommand(PrimitiveTopology primitive_topology, uint32_t buffer_handle, uint32_t draw_count)
    : primitive_topology(primitive_topology), buffer_handle(buffer_handle), draw_count(draw_count)
{
}

Rndr::DrawVerticesMultiCommand::~DrawVerticesMultiCommand()
{
    glDeleteBuffers(1, &buffer_handle);
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
    buffer_handle = other.buffer_handle;
    draw_count = other.draw_count;
    other.buffer_handle = 0;
    other.draw_count = 0;
    return *this;
}
