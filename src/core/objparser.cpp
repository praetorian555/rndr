#include "rndr/core/objparser.h"

#include <filesystem>

#include "math/normal3.h"
#include "math/point3.h"
#include "math/vector2.h"
#include "math/vector3.h"

#include "rndr/core/log.h"

#include "rndr/render/material.h"
#include "rndr/render/mesh.h"
#include "rndr/render/model.h"

rndr::Model* rndr::ObjParser::Parse(const std::string& FilePath)
{
    if (!std::filesystem::exists(FilePath))
    {
        RNDR_LOG_ERROR_OR_ASSERT("Failed to find file: %s!", FilePath.c_str());
        return nullptr;
    }
    FILE* FileHandle = fopen(FilePath.c_str(), "rb");
    if (!FileHandle)
    {
        RNDR_LOG_ERROR_OR_ASSERT("Failed to open file: %s!", FilePath.c_str());
        return nullptr;
    }

    fseek(FileHandle, 0, SEEK_END);
    uint64_t FileSize = ftell(FileHandle);
    assert(FileSize > 0);
    fseek(FileHandle, 0, SEEK_SET);
    Span<char> FileContents;
    FileContents.Data = new char[FileSize];
    FileContents.Size = FileSize;
    uint64_t BytesRead = 0;
    int ReadAttempts = 0;
    constexpr int MaxReadAttempts = 10;
    while (BytesRead != FileSize || ReadAttempts < MaxReadAttempts)
    {
        uint64_t LastRead = fread(FileContents.Data + BytesRead, 1, FileSize - BytesRead, FileHandle);
        BytesRead += LastRead;
        ReadAttempts++;
    }
    assert(BytesRead == FileSize);
    fclose(FileHandle);

    Model* Mod = Parse(FileContents);
    delete[] FileContents.Data;
    return Mod;
}

static int GoToNextLine(const char* Data, int StartPosition)
{
    while (Data[StartPosition] != '\n' || Data[StartPosition] == EOF)
        StartPosition++;
    StartPosition++;
    return StartPosition;
}

static int ScanString(const char* StreamPtr, char* DstBuffer, int MaxBufferSize)
{
    memset(DstBuffer, 0, MaxBufferSize);
    for (int i = 0; i < MaxBufferSize - 1; i++)
    {
        char Ch = *StreamPtr++;
        if (Ch != '\r' && Ch != '\n' && Ch != ' ')
        {
            DstBuffer[i] = Ch;
        }
    }
    return 1;
}

struct ObjFace
{
    int Positions[3];
    int TexCoords[3];
    int Normals[3];
};

static rndr::Mesh* ConstructMesh(const std::string& Name,
                                 const std::vector<math::Point3>& Positions,
                                 const std::vector<math::Vector2>& TexCoords,
                                 const std::vector<math::Normal3>& Normals,
                                 const std::vector<ObjFace>& Faces);

static float ReadFloat(const char* Data, int& Position)
{
    char* EndPtr;
    float Value = strtof(Data, &EndPtr);
    Position += EndPtr - Data;
    return Value;
}

static int ReadInt(const char* Data, int& Position)
{
    char* EndPtr;
    int Value = strtol(Data, &EndPtr, 10);
    Position += EndPtr - Data;
    return Value;
}

rndr::Model* rndr::ObjParser::Parse(Span<char> Data)
{
    std::string MeshName;
    constexpr int DefaultSize = 50'000;
    std::vector<math::Point3> Positions;
    std::vector<math::Vector2> TexCoords;
    std::vector<math::Normal3> Normals;
    std::vector<ObjFace> Faces;
    Positions.reserve(DefaultSize);
    TexCoords.reserve(DefaultSize);
    Normals.reserve(DefaultSize);
    Faces.reserve(DefaultSize);

    Model* Mod = new Model();

    for (int Position = 0; Position < Data.Size;)
    {
        // Line comment
        if (strncmp(Data.Data + Position, "#", 1) == 0)
        {
            Position = GoToNextLine(Data.Data, Position);
        }
        // Vertex texture coordinate
        else if (strncmp(Data.Data + Position, "vt ", 3) == 0)
        {
            Position += 3;
            math::Vector2 Value;
            Value.X = ReadFloat(Data.Data + Position, Position);
            Value.Y = ReadFloat(Data.Data + Position, Position);
            TexCoords.push_back(Value);
            Position = GoToNextLine(Data.Data, Position);
        }
        // Vertex normal
        else if (strncmp(Data.Data + Position, "vn ", 3) == 0)
        {
            Position += 3;
            math::Normal3 Normal;
            Normal.X = ReadFloat(Data.Data + Position, Position);
            Normal.Y = ReadFloat(Data.Data + Position, Position);
            Normal.Z = ReadFloat(Data.Data + Position, Position);
            Normals.push_back(Normal);
            Position = GoToNextLine(Data.Data, Position);
        }
        // Vertex position
        else if (strncmp(Data.Data + Position, "v ", 2) == 0)
        {
            Position += 2;
            math::Point3 VertexPosition;
            VertexPosition.X = ReadFloat(Data.Data + Position, Position);
            VertexPosition.Y = ReadFloat(Data.Data + Position, Position);
            VertexPosition.Z = ReadFloat(Data.Data + Position, Position);
            Positions.push_back(VertexPosition);
            Position = GoToNextLine(Data.Data, Position);
        }
        // Face
        else if (strncmp(Data.Data + Position, "f ", 2) == 0)
        {
            Position += 2;
            ObjFace Face;
            Face.Positions[0] = ReadInt(Data.Data + Position, Position);
            Position++;
            Face.TexCoords[0] = ReadInt(Data.Data + Position, Position);
            Position++;
            Face.Normals[0] = ReadInt(Data.Data + Position, Position);
            Face.Positions[1] = ReadInt(Data.Data + Position, Position);
            Position++;
            Face.TexCoords[1] = ReadInt(Data.Data + Position, Position);
            Position++;
            Face.Normals[1] = ReadInt(Data.Data + Position, Position);
            Face.Positions[2] = ReadInt(Data.Data + Position, Position);
            Position++;
            Face.TexCoords[2] = ReadInt(Data.Data + Position, Position);
            Position++;
            Face.Normals[2] = ReadInt(Data.Data + Position, Position);
            Faces.push_back(Face);
            Position = GoToNextLine(Data.Data, Position);
        }
        // Material library
        else if (strncmp(Data.Data + Position, "mtllib ", 7) == 0)
        {
            Position = GoToNextLine(Data.Data, Position);
        }
        // Mesh name
        else if (strncmp(Data.Data + Position, "o ", 2) == 0)
        {
            if (!MeshName.empty())
            {
                Mesh* Mesh = ConstructMesh(MeshName, Positions, TexCoords, Normals, Faces);
                Mod->AddMesh(Mesh);
                MeshName.clear();
                // Positions.clear();
                // TexCoords.clear();
                // Normals.clear();
                // Faces.clear();
            }

            Position += 2;
            char Name[128] = {};
            int Result = ScanString(Data.Data + Position, Name, 128);
            MeshName = Name;
            Position = GoToNextLine(Data.Data, Position);
        }
        // Smooth shading
        else if (strncmp(Data.Data + Position, "s ", 2) == 0)
        {
            Position = GoToNextLine(Data.Data, Position);
        }
        // Use material
        else if (strncmp(Data.Data + Position, "usemtl ", 7) == 0)
        {
            Position = GoToNextLine(Data.Data, Position);
        }
        else
        {
            Position = GoToNextLine(Data.Data, Position);
        }
    }

    if (!MeshName.empty())
    {
        Mesh* Mesh = ConstructMesh(MeshName, Positions, TexCoords, Normals, Faces);
        Mod->AddMesh(Mesh);
    }

    return Mod;
}

static rndr::Mesh* ConstructMesh(const std::string& Name,
                                 const std::vector<math::Point3>& Positions,
                                 const std::vector<math::Vector2>& TexCoords,
                                 const std::vector<math::Normal3>& Normals,
                                 const std::vector<ObjFace>& Faces)
{
    const int VertexCount = Faces.size() * 3;
    rndr::Span<math::Point3> MeshPositions(new math::Point3[VertexCount], VertexCount);
    rndr::Span<math::Vector2> MeshTexCoords(new math::Vector2[VertexCount], VertexCount);
    rndr::Span<math::Normal3> MeshNormals(new math::Normal3[VertexCount], VertexCount);
    rndr::Span<int> MeshIndices(new int[VertexCount], VertexCount);
    rndr::Span<math::Vector3> MeshTangents;
    rndr::Span<math::Vector3> MeshBitangents;

    for (int i = 0; i < Faces.size(); i++)
    {
        const ObjFace& F = Faces[i];
        for (int j = 0; j < 3; j++)
        {
            MeshPositions[3 * i + j] = Positions[F.Positions[j] - 1];
            MeshTexCoords[3 * i + j] = TexCoords[F.TexCoords[j] - 1];
            MeshNormals[3 * i + j] = Normals[F.Normals[j] - 1];
            MeshIndices[3 * i + j] = 3 * i + j;
        }
    }

    rndr::Mesh* Mesh = new rndr::Mesh(Name, MeshPositions, MeshTexCoords, MeshNormals, MeshTangents, MeshBitangents, MeshIndices);
    return Mesh;
}
