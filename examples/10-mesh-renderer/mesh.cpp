#include "mesh.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "rndr/core/math.h"

namespace
{

}

bool ReadMeshData(const aiScene& ai_scene, MeshData& out_mesh_data)
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
        Rndr::Array<Rndr::Point3f> positions;
        Rndr::Array<Rndr::Normal3f> normals;
        Rndr::Array<Rndr::Point2f> uvs;
        Rndr::Array<uint32_t> indices;
        uint32_t stream_count = 1; // Positions are always guaranteed to be present.

        uint32_t stream_offset_base = 0;
        if (!out_mesh_data.vertex_buffer_data.empty())
        {
            stream_offset_base = static_cast<uint32_t>(out_mesh_data.vertex_buffer_data.size());
        }

        for (uint32_t i = 0; i < ai_mesh->mNumVertices; ++i)
        {
            positions.emplace_back(ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z);
        }
        mesh_desc.stream_element_size[stream_count - 1] = sizeof(Rndr::Point3f);
        mesh_desc.stream_offsets[stream_count - 1] = stream_offset_base;
        out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(),
                                                reinterpret_cast<uint8_t*>(positions.data()),
                                                reinterpret_cast<uint8_t*>(positions.data() + positions.size()));
        stream_offset_base += static_cast<uint32_t>(positions.size() * sizeof(Rndr::Point3f));

        if (ai_mesh->HasNormals())
        {
            for (uint32_t i = 0; i < ai_mesh->mNumVertices; ++i)
            {
                normals.emplace_back(ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z);
            }
            ++stream_count;
            mesh_desc.stream_element_size[stream_count - 1] = sizeof(Rndr::Normal3f);
            mesh_desc.stream_offsets[stream_count - 1] = stream_offset_base;
            out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(),
                                                    reinterpret_cast<uint8_t*>(normals.data()),
                                                    reinterpret_cast<uint8_t*>(normals.data() + normals.size()));
            stream_offset_base += static_cast<uint32_t>(normals.size() * sizeof(Rndr::Normal3f));
        }

        if (ai_mesh->HasTextureCoords(0))
        {
            for (uint32_t i = 0; i < ai_mesh->mNumVertices; ++i)
            {
                uvs.emplace_back(ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y);
            }
            ++stream_count;
            mesh_desc.stream_element_size[stream_count - 1] = sizeof(Rndr::Point2f);
            mesh_desc.stream_offsets[stream_count - 1] = stream_offset_base;
            out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(),
                                                    reinterpret_cast<uint8_t*>(uvs.data()),
                                                    reinterpret_cast<uint8_t*>(uvs.data() + uvs.size()));
            stream_offset_base += static_cast<uint32_t>(uvs.size() * sizeof(Rndr::Point2f));
        }

        // Once we are done with placing streams, we have to add the end offset of the last stream.
        mesh_desc.stream_offsets[stream_count] = stream_offset_base;

        mesh_desc.stream_count = stream_count;
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
        out_mesh_data.index_buffer_data.insert(out_mesh_data.index_buffer_data.end(),
                                               reinterpret_cast<uint8_t*>(indices.data()),
                                               reinterpret_cast<uint8_t*>(indices.data() + indices.size()));
        lod_offset_base += static_cast<uint32_t>(indices.size() * sizeof(int32_t));
        lod_count++;

        // TODO: Generate LODs

        // Once we are done with placing LODs, we have to add the end offset of the last LOD.
        mesh_desc.lod_offsets[lod_count] = lod_offset_base;

        mesh_desc.m_lod_count = lod_count;

        // Calculates total size in bytes of the mesh vertex buffer and index buffer.
        mesh_desc.mesh_size = mesh_desc.stream_offsets[stream_count] + mesh_desc.lod_offsets[lod_count];

        // TODO: Add material info

        out_mesh_data.meshes.emplace_back(mesh_desc);
    }

    return true;
}
