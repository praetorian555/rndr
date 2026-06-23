#pragma once

#include "opal/clonable-base.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"
#include "opal/variant.h"

#include "rndr/canvas/texture.hpp"
#include "rndr/math.hpp"
#include "rndr/types.hpp"

namespace Rndr::Canvas
{

class Mesh;
class Brush;
class Context;
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

struct SetRenderTargetCommand : Opal::ClonableBase<SetRenderTargetCommand>
{
    Opal::Ref<const RenderTarget> target;
    OPAL_CLONE_FIELDS(target);
};

struct SetContextCommand : Opal::ClonableBase<SetContextCommand>
{
    Opal::Ref<const Context> context;
    OPAL_CLONE_FIELDS(context);
};

struct DrawMeshCommand : Opal::ClonableBase<DrawMeshCommand>
{
    Opal::Ref<Mesh> mesh;
    Opal::Ref<Brush> brush;
    OPAL_CLONE_FIELDS(mesh, brush);
};

struct DrawMeshInstancedCommand : Opal::ClonableBase<DrawMeshInstancedCommand>
{
    Opal::Ref<Mesh> mesh;
    Opal::Ref<Brush> brush;
    u32 instance_count = 1;
    OPAL_CLONE_FIELDS(mesh, brush, instance_count);
};

// struct DrawIndirectCommand
// {
//     Opal::Ref<const Mesh> mesh;
//     Opal::Ref<Brush> brush;
//     const DrawCommandBuffer<::Rndr::Canvas::DrawCommand>* commands = nullptr;
// };
//
// struct DrawIndexedIndirectCommand
// {
//     const Mesh* mesh = nullptr;
//     Brush* brush = nullptr;
//     const DrawCommandBuffer<::Rndr::Canvas::DrawIndexedCommand>* commands = nullptr;
// };

struct DispatchCommand
{
    Brush* brush = nullptr;
    u32 group_count_x = 1;
    u32 group_count_y = 1;
    u32 group_count_z = 1;
};

struct ClearCommand
{
    Vector4f color = {0, 0, 0, 1};
    f32 depth = 1.0f;
    i32 stencil = 0;
    bool clear_color = true;
    bool clear_depth = true;
    bool clear_stencil = true;
};

struct BlitCommand : Opal::ClonableBase<BlitCommand>
{
    Opal::Ref<const Texture> source;
    Opal::Ref<const RenderTarget> destination;
    i32 src_x = 0;
    i32 src_y = 0;
    i32 src_width = 0;
    i32 src_height = 0;
    i32 dst_x = 0;
    i32 dst_y = 0;
    i32 dst_width = 0;
    i32 dst_height = 0;
    TextureFilter filter = TextureFilter::Linear;
    OPAL_CLONE_FIELDS(source, destination, src_x, src_y, src_width, src_height, dst_x, dst_y, dst_width, dst_height, filter);
};

struct BeginEventCommand
{
    const char* event_name;
};

struct EndEventCommand
{
    const char* event_name;
};

using CommandVariant = Opal::Variant<SetViewportCommand, SetRenderTargetCommand, SetContextCommand, DrawMeshCommand,
                                     DrawMeshInstancedCommand, DispatchCommand, ClearCommand, BlitCommand, BeginEventCommand,
                                     EndEventCommand>;

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
     * Bind the default framebuffer (handle 0) and set the viewport to match the context's
     * dimensions. Subsequent draw commands will render to the screen.
     */
    void SetRenderTarget(const Context& context);

    /** Clear color, depth, and stencil attachments of the currently bound render target. */
    void Clear(const Vector4f& color, f32 depth = 1.0f, i32 stencil = 0);

    /** Clear only the color attachment. */
    void ClearColor(const Vector4f& color);

    /** Clear only the depth and stencil attachments. */
    void ClearDepthStencil(f32 depth = 1.0f, i32 stencil = 0);

    /** Record a draw call. The mesh and brush must remain valid until Execute() is called. */
    void Draw(Mesh& mesh, Brush& brush);

    /**
     * Record an instanced draw call. The mesh and brush must remain valid until Execute() is called.
     * @param mesh Mesh to draw.
     * @param brush Brush with pipeline state and uniforms.
     * @param instance_count Number of instances to draw.
     */
    void DrawInstanced(Mesh& mesh, Brush& brush, u32 instance_count);

    // /** Record an indirect draw call for non-indexed geometry. */
    // void DrawIndirect(const Mesh& mesh, Brush& brush, const DrawCommandBuffer<DrawCommand>& commands);
    //
    // /** Record an indirect draw call for indexed geometry. */
    // void DrawIndexedIndirect(const Mesh& mesh, Brush& brush, const DrawCommandBuffer<DrawIndexedCommand>& commands);

    /**
     * Record a compute dispatch. The brush must hold a compute shader and remain valid until
     * Execute() is called. Issues a glMemoryBarrier(GL_ALL_BARRIER_BITS) after the dispatch.
     */
    void Dispatch(Brush& brush, u32 group_count_x, u32 group_count_y = 1, u32 group_count_z = 1);

    /**
     * Blit (copy) a source texture into a render target. The full source texture is copied into the
     * destination's first color attachment, stretching to fill the destination if their dimensions
     * differ. The source and destination must remain valid until Execute() is called.
     * @param source Source texture to copy from. Must be a single-sample Texture2D.
     * @param destination Render target to copy into.
     * @param filter Sampling filter used when the source and destination dimensions differ.
     */
    void Blit(const Texture& source, const RenderTarget& destination, TextureFilter filter = TextureFilter::Linear);

    /**
     * Blit (copy) a rectangular region of a source texture into a rectangular region of a render
     * target's first color attachment. The source region is stretched or shrunk to fit the
     * destination region. Regions use a bottom-left origin, matching OpenGL framebuffer coordinates.
     * The source and destination must remain valid until Execute() is called.
     * @param source Source texture to copy from. Must be a single-sample Texture2D.
     * @param src_x Horizontal texel offset of the source region.
     * @param src_y Vertical texel offset of the source region.
     * @param src_width Width of the source region in texels.
     * @param src_height Height of the source region in texels.
     * @param destination Render target to copy into.
     * @param dst_x Horizontal texel offset of the destination region.
     * @param dst_y Vertical texel offset of the destination region.
     * @param dst_width Width of the destination region in texels.
     * @param dst_height Height of the destination region in texels.
     * @param filter Sampling filter used when the source and destination regions differ in size.
     */
    void Blit(const Texture& source, i32 src_x, i32 src_y, i32 src_width, i32 src_height, const RenderTarget& destination,
              i32 dst_x, i32 dst_y, i32 dst_width, i32 dst_height, TextureFilter filter = TextureFilter::Linear);

    void BeginEvent(const char* event_name);
    void EndEvent(const char* event_name);

    /** Execute all recorded commands and clear internal state. */
    void Execute();

private:
    Opal::DynamicArray<Impl::CommandVariant> m_commands;
};

}  // namespace Rndr::Canvas
