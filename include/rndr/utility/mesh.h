#pragma once

#include "rndr/core/base.h"
#include "rndr/core/containers/array.h"
#include "rndr/core/containers/span.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/enum-flags.h"
#include "rndr/core/math.h"

struct aiScene;

namespace Rndr
{

/**
 * Description of a single mesh. It contains information about the mesh's streams and LODs. It does not contain the actual
 * mesh data. Mesh data is stored in MeshData.
 */
struct MeshDescription final
{
public:
    static constexpr const uint32_t k_max_lods = 8;
    static constexpr const uint32_t k_max_streams = 8;

    uint32_t material_id = 0;

    /** Total size of the mesh data in bytes. Equal to sum of all vertices and all indices. */
    uint32_t mesh_size = 0;

    /** Number of vertices belonging to this mesh in the vertex buffer. */
    uint32_t vertex_count = 0;

    /** Offset of the mesh in the vertex buffer in vertices. */
    uint32_t vertex_offset = 0;

    /** Sizes of the vertex in bytes. */
    uint32_t vertex_size = 0;

    /** Offset of the mesh in the index buffer in indices. */
    uint32_t index_offset = 0;

    /** Number of LODs of this mesh. */
    uint32_t lod_count = 0;

    /** Offsets of the LODs in indices starting from 0. First index is reserved for most detailed version of the mesh. */
    StackArray<uint32_t, k_max_lods> lod_offsets = {};

    [[nodiscard]] RNDR_FORCE_INLINE uint32_t GetLodIndicesCount(uint32_t lod) const
    {
        RNDR_ASSERT(lod < lod_count);
        return lod_offsets[lod + 1] - lod_offsets[lod];
    }
};

/**
 * Collection of multiple meshes all stored in single vertex and index buffers. It also contains descriptions of all meshes.
 */
struct MeshData
{
    Array<MeshDescription> meshes;
    Array<uint8_t> vertex_buffer_data;
    Array<uint8_t> index_buffer_data;
    Array<Bounds3f> bounding_boxes;
};

/**
 * Used to create indirect draw commands for rendering meshes.
 */
struct MeshDrawData
{
    uint32_t mesh_index;
    uint32_t material_index;
    uint32_t lod;
    uint32_t vertex_buffer_offset;
    uint32_t index_buffer_offset;
    uint32_t transform_index;
};

/**
 * Header of the mesh file.
 */
struct MeshFileHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t mesh_count;
    uint32_t data_offset;
    uint32_t vertex_buffer_size;
    uint32_t index_buffer_size;
};

enum class MeshAttributesToLoad : uint8_t
{
    LoadPositions = 1 << 0,
    LoadNormals = 1 << 1,
    LoadUvs = 1 << 2,
    LoadAll = LoadPositions | LoadNormals | LoadUvs,
};
RNDR_ENUM_CLASS_FLAGS(MeshAttributesToLoad)

namespace Mesh
{

/**
 * Reads mesh data from a file containing optimized rndr mesh data format.
 * @param out_mesh_data Destination mesh data.
 * @param file_path Path to the file.
 * @return True if mesh data was read successfully, false otherwise.
 */
bool ReadData(MeshData& out_mesh_data, const String& file_path);

/**
 * Writes mesh data to a file containing optimized rndr mesh data format.
 * @param mesh_data Mesh data to write.
 * @param file_path Path to the file.
 * @return True if mesh data was written successfully, false otherwise.
 */
bool WriteData(const MeshData& mesh_data, const String& file_path);

/**
 * Updates bounding boxes of all meshes in the mesh data.
 * @param mesh_data Mesh data to update.
 * @return True if bounding boxes were updated successfully, false otherwise.
 */
bool UpdateBoundingBoxes(MeshData& mesh_data);

/**
 * Merges multiple mesh data into single mesh data. All meshes are stored in single vertex and index buffers. Mesh descriptions are updated
 * accordingly.
 * @param out_mesh_data Destination mesh data.
 * @param mesh_data Mesh data to merge.
 * @return True if mesh data was merged successfully, false otherwise.
 */
bool Merge(MeshData& out_mesh_data, const Span<MeshData>& mesh_data);

}  // namespace Mesh

}  // namespace Rndr
