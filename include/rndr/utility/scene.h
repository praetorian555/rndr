#pragma once

#include "rndr/core/base.h"
#include "rndr/core/containers/array.h"
#include "rndr/core/containers/hash-map.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/enum-flags.h"
#include "rndr/core/math.h"
#include "rndr/utility/material.h"
#include "rndr/utility/mesh.h"

namespace Rndr
{

namespace Scene
{
using NodeId = int32_t;

constexpr int32_t k_max_node_level = 16;
constexpr NodeId k_invalid_node_id = -1;

struct HierarchyNode
{
    /** Parent node id or -1 if this is a root node. */
    NodeId parent = k_invalid_node_id;

    /** First child node id or -1 if this node has no children. */
    NodeId first_child = k_invalid_node_id;

    /** Next sibling node id or -1 if this node has no siblings left. */
    NodeId next_sibling = k_invalid_node_id;

    NodeId last_sibling = k_invalid_node_id;

    /** Level of the node in the hierarchy. Root node is at level 0. */
    int32_t level = 0;
};
}  // namespace Scene

/**
 * Describes the scene organization and the transforms of the nodes.
 */
struct SceneDescription
{
    /** Transforms relative to the parent node. Transform of the root is relative to the world. */
    Array<Rndr::Matrix4x4f> local_transforms;

    /** Transforms relative to the world. */
    Array<Rndr::Matrix4x4f> world_transforms;

    /** Hierarchy of the nodes. */
    Array<Scene::HierarchyNode> hierarchy;

    /** Maps node id to mesh id. */
    HashMap<Scene::NodeId, uint32_t> node_id_to_mesh_id;

    /** Maps node id to material id. */
    HashMap<Scene::NodeId, uint32_t> node_id_to_material_id;

    /** Maps node id to node name. */
    HashMap<Scene::NodeId, uint32_t> node_id_to_name;

    /** List of node names. */
    Array<String> node_names;

    /** List of material names. */
    Array<String> material_names;

    /** List of nodes that are dirty and that need to recalculate their world transform. */
    Array<Scene::NodeId> dirty_nodes[Scene::k_max_node_level];
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

/**
 * Adds a node to the scene.
 * @param scene The scene to add the node to.
 * @param parent The parent node id.
 * @param level The level of the node in the hierarchy.
 * @return The id of the new node.
 */
NodeId AddNode(SceneDescription& scene, NodeId parent, int32_t level);

/**
 * Set the debug name of a node.
 * @param scene The scene to set the node name in.
 * @param node The node id.
 * @param name The name to set.
 */
void SetNodeName(SceneDescription& scene, NodeId node, const String& name);

void SetNodeMeshId(SceneDescription& scene, NodeId node, uint32_t mesh_id);
void SetNodeMaterialId(SceneDescription& scene, NodeId node, uint32_t material_id);

/**
 * Check if a node id is valid in the given scene description.
 * @param scene The scene description to check the node id in.
 * @param node The node id to check.
 * @return True if the node id is valid, false otherwise.
 */
bool IsValidNodeId(const SceneDescription& scene, NodeId node);

}  // namespace Scene

}  // namespace Rndr
