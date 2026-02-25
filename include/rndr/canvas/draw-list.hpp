#pragma once

#include "rndr/types.hpp"

namespace Rndr
{
namespace Canvas
{

class Mesh;
class Brush;
class RenderTarget;
struct DrawCommand;
struct DrawIndexedCommand;
template<typename T>
class DrawCommandBuffer;

/**
 * Records draw calls, then executes and resets. Single use: Execute() processes all commands then
 * clears internal state. The list object is reusable across frames, but commands are consumed on
 * execute.
 */
class DrawList
{
public:
    DrawList() = default;
    ~DrawList();

    DrawList(const DrawList&) = delete;
    DrawList& operator=(const DrawList&) = delete;
    DrawList(DrawList&& other) noexcept;
    DrawList& operator=(DrawList&& other) noexcept;

    /** Set the viewport rectangle. */
    void SetViewport(i32 x, i32 y, i32 width, i32 height);

    /** Set the render target to draw to. Can be an off-screen target or a display target. */
    void SetRenderTarget(const RenderTarget& target);

    /** Record a draw call. */
    void Draw(const Mesh& mesh, const Brush& brush);

    /** Record an indirect draw call for non-indexed geometry. */
    void DrawIndirect(const Mesh& mesh, const Brush& brush, const DrawCommandBuffer<DrawCommand>& commands);

    /** Record an indirect draw call for indexed geometry. */
    void DrawIndexedIndirect(const Mesh& mesh, const Brush& brush, const DrawCommandBuffer<DrawIndexedCommand>& commands);

    /** Execute all recorded commands and clear internal state. */
    void Execute();

private:
};

}  // namespace Canvas
}  // namespace Rndr
