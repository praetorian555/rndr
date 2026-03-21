#pragma once

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"

#include "rndr/canvas/buffer.hpp"
#include "rndr/canvas/vertex-layout.hpp"

namespace Rndr::Canvas
{

/**
 * Geometry data paired with its vertex layout. Owns GPU resources (VAO, VBO, IBO).
 * Vertex data stride is validated against the layout at construction.
 */
class Mesh
{
public:
    Mesh() = default;

    /**
     * Create a mesh from a vertex layout and data.
     * @param layout Vertex layout describing the data format.
     * @param vertex_data Raw vertex data. Size must be a multiple of the layout stride.
     * @param index_data Raw index data (u32 indices). Can be empty for non-indexed geometry.
     * @param debug_name Debug name of the mesh.
     */
    explicit Mesh(const VertexLayout& layout, Opal::ArrayView<const u8> vertex_data, Opal::ArrayView<const u8> index_data,
                  Opal::StringUtf8 debug_name = "");

    explicit Mesh(const VertexLayout& layout, i32 max_vertex_count, i32 max_index_count, Opal::StringUtf8 debug_name = "");

    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    [[nodiscard]] Mesh Clone() const;
    void Destroy();

    /**
     * Upload data from CPU side to the GPU if it changed since the last upload.
     */
    void Upload();

    /**
     * Append vertex and index data to the CPU side buffer.
     * @param vertex_data Vertex data to add.
     * @param index_data Index data to add.
     */
    void Append(Opal::ArrayView<const u8> vertex_data, Opal::ArrayView<const u8> index_data);

    /**
     * Clear CPU side buffer contents.
     */
    void Clear();

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] u32 GetNativeHandle() const;
    [[nodiscard]] u32 GetVertexCount() const;
    [[nodiscard]] u32 GetIndexCount() const;
    [[nodiscard]] bool HasIndices() const;
    [[nodiscard]] const VertexLayout& GetVertexLayout() const;

private:
    void SetupVAO();

    Opal::StringUtf8 m_debug_name;
    u32 m_vao = 0;
    Buffer m_vertex_buffer;
    Buffer m_index_buffer;
    u32 m_vertex_count = 0;
    u32 m_index_count = 0;
    u32 m_max_vertex_count = 0;
    u32 m_max_index_count = 0;
    VertexLayout m_layout;
    Opal::DynamicArray<u8> m_vertex_data;
    Opal::DynamicArray<u8> m_index_data;
    bool m_dirty = false;
};

}  // namespace Rndr::Canvas
