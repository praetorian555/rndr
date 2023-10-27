#pragma once

#include "rndr/core/base.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/containers/array.h"

/**
 * Mesh is a collection of vertex and index buffers. It is used to represent a single renderable object. All vertex attributes are stored in
 * separate streams in the vertex buffer. Each stream has a different size and element size. The index buffer contains indices for all LODs
 * of the mesh. Each LOD is stored in a separate part of the index buffer.
 */
struct MeshDescription final
{
private:
    static constexpr const uint32_t k_max_lods = 8;
    static constexpr const uint32_t k_max_streams = 8;

public:
    uint32_t material_id = 0;

    uint32_t mesh_size = 0; // Total size of the mesh in bytes. Total size of index buffer and vertex buffer in bytes.

    uint32_t stream_count = 0; // Number of streams in the vertex buffer.
    uint32_t vertex_count = 0; // Total number of vertices stored in the vertex buffer.
    Rndr::StackArray<uint32_t, k_max_streams> stream_offsets; // Sizes of the streams in the vertex buffer.
    Rndr::StackArray<uint32_t, k_max_streams> stream_element_size; // Sizes of the elements in the streams in the vertex buffer.

    uint32_t m_lod_count = 0; // Number of LODs in the index buffer.
    Rndr::StackArray<uint32_t, k_max_lods> lod_offsets; // Positions of the LODs in the index buffer.

    RNDR_FORCE_INLINE uint32_t GetLodIndicesCount(uint32_t lod) const
    {
        RNDR_ASSERT(lod < m_lod_count - 1);
        return lod_offsets[lod + 1] - lod_offsets[lod];
    }
};

/**
 * Collection of multiple meshes all stored in single vertex and index buffers.
 */
struct MeshData
{
    Rndr::Array<MeshDescription> meshes;
    Rndr::Array<uint8_t> vertex_buffer_data;
    Rndr::Array<uint8_t> index_buffer_data;
};

bool ReadMeshData(const struct aiScene& ai_scene, MeshData& out_mesh_data);