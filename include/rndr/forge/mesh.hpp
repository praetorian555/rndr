#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/types.hpp"

namespace Rndr::Forge
{

/**
 * CPU-side mesh data: packed vertex and index buffers ready to be uploaded to a GPU buffer.
 *
 * Vertex layout produced by LoadMesh is fixed and tightly packed:
 *   - position : float3
 *   - normal   : float3
 *   - uv       : float2
 * Indices are 32-bit unsigned integers.
 */
struct Mesh
{
    Opal::StringUtf8 name;
    Opal::DynamicArray<u8> vertices;
    Opal::DynamicArray<u8> indices;
    u32 vertex_size = 0;
    u32 vertex_count = 0;
    u32 index_size = 0;
    u32 index_count = 0;
};

/**
 * Load mesh data from a file using assimp. Only the first mesh in the imported scene is read.
 * Vertices are packed as position (float3), normal (float3), uv (float2). Indices are 32-bit.
 *
 * @param file_path Absolute or relative path to the mesh file.
 * @param out_mesh Output mesh with vertex and index data populated.
 * @throw Opal::Exception if the file cannot be loaded or required vertex attributes are missing.
 */
void LoadMesh(const Opal::StringUtf8& file_path, Mesh& out_mesh);

}  // namespace Rndr::Forge