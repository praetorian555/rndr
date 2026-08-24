#pragma once

#include "opal/container/ref.h"

#include "rndr/canvas/brush.hpp"
#include "rndr/canvas/draw-list.hpp"
#include "rndr/canvas/mesh.hpp"
#include "rndr/canvas/shader.hpp"
#include "rndr/math.hpp"

namespace Rndr::Canvas
{

class Context;

/**
 * Renders an infinite ground-plane grid with axis-colored origin lines.
 * The Z-axis line is drawn in blue and the X-axis line in red.
 */
class GridRenderer
{
public:
    GridRenderer() = default;

    /**
     * Create the renderer with its shader, mesh and brush ready to draw.
     * @param context Context to render with. Has to outlive the renderer.
     * @return The renderer, or whatever creating its shader, mesh or brush reports. The reason is logged
     *         at error level.
     */
    [[nodiscard]] static Opal::Expected<GridRenderer, ErrorCode> Create(Opal::Ref<Context> context);

    ~GridRenderer();

    GridRenderer(const GridRenderer&) = delete;
    GridRenderer& operator=(const GridRenderer&) = delete;
    // Not defaulted: the brush points at the shader member, so a move has to re-point it.
    GridRenderer(GridRenderer&& other) noexcept;
    GridRenderer& operator=(GridRenderer&& other) noexcept;

    void Destroy();

    /**
     * Record draw commands into the draw list.
     * @param draw_list Draw list to record into.
     * @param view View matrix.
     * @param projection Projection matrix.
     */
    void Render(DrawList& draw_list, const Matrix4x4f& view, const Matrix4x4f& projection);

private:
    Opal::Ref<Context> m_context;
    Shader m_shader;
    Brush m_brush;
    Mesh m_mesh;
};

}  // namespace Rndr::Canvas
