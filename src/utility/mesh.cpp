#include "rndr/utility/mesh.h"

#include "rndr/core/file.h"
#include "rndr/core/math.h"

namespace
{
constexpr uint32_t k_magic = 0x89ABCDEF;
}

bool Rndr::Mesh::ReadData(MeshData& out_mesh_data, const Rndr::String& file_path)
{
    Rndr::FileHandler f(file_path.c_str(), "rb");
    if (!f.IsValid())
    {
        RNDR_LOG_ERROR("Failed to open file %s!", file_path.c_str());
        return false;
    }

    MeshFileHeader header;
    if (!f.Read(&header, sizeof(MeshFileHeader), 1))
    {
        RNDR_LOG_ERROR("Failed to read mesh file header!");
        return false;
    }
    if (header.magic != k_magic)
    {
        RNDR_LOG_ERROR("Invalid mesh file magic!");
        return false;
    }

    out_mesh_data.meshes.resize(header.mesh_count);
    if (!f.Read(out_mesh_data.meshes.data(), sizeof(MeshDescription), header.mesh_count))
    {
        RNDR_LOG_ERROR("Failed to read mesh descriptions!");
        return false;
    }

    out_mesh_data.vertex_buffer_data.resize(header.vertex_buffer_size);
    if (!f.Read(out_mesh_data.vertex_buffer_data.data(), 1, header.vertex_buffer_size))
    {
        RNDR_LOG_ERROR("Failed to read vertex buffer data!");
        return false;
    }

    out_mesh_data.index_buffer_data.resize(header.index_buffer_size);
    if (!f.Read(out_mesh_data.index_buffer_data.data(), 1, header.index_buffer_size))
    {
        RNDR_LOG_ERROR("Failed to read index buffer data!");
        return false;
    }

    out_mesh_data.bounding_boxes.resize(header.mesh_count);
    if (!f.Read(out_mesh_data.bounding_boxes.data(), sizeof(Bounds3f), header.mesh_count))
    {
        RNDR_LOG_ERROR("Failed to read bounding boxes!");
        return false;
    }

    return true;
}

bool Rndr::Mesh::WriteData(const MeshData& mesh_data, const String& file_path)
{
    Rndr::FileHandler f(file_path.c_str(), "wb");
    if (!f.IsValid())
    {
        RNDR_LOG_ERROR("Failed to open file %s!", file_path.c_str());
        return false;
    }

    MeshFileHeader header;
    header.magic = k_magic;
    header.version = 1;
    header.mesh_count = static_cast<uint32_t>(mesh_data.meshes.size());
    header.data_offset = sizeof(MeshFileHeader) + header.mesh_count * sizeof(MeshDescription);
    header.vertex_buffer_size = static_cast<uint32_t>(mesh_data.vertex_buffer_data.size());
    header.index_buffer_size = static_cast<uint32_t>(mesh_data.index_buffer_data.size());

    f.Write(&header, sizeof(MeshFileHeader), 1);
    f.Write(mesh_data.meshes.data(), sizeof(MeshDescription), mesh_data.meshes.size());
    f.Write(mesh_data.vertex_buffer_data.data(), 1, mesh_data.vertex_buffer_data.size());
    f.Write(mesh_data.index_buffer_data.data(), 1, mesh_data.index_buffer_data.size());
    f.Write(mesh_data.bounding_boxes.data(), sizeof(Bounds3f), mesh_data.bounding_boxes.size());

    return true;
}

bool Rndr::Mesh::UpdateBoundingBoxes(Rndr::MeshData& mesh_data)
{
    mesh_data.bounding_boxes.clear();
    mesh_data.bounding_boxes.resize(mesh_data.meshes.size());

    for (size_t i = 0; i < mesh_data.meshes.size(); ++i)
    {
        const MeshDescription& mesh_desc = mesh_data.meshes[i];
        const uint32_t index_count = mesh_desc.GetLodIndicesCount(0);

        Point3f min(Math::k_largest_float);
        Point3f max(Math::k_smallest_float);

        uint32_t* index_buffer = reinterpret_cast<uint32_t*>(mesh_data.index_buffer_data.data());
        float* vertex_buffer = reinterpret_cast<float*>(mesh_data.vertex_buffer_data.data());
        for (uint32_t j = 0; j < index_count; ++j)
        {
            const uint32_t vertex_offset = mesh_desc.vertex_offset + index_buffer[mesh_desc.index_offset + j];
            const float* vertex = vertex_buffer + vertex_offset * (mesh_desc.vertex_size / sizeof(float));
            min = Math::Min(min, Point3f(vertex[0], vertex[1], vertex[2]));
            max = Math::Max(max, Point3f(vertex[0], vertex[1], vertex[2]));
        }

        mesh_data.bounding_boxes[i] = Bounds3f(min, max);
    }

    return false;
}

bool Rndr::Mesh::Merge(MeshData& out_mesh_data, const Span<MeshData>& mesh_data)
{
    if (mesh_data.empty())
    {
        return false;
    }

    uint32_t vertex_offset = 0;
    uint32_t index_offset = 0;
    for (const MeshData& mesh : mesh_data)
    {
        for (const MeshDescription& mesh_desc : mesh.meshes)
        {
            MeshDescription new_mesh_desc = mesh_desc;
            new_mesh_desc.vertex_offset += vertex_offset;
            new_mesh_desc.index_offset += index_offset;
            out_mesh_data.meshes.emplace_back(new_mesh_desc);

            vertex_offset += mesh_desc.vertex_count;
            for (uint32_t i = 0; i < mesh_desc.lod_count; ++i)
            {
                index_offset += mesh_desc.GetLodIndicesCount(i);
            }
        }

        out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(), mesh.vertex_buffer_data.begin(),
                                                mesh.vertex_buffer_data.end());
        out_mesh_data.index_buffer_data.insert(out_mesh_data.index_buffer_data.end(), mesh.index_buffer_data.begin(),
                                               mesh.index_buffer_data.end());
    }

    UpdateBoundingBoxes(out_mesh_data);

    return true;
}
