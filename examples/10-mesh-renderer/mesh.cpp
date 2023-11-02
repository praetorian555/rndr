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

    for (uint32_t mesh_index = 0; mesh_index < ai_scene.mNumMeshes; ++mesh_index)
    {
        aiMesh* ai_mesh = ai_scene.mMeshes[mesh_index];
        MeshDescription mesh_desc;
        Rndr::Array<uint32_t> indices;

        const bool has_normals = ai_mesh->HasNormals() && static_cast<bool>(attributes_to_load & k_load_normals);
        const bool has_uvs = ai_mesh->HasTextureCoords(0) && static_cast<bool>(attributes_to_load & k_load_uvs);

        uint32_t stream_offset_base = 0;
        if (!out_mesh_data.vertex_buffer_data.empty())
        {
            stream_offset_base = static_cast<uint32_t>(out_mesh_data.vertex_buffer_data.size());
        }

        for (uint32_t i = 0; i < ai_mesh->mNumVertices; ++i)
        {
            Rndr::Point3f position(ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z);
            out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(), reinterpret_cast<uint8_t*>(position.data),
                                                    reinterpret_cast<uint8_t*>(position.data + sizeof(position)));
            stream_offset_base += sizeof(position);

            if (has_normals)
            {
                Rndr::Normal3f normal(ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z);
                out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(), reinterpret_cast<uint8_t*>(normal.data),
                                                        reinterpret_cast<uint8_t*>(normal.data + sizeof(normal)));
                stream_offset_base += sizeof(normal);
            }
            if (has_uvs)
            {
                Rndr::Point2f uv(ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y);
                out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(), reinterpret_cast<uint8_t*>(uv.data),
                                                        reinterpret_cast<uint8_t*>(uv.data + sizeof(uv)));
                stream_offset_base += sizeof(uv);
            }
        }
        mesh_desc.stream_element_size[0] = sizeof(Rndr::Point3f);
        if (has_normals)
        {
            mesh_desc.stream_element_size[0] += sizeof(Rndr::Normal3f);
        }
        if (has_uvs)
        {
            mesh_desc.stream_element_size[0] += sizeof(Rndr::Point2f);
        }
        mesh_desc.stream_offsets[1] = stream_offset_base;

        mesh_desc.stream_count = 1;
        mesh_desc.vertex_count = ai_mesh->mNumVertices;

        uint32_t lod_offset_base = 0;
        if (!out_mesh_data.index_buffer_data.empty())
        {
            lod_offset_base = static_cast<uint32_t>(out_mesh_data.index_buffer_data.size());
        }

        for (uint32_t i = 0; i < ai_mesh->mNumFaces; ++i)
        {
            const aiFace& face = ai_mesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; ++j)
            {
                indices.emplace_back(face.mIndices[j]);
            }
        }

        uint32_t lod_count = 0;

        mesh_desc.lod_offsets[lod_count] = lod_offset_base;
        out_mesh_data.index_buffer_data.insert(out_mesh_data.index_buffer_data.end(), reinterpret_cast<uint8_t*>(indices.data()),
                                               reinterpret_cast<uint8_t*>(indices.data() + indices.size()));
        lod_offset_base += static_cast<uint32_t>(indices.size() * sizeof(int32_t));
        lod_count++;

        // TODO: Generate LODs

        // Once we are done with placing LODs, we have to add the end offset of the last LOD.
        mesh_desc.lod_offsets[lod_count] = lod_offset_base;

        mesh_desc.m_lod_count = lod_count;

        // Calculates total size in bytes of the mesh vertex buffer and index buffer.
        mesh_desc.mesh_size = mesh_desc.stream_offsets[1] + mesh_desc.lod_offsets[lod_count];

        // TODO: Add material info

        out_mesh_data.meshes.emplace_back(mesh_desc);
    }

    return true;
}
