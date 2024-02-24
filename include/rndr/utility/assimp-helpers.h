#pragma once

#include "rndr/core/base.h"
#include "rndr/utility/mesh.h"

struct aiMaterial;

namespace Rndr
{

struct MaterialDescription;

namespace AssimpHelpers
{

/**
 * Reads mesh data from the Assimp scene.
 * @param out_mesh_data Destination mesh data.
 * @param ai_scene Assimp scene.
 * @param attributes_to_load Attributes to load from the Assimp scene.
 * @return True if mesh data was read successfully, false otherwise.
 */
bool ReadMeshData(MeshData& out_mesh_data, const aiScene& ai_scene,
                  MeshAttributesToLoad attributes_to_load = MeshAttributesToLoad::LoadPositions);

/**
 * Reads mesh data from the specified file.
 * @param out_mesh_data Destination mesh data.
 * @param mesh_file_path Path to the mesh file.
 * @param attributes_to_load Attributes to load from the mesh file.
 * @return True if mesh data was read successfully, false otherwise.
 */
bool ReadMeshData(MeshData& out_mesh_data, const String& mesh_file_path,
                  MeshAttributesToLoad attributes_to_load = MeshAttributesToLoad::LoadPositions);

/**
 * Reads material description from the Assimp material.
 * @param out_description Material description to be filled. Note that texture maps will not contain the handles to actual texture data on
 * the GPU but rather index of a texture path in the array of texture paths.
 * @param out_texture_paths Array of texture paths. The indices of the texture paths in this array are stored in the material description.
 * @param out_opacity_maps Array of paths to opacity maps.
 * @param ai_material Assimp material to read from.
 * @return True if the material description was successfully read, false otherwise.
 */
bool ReadMaterialDescription(MaterialDescription& out_description, Array<String>& out_texture_paths, Array<String>& out_opacity_maps,
                             const aiMaterial& ai_material);

}  // namespace AssimpHelpers
}  // namespace Rndr