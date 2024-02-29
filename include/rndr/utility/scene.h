#pragma once

#include "rndr/core/base.h"
#include "rndr/core/containers/array.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/containers/hash-map.h"
#include "rndr/core/enum-flags.h"
#include "rndr/core/math.h"
#include "rndr/utility/mesh.h"
#include "rndr/utility/material.h"

namespace Rndr
{

using NodeId = uint32_t;

constexpr int32_t k_max_node_level = 16;

struct HierarchyNode
{
    NodeId parent;
    NodeId first_child;
    NodeId next_sibling;
    NodeId last_sibling;
    int32_t level;
};

/**
 * Describes the scene organization and the transforms of the nodes.
 */
struct SceneDescription
{
    /** Transforms relative to the parent node. Transform of the root is relative to the world. */
    Array<Rndr::Matrix4x4f> local_transforms;

    /** Transforms relative to the world. */
    Array<Rndr::Matrix4x4f> world_transforms;

    /** List of nodes that are dirty and that need to recalculate their world transform. */
    Array<NodeId> dirty_nodes[k_max_node_level];

    /** Hierarchy of the nodes. */
    Array<HierarchyNode> hierarchy;

    HashMap<NodeId, uint32_t> node_id_to_mesh_id;
    HashMap<NodeId, uint32_t> node_id_to_material_id;
    HashMap<NodeId, uint32_t> node_id_to_name;

    Array<String> node_names;
    Array<String> material_names;
};

/**
 * Groups all the data needed to draw a scene.
 */
struct SceneDrawData
{
    MeshData mesh_data;
    Array<MeshDrawData> shapes;

    Array<MaterialDescription> materials;
    Array<Image> textures;

    SceneDescription scene_description;
};

namespace Scene
{

/**
 * Loads a scene description from a file.
 * @param out_scene_description The scene description to fill.
 * @param scene_file The file to load the scene description from.
 * @return True if the scene description was successfully loaded, false otherwise.
 */
bool ReadSceneDescription(SceneDescription& out_scene_description, const char* scene_file);

/**
 * Loads a scene draw data from a file.
 * @param out_scene The scene draw data to fill.
 * @param scene_file The file to load the scene description from.
 * @param mesh_file The file to load the mesh data from.
 * @param material_file The file to load the material data from.
 * @return True if the scene draw data was successfully loaded, false otherwise.
 */
bool ReadScene(SceneDrawData& out_scene, const char* scene_file, const char* mesh_file, const char* material_file);

/**
 * Writes a scene description to a file.
 * @param scene_description The scene description to write.
 * @param scene_file The file to write the scene description to.
 * @return True if the scene description was successfully written, false otherwise.
 */
bool WriteSceneDescription(const SceneDescription& scene_description, const char* scene_file);

/**
 * Writes a scene draw data to a file.
 * @param scene The scene draw data to write.
 * @param scene_file The file to write the scene description to.
 * @param mesh_file The file to write the mesh data to.
 * @param material_file The file to write the material data to.
 * @return True if the scene draw data was successfully written, false otherwise.
 */
bool WriteScene(const SceneDrawData& scene, const char* scene_file, const char* mesh_file, const char* material_file);

}  // namespace Scene

}  // namespace Rndr
