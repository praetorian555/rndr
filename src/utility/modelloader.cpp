#include "rndr/utility/modelloader.h"

#ifdef RNDR_ASSIMP

// This macro leaks into this cpp during unity builds and affects assimp headers
#undef min

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "rndr/core/log.h"

rndr::ModelLoader::ModelLoader() : m_Importer(New<Assimp::Importer>("AssimpImporter")) {}

rndr::ModelLoader::~ModelLoader()
{
    Delete<Assimp::Importer>(m_Importer);
}

rndr::ScopePtr<rndr::Model> rndr::ModelLoader::Load(std::string ModelPath)
{
    const uint32_t Flags = aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices |
                           aiProcess_MakeLeftHanded | aiProcess_Triangulate;
    const aiScene* Scene = m_Importer->ReadFile(ModelPath, Flags);
    if (Scene == nullptr)
    {
        RNDR_LOG_ERROR("ModelLoader::Load: Failed to load file with error: %s!",
                       m_Importer->GetErrorString());
        return {};
    }
    // TODO(Marko): Currently we don't support complex scenes with more then one mesh
    assert(Scene->mNumMeshes == 1);

    aiMesh* AiMesh = Scene->mMeshes[0];
    assert(AiMesh != nullptr);

    ScopePtr<Model> OutModel = CreateScoped<Model>(ModelPath.c_str());
    OutModel->Positions.resize(AiMesh->mNumVertices);
    OutModel->Indices.resize(AiMesh->mNumFaces * 3);
    OutModel->Normals.resize(AiMesh->mNumVertices);
    OutModel->Tangents.resize(AiMesh->mNumVertices);
    OutModel->Bitangents.resize(AiMesh->mNumVertices);

    for (int VertexIndex = 0; VertexIndex < OutModel->Positions.size(); VertexIndex++)
    {
        const aiVector3D AiPosition = AiMesh->mVertices[VertexIndex];
        OutModel->Positions[VertexIndex] = math::Point3{AiPosition.x, AiPosition.y, AiPosition.z};
        if (AiMesh->mNormals != nullptr)
        {
            const aiVector3D AiNormal = AiMesh->mNormals[VertexIndex];
            OutModel->Normals[VertexIndex] = math::Normal3{AiNormal.x, AiNormal.y, AiNormal.z};
        }
        if (AiMesh->mTangents != nullptr)
        {
            const aiVector3D AiTangent = AiMesh->mTangents[VertexIndex];
            OutModel->Tangents[VertexIndex] = math::Vector3{AiTangent.x, AiTangent.y, AiTangent.z};
        }
        if (AiMesh->mBitangents != nullptr)
        {
            const aiVector3D AiBitangent = AiMesh->mBitangents[VertexIndex];
            OutModel->Bitangents[VertexIndex] =
                math::Vector3{AiBitangent.x, AiBitangent.y, AiBitangent.z};
        }
    }
    for (uint32_t FaceIndex = 0; FaceIndex < AiMesh->mNumFaces; FaceIndex++)
    {
        const aiFace& AiFace = AiMesh->mFaces[FaceIndex];
        OutModel->Indices[FaceIndex * 3] = AiFace.mIndices[0];
        OutModel->Indices[FaceIndex * 3 + 1] = AiFace.mIndices[1];
        OutModel->Indices[FaceIndex * 3 + 2] = AiFace.mIndices[2];
    }

    m_Importer->FreeScene();

    return OutModel;
}

#endif
