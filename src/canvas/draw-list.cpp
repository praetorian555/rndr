#include "rndr/canvas/draw-list.hpp"

#include "glad/glad.h"

#include "rndr/canvas/brush.hpp"
#include "rndr/canvas/context.hpp"
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
    m_commands.PushBack(std::move(cmd));
}

void Rndr::Canvas::DrawList::SetRenderTarget(const Context& context)
{
    Impl::SetContextCommand cmd;
    cmd.context = &context;
    m_commands.PushBack(std::move(cmd));
}

void Rndr::Canvas::DrawList::Clear(const Vector4f& color, f32 depth, i32 stencil)
{
    Impl::ClearCommand cmd;
    cmd.color = color;
    cmd.depth = depth;
    cmd.stencil = stencil;
    cmd.clear_color = true;
    cmd.clear_depth = true;
    cmd.clear_stencil = true;
    m_commands.EmplaceBack(cmd);
}

void Rndr::Canvas::DrawList::ClearColor(const Vector4f& color)
{
    Impl::ClearCommand cmd;
    cmd.color = color;
    cmd.clear_color = true;
    cmd.clear_depth = false;
    cmd.clear_stencil = false;
    m_commands.EmplaceBack(cmd);
}

void Rndr::Canvas::DrawList::ClearDepthStencil(f32 depth, i32 stencil)
{
    Impl::ClearCommand cmd;
    cmd.depth = depth;
    cmd.stencil = stencil;
    cmd.clear_color = false;
    cmd.clear_depth = true;
    cmd.clear_stencil = true;
    m_commands.EmplaceBack(cmd);
}

void Rndr::Canvas::DrawList::Draw(Mesh& mesh, Brush& brush)
{
    Impl::DrawMeshCommand cmd;
    cmd.mesh = &mesh;
    cmd.brush = &brush;
    m_commands.PushBack(std::move(cmd));
}

void Rndr::Canvas::DrawList::DrawInstanced(Mesh& mesh, Brush& brush, u32 instance_count)
{
    Impl::DrawMeshInstancedCommand cmd;
    cmd.mesh = &mesh;
    cmd.brush = &brush;
    cmd.instance_count = instance_count;
    m_commands.PushBack(std::move(cmd));
}

// void Rndr::Canvas::DrawList::DrawIndirect(const Mesh& mesh, Brush& brush, const DrawCommandBuffer<DrawCommand>& commands)
// {
//     Impl::DrawIndirectCommand cmd;
//     cmd.mesh = &mesh;
//     cmd.brush = &brush;
//     cmd.commands = &commands;
//     m_commands.EmplaceBack(cmd);
// }
//
// void Rndr::Canvas::DrawList::DrawIndexedIndirect(const Mesh& mesh, Brush& brush, const DrawCommandBuffer<DrawIndexedCommand>& commands)
// {
//     Impl::DrawIndexedIndirectCommand cmd;
//     cmd.mesh = &mesh;
//     cmd.brush = &brush;
//     cmd.commands = &commands;
//     m_commands.EmplaceBack(cmd);
// }

void Rndr::Canvas::DrawList::Dispatch(Brush& brush, u32 group_count_x, u32 group_count_y, u32 group_count_z)
{
    Impl::DispatchCommand cmd;
    cmd.brush = &brush;
    cmd.group_count_x = group_count_x;
    cmd.group_count_y = group_count_y;
    cmd.group_count_z = group_count_z;
    m_commands.EmplaceBack(cmd);
}

void Rndr::Canvas::DrawList::BeginEvent(const char* event_name)
{
    m_commands.EmplaceBack<Impl::BeginEventCommand>({event_name});
}

void Rndr::Canvas::DrawList::EndEvent(const char* event_name)
{
    m_commands.EmplaceBack<Impl::EndEventCommand>({event_name});
}

void Rndr::Canvas::DrawList::Execute()
{
    RNDR_CPU_EVENT_SCOPED("Canvas::DrawList::Execute");

    for (u64 i = 0; i < m_commands.GetSize(); ++i)
    {
        m_commands[i].Visit(Opal::Overloaded{
            [](const Impl::SetViewportCommand& c) { glViewport(c.x, c.y, c.width, c.height); },
            [](const Impl::SetRenderTargetCommand& c) { glBindFramebuffer(GL_FRAMEBUFFER, c.target->GetNativeHandle()); },
            [](const Impl::SetContextCommand& c)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, c.context->GetWidth(), c.context->GetHeight());
            },
            [](const Impl::DrawMeshCommand& c)
            {
                c.brush->Apply();
                c.mesh->Upload();
                glBindVertexArray(c.mesh->GetNativeHandle());
                if (c.mesh->HasIndices())
                {
                    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(c.mesh->GetIndexCount()), GL_UNSIGNED_INT, nullptr);
                }
                else
                {
                    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(c.mesh->GetVertexCount()));
                }
            },
            [](const Impl::DrawMeshInstancedCommand& c)
            {
                c.brush->Apply();
                c.mesh->Upload();
                glBindVertexArray(c.mesh->GetNativeHandle());
                if (c.mesh->HasIndices())
                {
                    glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(c.mesh->GetIndexCount()), GL_UNSIGNED_INT,
                                            nullptr, static_cast<GLsizei>(c.instance_count));
                }
                else
                {
                    glDrawArraysInstanced(GL_TRIANGLES, 0, static_cast<GLsizei>(c.mesh->GetVertexCount()),
                                          static_cast<GLsizei>(c.instance_count));
                }
            },
            [](const Impl::DispatchCommand& c)
            {
                c.brush->Apply();
                glDispatchCompute(c.group_count_x, c.group_count_y, c.group_count_z);
                glMemoryBarrier(GL_ALL_BARRIER_BITS);
            },
            [](const Impl::ClearCommand& c)
            {
                GLbitfield mask = 0;
                if (c.clear_color)
                {
                    glClearColor(c.color.x, c.color.y, c.color.z, c.color.w);
                    mask |= GL_COLOR_BUFFER_BIT;
                }
                if (c.clear_depth)
                {
                    glClearDepth(static_cast<GLdouble>(c.depth));
                    mask |= GL_DEPTH_BUFFER_BIT;
                }
                if (c.clear_stencil)
                {
                    glClearStencil(c.stencil);
                    mask |= GL_STENCIL_BUFFER_BIT;
                }
                if (mask != 0)
                {
                    glClear(mask);
                }
            },
            [](const Impl::BeginEventCommand& c)
            {
                Trace::BeginGpuEvent(c.event_name);
            },
            [](const Impl::EndEventCommand& c)
            {
                Trace::EndGpuEvent(c.event_name);
            }
        });
    }

    m_commands.Clear();
}
