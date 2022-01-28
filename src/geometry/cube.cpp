#include "rndr/geometry/cube.h"

const std::vector<rndr::CubeVertexData>& rndr::Cube::GetVertices()
{
    // clang-format off
    static std::vector<CubeVertexData> s_CubeVertices
    {
        // Front face
        {{-0.5, -0.5, 0.5}, {0, 0}},
        {{ 0.5, -0.5, 0.5}, {1, 0}},
        {{ 0.5,  0.5, 0.5}, {1, 1}},
        {{-0.5,  0.5, 0.5}, {0, 1}},

        // Back face
        {{ 0.5, -0.5, -0.5}, {0, 0}},
        {{-0.5, -0.5, -0.5}, {1, 0}},
        {{-0.5,  0.5, -0.5}, {1, 1}},
        {{ 0.5,  0.5, -0.5}, {0, 1}},

        // Bottom face
        {{-0.5, -0.5, -0.5}, {0, 0}},
        {{ 0.5, -0.5, -0.5}, {1, 0}},
        {{ 0.5, -0.5,  0.5}, {1, 1}},
        {{-0.5, -0.5,  0.5}, {0, 1}},

        // Top face
        {{-0.5,  0.5,  0.5}, {0, 0}},
        {{ 0.5,  0.5,  0.5}, {1, 0}},
        {{ 0.5,  0.5, -0.5}, {1, 1}},
        {{-0.5,  0.5, -0.5}, {0, 1}},

        // Left face
        {{-0.5, -0.5, -0.5}, {0, 0}},
        {{-0.5, -0.5,  0.5}, {1, 0}},
        {{-0.5,  0.5,  0.5}, {1, 1}},
        {{-0.5,  0.5, -0.5}, {0, 1}},

        // Right face
        {{ 0.5, -0.5,  0.5}, {0, 0}},
        {{ 0.5, -0.5, -0.5}, {1, 0}},
        {{ 0.5,  0.5, -0.5}, {1, 1}},
        {{ 0.5,  0.5,  0.5}, {0, 1}},
    };
    // clang-format on

    return s_CubeVertices;
}

const std::vector<int>& rndr::Cube::GetIndices()
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