#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/variant.h"

#include "rndr/types.hpp"

namespace Rndr::Canvas
{

class Mesh;
class Brush;
class Display;
class RenderTarget;
struct DrawCommand;
struct DrawIndexedCommand;
template <typename T>
class DrawCommandBuffer;

namespace Impl
{

struct SetViewportCommand
{
    i32 x;
    i32 y;
    i32 width;
    i32 height;
};

struct SetRenderTargetCommand
{
    const RenderTarget* target = nullptr;
};

struct SetDisplayCommand
{
    const Display* display = nullptr;
};

struct DrawMeshCommand
{
    const Mesh* mesh = nullptr;
    Brush* brush = nullptr;
};

struct DrawIndirectCommand
{
    const Mesh* mesh = nullptr;
    Brush* brush = nullptr;
    const DrawCommandBuffer<::Rndr::Canvas::DrawCommand>* commands = nullptr;
};

struct DrawIndexedIndirectCommand
{
    const Mesh* mesh = nullptr;
    Brush* brush = nullptr;
    const DrawCommandBuffer<::Rndr::Canvas::DrawIndexedCommand>* commands = nullptr;
};

struct DispatchCommand
{
    Brush* brush = nullptr;
    u32 group_count_x = 1;
    u32 group_count_y = 1;
    u32 group_count_z = 1;
};

using CommandVariant = Opal::Variant<SetViewportCommand, SetRenderTargetCommand, SetDisplayCommand, DrawMeshCommand,
                                     DrawIndirectCommand, DrawIndexedIndirectCommand, DispatchCommand>;

}  // namespace Impl

/**
 * Records draw calls, then executes and resets. Single use: Execute() processes all commands then
 * clears internal state. The list object is reusable across frames, but commands are consumed on
 * execute.
 *
 * Typical usage:
 * @code
 *   DrawList list;
 *   list.SetRenderTarget(display);
 *   list.Draw(mesh, brush);
 *   list.Execute();
 * @endcode
 */
class DrawList
{
public:
    DrawList() = default;
    ~DrawList() = default;

    DrawList(const DrawList&) = delete;
    DrawList& operator=(const DrawList&) = delete;
    DrawList(DrawList&& other) noexcept = default;
    DrawList& operator=(DrawList&& other) noexcept = default;

    /** Set the viewport rectangle. */
    void SetViewport(i32 x, i32 y, i32 width, i32 height);

    /**
     * Bind an off-screen render target. Subsequent draw commands will render into this FBO.
     */
    void SetRenderTarget(const RenderTarget& target);

    /**
     * Bind the display's default framebuffer (handle 0) and set the viewport to match the
     * display's dimensions. Subsequent draw commands will render to the screen.
     */
    void SetRenderTarget(const Display& display);

    /** Record a draw call. The mesh and brush must remain valid until Execute() is called. */
    void Draw(const Mesh& mesh, Brush& brush);

    /** Record an indirect draw call for non-indexed geometry. */
    void DrawIndirect(const Mesh& mesh, Brush& brush, const DrawCommandBuffer<DrawCommand>& commands);

    /** Record an indirect draw call for indexed geometry. */
    void DrawIndexedIndirect(const Mesh& mesh, Brush& brush, const DrawCommandBuffer<DrawIndexedCommand>& commands);

    /**
     * Record a compute dispatch. The brush must hold a compute shader and remain valid until
     * Execute() is called. Issues a glMemoryBarrier(GL_ALL_BARRIER_BITS) after the dispatch.
     */
    void Dispatch(Brush& brush, u32 group_count_x, u32 group_count_y = 1, u32 group_count_z = 1);

    /** Execute all recorded commands and clear internal state. */
    void Execute();

private:
    Opal::DynamicArray<Impl::CommandVariant> m_commands;
};

}  // namespace Rndr::Canvas
