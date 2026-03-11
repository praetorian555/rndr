#include "rndr/canvas/draw-list.hpp"

#include "glad/glad.h"

#include "rndr/canvas/brush.hpp"
#include "rndr/canvas/display.hpp"
#include "rndr/canvas/mesh.hpp"
#include "rndr/canvas/render-target.hpp"
#include "rndr/trace.hpp"

void Rndr::Canvas::DrawList::SetViewport(i32 x, i32 y, i32 width, i32 height)
{
    Impl::SetViewportCommand cmd;
    cmd.x = x;
    cmd.y = y;
    cmd.width = width;
    cmd.height = height;
    m_commands.EmplaceBack(cmd);
}

void Rndr::Canvas::DrawList::SetRenderTarget(const RenderTarget& target)
{
    Impl::SetRenderTargetCommand cmd;
    cmd.target = &target;
    m_commands.EmplaceBack(cmd);
}

void Rndr::Canvas::DrawList::SetRenderTarget(const Display& display)
{
    Impl::SetDisplayCommand cmd;
    cmd.display = &display;
    m_commands.EmplaceBack(cmd);
}

void Rndr::Canvas::DrawList::Draw(const Mesh& mesh, Brush& brush)
{
    Impl::DrawMeshCommand cmd;
    cmd.mesh = &mesh;
    cmd.brush = &brush;
    m_commands.EmplaceBack(cmd);
}

void Rndr::Canvas::DrawList::DrawIndirect(const Mesh& mesh, Brush& brush, const DrawCommandBuffer<DrawCommand>& commands)
{
    Impl::DrawIndirectCommand cmd;
    cmd.mesh = &mesh;
    cmd.brush = &brush;
    cmd.commands = &commands;
    m_commands.EmplaceBack(cmd);
}

void Rndr::Canvas::DrawList::DrawIndexedIndirect(const Mesh& mesh, Brush& brush,
                                                  const DrawCommandBuffer<DrawIndexedCommand>& commands)
{
    Impl::DrawIndexedIndirectCommand cmd;
    cmd.mesh = &mesh;
    cmd.brush = &brush;
    cmd.commands = &commands;
    m_commands.EmplaceBack(cmd);
}

void Rndr::Canvas::DrawList::Dispatch(Brush& brush, u32 group_count_x, u32 group_count_y, u32 group_count_z)
{
    Impl::DispatchCommand cmd;
    cmd.brush = &brush;
    cmd.group_count_x = group_count_x;
    cmd.group_count_y = group_count_y;
    cmd.group_count_z = group_count_z;
    m_commands.EmplaceBack(cmd);
}

void Rndr::Canvas::DrawList::Execute()
{
    RNDR_CPU_EVENT_SCOPED("Canvas::DrawList::Execute");

    for (u64 i = 0; i < m_commands.GetSize(); ++i)
    {
        m_commands[i].Visit(Opal::Overloaded{
            [](const Impl::SetViewportCommand& c) { glViewport(c.x, c.y, c.width, c.height); },
            [](const Impl::SetRenderTargetCommand& c)
            { glBindFramebuffer(GL_FRAMEBUFFER, c.target->GetNativeHandle()); },
            [](const Impl::SetDisplayCommand& c)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, c.display->GetWidth(), c.display->GetHeight());
            },
            [](const Impl::DrawMeshCommand& c)
            {
                c.brush->Apply();
                glBindVertexArray(c.mesh->GetNativeHandle());
                if (c.mesh->HasIndices())
                {
                    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(c.mesh->GetIndexCount()), GL_UNSIGNED_INT,
                                   nullptr);
                }
                else
                {
                    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(c.mesh->GetVertexCount()));
                }
            },
            [](Impl::DrawIndirectCommand&)
            {
                // Requires DrawCommandBuffer implementation.
            },
            [](Impl::DrawIndexedIndirectCommand&)
            {
                // Requires DrawCommandBuffer implementation.
            },
            [](const Impl::DispatchCommand& c)
            {
                c.brush->Apply();
                glDispatchCompute(c.group_count_x, c.group_count_y, c.group_count_z);
                glMemoryBarrier(GL_ALL_BARRIER_BITS);
            },
        });
    }

    m_commands.Clear();
}
