#include "rndr/geometry/cube.h"

rndr::Span<rndr::Point3r> rndr::Cube::GetVertexPositions()
{
    // clang-format off
    static std::vector<rndr::Point3r> s_CubePositions
    {
        // Front face
        {-0.5, -0.5, 0.5},
        { 0.5, -0.5, 0.5},
        { 0.5,  0.5, 0.5},
        {-0.5,  0.5, 0.5},

        // Back face
        { 0.5, -0.5, -0.5},
        {-0.5, -0.5, -0.5},
        {-0.5,  0.5, -0.5},
        { 0.5,  0.5, -0.5},

        // Bottom face
        {-0.5, -0.5, -0.5},
        { 0.5, -0.5, -0.5},
        { 0.5, -0.5,  0.5},
        {-0.5, -0.5,  0.5},

        // Top face
        {-0.5,  0.5,  0.5},
        { 0.5,  0.5,  0.5},
        { 0.5,  0.5, -0.5},
        {-0.5,  0.5, -0.5},

        // Left face
        {-0.5, -0.5, -0.5},
        {-0.5, -0.5,  0.5},
        {-0.5,  0.5,  0.5},
        {-0.5,  0.5, -0.5},

        // Right face
        { 0.5, -0.5,  0.5},
        { 0.5, -0.5, -0.5},
        { 0.5,  0.5, -0.5},
        { 0.5,  0.5,  0.5},
    };
    // clang-format on

    return s_CubePositions;
}

rndr::Span<rndr::Point2r> rndr::Cube::GetVertexTextureCoordinates()
{
    // clang-format off
    static std::vector<rndr::Point2r> s_CubeTexCoords
    {
        // Front face
        {0, 0},
        {1, 0},
        {1, 1},
        {0, 1},

        // Back face
        {0, 0},
        {1, 0},
        {1, 1},
        {0, 1},

        // Bottom face
        {0, 0},
        {1, 0},
        {1, 1},
        {0, 1},

        // Top face
        {0, 0},
        {1, 0},
        {1, 1},
        {0, 1},

        // Left face
        {0, 0},
        {1, 0},
        {1, 1},
        {0, 1},

        // Right face
        {0, 0},
        {1, 0},
        {1, 1},
        {0, 1},
    };
    // clang-format on

    return s_CubeTexCoords;
}

rndr::Span<rndr::Normal3r> rndr::Cube::GetNormals()
{
    // clang-format off
    static std::vector<rndr::Normal3r> s_CubeNormals
    {
        // Front face
        {0, 0, 1},
        {0, 0, 1},
        {0, 0, 1},
        {0, 0, 1},

        // Back face
        {0, 0, -1},
        {0, 0, -1},
        {0, 0, -1},
        {0, 0, -1},

        // Bottom face
        {0, -1, 0},
        {0, -1, 0},
        {0, -1, 0},
        {0, -1, 0},

        // Top face
        {0, 1, 0},
        {0, 1, 0},
        {0, 1, 0},
        {0, 1, 0},

        // Left face
        {-1, 0, 0},
        {-1, 0, 0},
        {-1, 0, 0},
        {-1, 0, 0},

        // Right face
        {1, 0, 0},
        {1, 0, 0},
        {1, 0, 0},
        {1, 0, 0},
    };
    // clang-format on

    return s_CubeNormals;
}

rndr::IntSpan rndr::Cube::GetIndices()
{
    // clang-format off
    static std::vector<int> s_CubeIndices
    {
        0, 1, 3,
        1, 2, 3,

        4, 5, 7,
        5, 6, 7,

        8,  9, 11,
        9, 10, 11,

        12, 13, 15,
        13, 14, 15,

        16, 17, 19,
        17, 18, 19,

        20, 21, 23,
        21, 22, 23
    };
    // clang-format on

    return s_CubeIndices;
}