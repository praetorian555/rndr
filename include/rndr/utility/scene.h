#pragma once

#include "rndr/core/base.h"
#include "rndr/core/containers/array.h"
#include "rndr/core/containers/stack-array.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/containers/hash-map.h"
#include "rndr/core/enum-flags.h"
#include "rndr/core/math.h"

namespace Rndr
{

using NodeId = uint32_t;

constexpr int32_t k_max_node_level = 16;

struct Hierarchy
{
    NodeId parent;
    NodeId first_child;
    NodeId next_sibling;
    NodeId last_sibling;
    int32_t level;
};

struct SceneDescription
{
    /** Transforms relative to the parent node. Transform of the root is relative to the world. */
    Array<Rndr::Matrix4x4f> local_transforms;

    /** Transforms relative to the world. */
    Array<Rndr::Matrix4x4f> world_transforms;

    /** List of nodes that are dirty and that need to recalculate their world transform. */
    Array<NodeId> dirty_nodes[k_max_node_level];

    /** Hierarchy of the nodes. */
    Array<Hierarchy> hierarchy;

    HashMap<NodeId, uint32_t> node_id_to_mesh_id;
    HashMap<NodeId, uint32_t> node_id_to_material_id;
    HashMap<NodeId, uint32_t> node_id_to_name;

    Array<String> node_names;
    Array<String> material_names;
};

}  // namespace Rndr
