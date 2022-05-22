#include "rndr/core/objparser.h"

#include <filesystem>

#include "rndr/core/log.h"
#include "rndr/core/math.h"
#include "rndr/core/mesh.h"

rndr::Mesh* rndr::ObjParser::Parse(const std::string& FilePath)
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

    Mesh* Mesh = Parse(FileContents);
    delete[] FileContents.Data;
    return Mesh;
}

static int GoToNextLine(const char* Data, int StartPosition)
{
    while (Data[StartPosition] != '\n' || Data[StartPosition] == EOF)
        StartPosition++;
    StartPosition++;
    return StartPosition;
}

rndr::Mesh* rndr::ObjParser::Parse(Span<char> Data)
{
    std::string MeshName;
    constexpr int DefaultSize = 100;
    std::vector<Point3r> VertexPositions;
    std::vector<Vector2r> VertexTexCoords;
    std::vector<Normal3r> VertexNormals;
    VertexPositions.reserve(DefaultSize);
    VertexTexCoords.reserve(DefaultSize);
    VertexNormals.reserve(DefaultSize);

    struct ObjFace
    {
        int Positions[3];
        int TexCoords[3];
        int Normals[3];
    };
    std::vector<ObjFace> Faces;
    Faces.reserve(DefaultSize);

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
            Vector2r TexCoords;
            int Result = sscanf(Data.Data + Position, "%f %f", &TexCoords.X, &TexCoords.Y);
            assert(Result == 2);
            VertexTexCoords.push_back(TexCoords);
            Position = GoToNextLine(Data.Data, Position);
        }
        // Vertex normal
        else if (strncmp(Data.Data + Position, "vn ", 3) == 0)
        {
            Position += 3;
            Normal3r Normal;
            int Result = sscanf(Data.Data + Position, "%f %f %f", &Normal.X, &Normal.Y, &Normal.Z);
            assert(Result == 3);
            VertexNormals.push_back(Normal);
            Position = GoToNextLine(Data.Data, Position);
        }
        // Vertex position
        else if (strncmp(Data.Data + Position, "v ", 2) == 0)
        {
            Position += 2;
            Point3r VertexPosition;
            int Result = sscanf(Data.Data + Position, "%f %f %f", &VertexPosition.X, &VertexPosition.Y, &VertexPosition.Z);
            assert(Result == 3);
            VertexPositions.push_back(VertexPosition);
            Position = GoToNextLine(Data.Data, Position);
        }
        // Face
        else if (strncmp(Data.Data + Position, "f ", 2) == 0)
        {
            Position += 2;
            ObjFace Face;
            int Result =
                sscanf(Data.Data + Position, "%d/%d/%d %d/%d/%d %d/%d/%d", &Face.Positions[0], &Face.TexCoords[0], &Face.Normals[0],
                       &Face.Positions[1], &Face.TexCoords[1], &Face.Normals[1], &Face.Positions[2], &Face.TexCoords[2], &Face.Normals[2]);
            assert(Result == 9);
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
            Position += 2;
            char Name[128] = {};
            int Result = sscanf(Data.Data + Position, "%s", Name);
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

    const int VertexCount = Faces.size() * 3;
    Span<Point3r> Positions(new Point3r[VertexCount], VertexCount);
    Span<Vector2r> TexCoords(new Vector2r[VertexCount], VertexCount);
    Span<Normal3r> Normals(new Normal3r[VertexCount], VertexCount);
    Span<int> Indices(new int[VertexCount], VertexCount);

    for (int i = 0; i < Faces.size(); i++)
    {
        ObjFace& F = Faces[i];
        for (int j = 0; j < 3; j++)
        {
            Positions[3 * i + j] = VertexPositions[F.Positions[j] - 1];
            TexCoords[3 * i + j] = VertexTexCoords[F.TexCoords[j] - 1];
            Normals[3 * i + j] = VertexNormals[F.Normals[j] - 1];
            Indices[3 * i + j] = 3*i + j;
        }
    }

    Mesh* M = new Mesh(MeshName, Positions, TexCoords, Normals, Span<Vector3r>{}, Span<Vector3r>{}, Indices);
    return M;
}
