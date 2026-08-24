#pragma once

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"

#include "rndr/canvas/buffer.hpp"
#include "rndr/canvas/vertex-layout.hpp"
#include "rndr/error-codes.hpp"

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
     * @return The mesh, ErrorCode::InvalidArgument for an invalid layout or data that does not match it,
     *         or whatever creating the GPU buffers reports. The reason is logged at error level.
     */
    [[nodiscard]] static Opal::Expected<Mesh, ErrorCode> Create(const VertexLayout& layout, Opal::ArrayView<const u8> vertex_data,
                                                                Opal::ArrayView<const u8> index_data, Opal::StringUtf8 debug_name = "");

    /**
     * Create an empty mesh with room for the given number of vertices and indices, filled through Append.
     * @return The mesh, ErrorCode::InvalidArgument for an invalid layout, or whatever creating the GPU
     *         buffers reports.
     */
    [[nodiscard]] static Opal::Expected<Mesh, ErrorCode> Create(const VertexLayout& layout, i32 max_vertex_count, i32 max_index_count,
                                                                Opal::StringUtf8 debug_name = "");

    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    /**
     * Copy this mesh into a new one from its CPU-side data.
     * @return The clone, ErrorCode::InvalidArgument for an invalid mesh, or whatever creating the GPU
     *         buffers reports.
     */
    [[nodiscard]] Opal::Expected<Mesh, ErrorCode> Clone() const;
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
    [[nodiscard]] ErrorCode SetupVAO();

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
