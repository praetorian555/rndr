#pragma once

#include "opal/container/array-view.h"

#include "rndr/canvas/vertex-layout.hpp"

namespace Rndr
{
namespace Canvas
{

/**
 * Geometry data paired with its vertex layout. Owns its data and knows its shape. When the layout
 * is shader-derived, vertex data stride is validated at construction.
 */
class Mesh
{
public:
    Mesh() = default;

    /**
     * Create a mesh from a vertex layout and data.
     * @param layout Vertex layout describing the data format.
     * @param vertex_data Raw vertex data. Stride is validated against the layout.
     * @param index_data Raw index data. Can be empty for non-indexed geometry.
     */
    explicit Mesh(const VertexLayout& layout, Opal::ArrayView<const u8> vertex_data, Opal::ArrayView<const u8> index_data = {});
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    [[nodiscard]] Mesh Clone() const;
    void Destroy();

    [[nodiscard]] bool IsValid() const;

private:
    u32 m_vao = 0;
    u32 m_vbo = 0;
    u32 m_ibo = 0;
};

}  // namespace Canvas
}  // namespace Rndr
