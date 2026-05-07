#include "rndr/advanced/mesh.hpp"

#include "assimp/cimport.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "opal/container/array-view.h"
#include "opal/exceptions.h"
#include "opal/paths.h"

#include "rndr/math.hpp"

void Rndr::Forge::LoadMesh(const Opal::StringUtf8& file_path, Mesh& out_mesh)
{
    constexpr u32 k_ai_process_flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                       aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                                       aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                                       aiProcess_GenUVCoords | aiProcess_CalcTangentSpace;
    const aiScene* scene = aiImportFile(*file_path, k_ai_process_flags);
    if (scene == nullptr || !scene->HasMeshes())
    {
        throw Opal::Exception("Failed to load mesh file or file contains no meshes!");
    }

    const aiMesh* ai_mesh = scene->mMeshes[0];
    if (ai_mesh->mVertices == nullptr)
    {
        aiReleaseImport(scene);
        throw Opal::Exception("Mesh has no position data!");
    }
    if (ai_mesh->mNormals == nullptr)
    {
        aiReleaseImport(scene);
        throw Opal::Exception("Mesh has no normal data!");
    }
    if (ai_mesh->mTextureCoords[0] == nullptr)
    {
        aiReleaseImport(scene);
        throw Opal::Exception("Mesh has no UV data!");
    }

    const Opal::StringUtf8 mesh_name = Opal::Paths::GetFileName(file_path).GetValue();

    out_mesh.name = mesh_name.Clone();
    out_mesh.vertex_size = sizeof(Point3f) + sizeof(Normal3f) + sizeof(Point2f);
    out_mesh.vertex_count = 0;
    out_mesh.index_size = sizeof(u32);
    out_mesh.index_count = 0;
    out_mesh.vertices.Clear();
    out_mesh.indices.Clear();

    for (u32 vertex_idx = 0; vertex_idx < ai_mesh->mNumVertices; ++vertex_idx)
    {
        Point3f position(ai_mesh->mVertices[vertex_idx].x, ai_mesh->mVertices[vertex_idx].y, ai_mesh->mVertices[vertex_idx].z);
        Normal3f normal(ai_mesh->mNormals[vertex_idx].x, ai_mesh->mNormals[vertex_idx].y, ai_mesh->mNormals[vertex_idx].z);
        Point2f uv(ai_mesh->mTextureCoords[0][vertex_idx].x, ai_mesh->mTextureCoords[0][vertex_idx].y);
        out_mesh.vertices.Append(Opal::AsWritableBytes(position));
        out_mesh.vertices.Append(Opal::AsWritableBytes(normal));
        out_mesh.vertices.Append(Opal::AsWritableBytes(uv));
        ++out_mesh.vertex_count;
    }

    for (u32 face_idx = 0; face_idx < ai_mesh->mNumFaces; ++face_idx)
    {
        const aiFace& face = ai_mesh->mFaces[face_idx];
        if (face.mNumIndices != 3)
        {
            continue;
        }
        for (u32 index_idx = 0; index_idx < face.mNumIndices; ++index_idx)
        {
            out_mesh.indices.Append(Opal::AsWritableBytes<u32>(face.mIndices[index_idx]));
            ++out_mesh.index_count;
        }
    }

    aiReleaseImport(scene);
}