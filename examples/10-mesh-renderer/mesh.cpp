#include "mesh.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "rndr/core/math.h"

namespace
{

}

bool ReadMeshData(MeshData& out_mesh_data, const struct aiScene& ai_scene, uint32_t attributes_to_load)
{
    if (!ai_scene.HasMeshes())
    {
        RNDR_LOG_ERROR("No meshes in the assimp scene!");
        return false;
    }

    const bool should_load_normals = attributes_to_load & k_load_normals;
    const bool should_load_uvs = attributes_to_load & k_load_uvs;
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
