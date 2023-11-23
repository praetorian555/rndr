#include "rndr/utility/mesh.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "rndr/core/math.h"

namespace
{
    constexpr uint32_t k_magic = 0x89ABCDEF;
}

bool Rndr::ReadMeshData(MeshData& out_mesh_data, const aiScene& ai_scene, MeshAttributesToLoad attributes_to_load)
{
    if (!ai_scene.HasMeshes())
    {
        RNDR_LOG_ERROR("No meshes in the assimp scene!");
        return false;
    }

    const bool should_load_normals = static_cast<bool>(attributes_to_load & MeshAttributesToLoad::LoadNormals);
    const bool should_load_uvs = static_cast<bool>(attributes_to_load & MeshAttributesToLoad::LoadUvs);
    uint32_t vertex_size = sizeof(Rndr::Point3f);
    if (should_load_normals)
    {
        vertex_size += sizeof(Rndr::Normal3f);
    }
    if (should_load_uvs)
    {
        vertex_size += sizeof(Rndr::Point2f);
    }

    uint32_t vertex_offset = 0;
    uint32_t index_offset = 0;
    for (uint32_t mesh_index = 0; mesh_index < ai_scene.mNumMeshes; ++mesh_index)
    {
        aiMesh* ai_mesh = ai_scene.mMeshes[mesh_index];

        for (uint32_t i = 0; i < ai_mesh->mNumVertices; ++i)
        {
            Rndr::Point3f position(ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z);
            out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(), reinterpret_cast<uint8_t*>(position.data),
                                                    reinterpret_cast<uint8_t*>(position.data) + sizeof(position));

            if (should_load_normals)
            {
                RNDR_ASSERT(ai_mesh->HasNormals());
                Rndr::Normal3f normal(ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z);
                out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(), reinterpret_cast<uint8_t*>(normal.data),
                                                        reinterpret_cast<uint8_t*>(normal.data) + sizeof(normal));
            }
            if (should_load_uvs)
            {
                RNDR_ASSERT(ai_mesh->HasTextureCoords(0));
                Rndr::Point2f uv(ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y);
                out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(), reinterpret_cast<uint8_t*>(uv.data),
                                                        reinterpret_cast<uint8_t*>(uv.data) + sizeof(uv));
            }
        }

        Rndr::Array<Rndr::Array<uint32_t>> lods(MeshDescription::k_max_lods);
        for (uint32_t i = 0; i < ai_mesh->mNumFaces; ++i)
        {
            const aiFace& face = ai_mesh->mFaces[i];
            if (face.mNumIndices != 3)
            {
                continue;
            }
            for (uint32_t j = 0; j < face.mNumIndices; ++j)
            {
                lods[0].emplace_back(face.mIndices[j]);
            }
        }

        out_mesh_data.index_buffer_data.insert(out_mesh_data.index_buffer_data.end(), reinterpret_cast<uint8_t*>(lods[0].data()),
                                               reinterpret_cast<uint8_t*>(lods[0].data()) + lods[0].size() * sizeof(uint32_t));

        // TODO: Generate LODs

        MeshDescription mesh_desc;
        mesh_desc.stream_count = 1;
        mesh_desc.vertex_count = ai_mesh->mNumVertices;
        mesh_desc.vertex_offset = vertex_offset;
        mesh_desc.index_offset = index_offset;
        mesh_desc.stream_offsets[0] = vertex_offset * vertex_size;
        mesh_desc.stream_offsets[1] = (vertex_offset + ai_mesh->mNumVertices) * vertex_size;
        mesh_desc.stream_element_size[0] = vertex_size;
        mesh_desc.lod_count = 1;
        mesh_desc.lod_offsets[0] = 0;
        mesh_desc.lod_offsets[1] = static_cast<uint32_t>(lods[0].size());
        mesh_desc.mesh_size = ai_mesh->mNumVertices * vertex_size + static_cast<uint32_t>(lods[0].size()) * sizeof(uint32_t);

        // TODO: Add material info

        out_mesh_data.meshes.emplace_back(mesh_desc);

        vertex_offset += ai_mesh->mNumVertices;
        index_offset += static_cast<uint32_t>(lods[0].size());
    }

    return true;
}

bool Rndr::WriteMeshData(const MeshData& mesh_data, const Rndr::String& file_path)
{
    FILE* f = nullptr;
    fopen_s(&f, file_path.c_str(), "wb");
    if (f == nullptr)
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

    fwrite(&header, 1, sizeof(MeshFileHeader), f);
    fwrite(mesh_data.meshes.data(), 1, header.mesh_count * sizeof(MeshDescription), f);
    fwrite(mesh_data.vertex_buffer_data.data(), 1, mesh_data.vertex_buffer_data.size(), f);
    fwrite(mesh_data.index_buffer_data.data(), 1, mesh_data.index_buffer_data.size(), f);

    fclose(f);
    return true;
}

bool Rndr::ReadMeshData(MeshData& out_mesh_data, const Rndr::String& file_path)
{
    FILE* f = nullptr;
    fopen_s(&f,file_path.c_str(), "rb");
    if (f == nullptr)
    {
        RNDR_LOG_ERROR("Failed to open file %s!", file_path.c_str());
        return false;
    }

    MeshFileHeader header;
    if (fread(&header, 1, sizeof(MeshFileHeader), f) != sizeof(MeshFileHeader))
    {
        RNDR_LOG_ERROR("Failed to read mesh file header!");
        fclose(f);
        return false;
    }
    if (header.magic != k_magic)
    {
        RNDR_LOG_ERROR("Invalid mesh file magic!");
        fclose(f);
        return false;
    }

    out_mesh_data.meshes.resize(header.mesh_count);
    if (fread(out_mesh_data.meshes.data(), 1, header.mesh_count * sizeof(MeshDescription), f) != header.mesh_count * sizeof(MeshDescription))
    {
        RNDR_LOG_ERROR("Failed to read mesh descriptions!");
        fclose(f);
        return false;
    }

    out_mesh_data.vertex_buffer_data.resize(header.vertex_buffer_size);
    if (fread(out_mesh_data.vertex_buffer_data.data(), 1, header.vertex_buffer_size, f) != header.vertex_buffer_size)
    {
        RNDR_LOG_ERROR("Failed to read vertex buffer data!");
        fclose(f);
        return false;
    }

    out_mesh_data.index_buffer_data.resize(header.index_buffer_size);
    if (fread(out_mesh_data.index_buffer_data.data(), 1, header.index_buffer_size, f) != header.index_buffer_size)
    {
        RNDR_LOG_ERROR("Failed to read index buffer data!");
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}
