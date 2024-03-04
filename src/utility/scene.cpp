#include "rndr/utility/scene.h"

#include <stack>

#include "rndr/core/file.h"

namespace
{
bool WriteMap(Rndr::FileHandler& file, const Rndr::HashMap<Rndr::Scene::NodeId, uint32_t>& map)
{
    Rndr::Array<uint32_t> flattened_map;
    flattened_map.reserve(map.size() * 2);

    for (const auto& pair : map)
    {
        flattened_map.push_back(pair.first);
        flattened_map.push_back(pair.second);
    }

    const uint32_t flattened_map_size = static_cast<uint32_t>(flattened_map.size());
    file.Write(&flattened_map_size, sizeof(uint32_t), 1);
    file.Write(flattened_map.data(), sizeof(uint32_t), flattened_map.size());
    return true;
}

bool ReadMap(Rndr::FileHandler& file, Rndr::HashMap<Rndr::Scene::NodeId, uint32_t>& map)
{
    uint32_t flattened_map_size = 0;
    file.Read(&flattened_map_size, sizeof(uint32_t), 1);
    Rndr::Array<uint32_t> flattened_map(flattened_map_size);
    file.Read(flattened_map.data(), sizeof(uint32_t), flattened_map.size());

    for (uint32_t i = 0; i < flattened_map_size; i += 2)
    {
        map[flattened_map[i]] = flattened_map[i + 1];
    }

    return true;
}

bool WriteStringList(Rndr::FileHandler& file, const Rndr::Array<Rndr::String>& strings)
{
    const uint32_t string_count = static_cast<uint32_t>(strings.size());
    file.Write(&string_count, sizeof(uint32_t), 1);
    for (const auto& string : strings)
    {
        const uint32_t string_length = static_cast<uint32_t>(string.size());
        file.Write(&string_length, sizeof(uint32_t), 1);
        file.Write(string.c_str(), string_length + 1, 1);
    }
    return true;
}

bool ReadStringList(Rndr::FileHandler& file, Rndr::Array<Rndr::String>& strings)
{
    uint32_t string_count = 0;
    file.Read(&string_count, sizeof(uint32_t), 1);
    strings.resize(string_count);
    for (auto& string : strings)
    {
        uint32_t string_length = 0;
        file.Read(&string_length, sizeof(uint32_t), 1);
        Rndr::Array<char> in_bytes(string_length + 1);
        file.Read(in_bytes.data(), string_length + 1, 1);
        string = Rndr::String(in_bytes.data());
    }
    return true;
}

};  // namespace

bool Rndr::Scene::ReadSceneDescription(SceneDescription& out_scene_description, const String& scene_file)
{
    Rndr::FileHandler file(scene_file.c_str(), "rb");
    if (!file.IsValid())
    {
        return false;
    }

    uint32_t node_count = 0;
    file.Read(&node_count, sizeof(uint32_t), 1);

    out_scene_description.local_transforms.resize(node_count);
    out_scene_description.world_transforms.resize(node_count);
    out_scene_description.hierarchy.resize(node_count);

    file.Read(out_scene_description.local_transforms.data(), sizeof(Rndr::Matrix4x4f), node_count);
    file.Read(out_scene_description.world_transforms.data(), sizeof(Rndr::Matrix4x4f), node_count);
    file.Read(out_scene_description.hierarchy.data(), sizeof(Rndr::Scene::HierarchyNode), node_count);

    ReadMap(file, out_scene_description.node_id_to_mesh_id);
    ReadMap(file, out_scene_description.node_id_to_material_id);

    if (!file.IsEOF())
    {
        ReadMap(file, out_scene_description.node_id_to_name);
        ReadStringList(file, out_scene_description.node_names);
        ReadStringList(file, out_scene_description.material_names);
    }

    return true;
}

bool Rndr::Scene::WriteSceneDescription(const Rndr::SceneDescription& scene_description, const String& scene_file)
{
    Rndr::FileHandler file(scene_file.c_str(), "wb");
    if (!file.IsValid())
    {
        return false;
    }

    const uint32_t node_count = static_cast<uint32_t>(scene_description.hierarchy.size());
    file.Write(&node_count, sizeof(uint32_t), 1);

    file.Write(scene_description.local_transforms.data(), sizeof(Rndr::Matrix4x4f), node_count);
    file.Write(scene_description.world_transforms.data(), sizeof(Rndr::Matrix4x4f), node_count);
    file.Write(scene_description.hierarchy.data(), sizeof(Rndr::Scene::HierarchyNode), node_count);

    WriteMap(file, scene_description.node_id_to_mesh_id);
    WriteMap(file, scene_description.node_id_to_material_id);

    if (!scene_description.node_id_to_name.empty() && !scene_description.node_names.empty())
    {
        WriteMap(file, scene_description.node_id_to_name);
        WriteStringList(file, scene_description.node_names);
        WriteStringList(file, scene_description.material_names);
    }

    return true;
}

Rndr::Scene::NodeId Rndr::Scene::AddNode(Rndr::SceneDescription& scene, int32_t parent, int32_t level)
{
    const NodeId node_id = static_cast<uint32_t>(scene.hierarchy.size());
    scene.local_transforms.emplace_back(Rndr::Matrix4x4f(1.0f));
    scene.world_transforms.emplace_back(Rndr::Matrix4x4f(1.0f));
    scene.hierarchy.emplace_back(HierarchyNode{.parent = parent, .last_sibling = -1, .level = level});

    if (parent > -1)
    {
        NodeId parent_first_child = scene.hierarchy[parent].first_child;
        if (parent_first_child == k_invalid_node_id)
        {
            scene.hierarchy[parent].first_child = node_id;
            scene.hierarchy[node_id].last_sibling = node_id;
        }
        else
        {
            NodeId last_sibling = scene.hierarchy[parent_first_child].last_sibling;
            if (last_sibling == k_invalid_node_id)
            {
                for (last_sibling = parent_first_child; scene.hierarchy[last_sibling].next_sibling != k_invalid_node_id;
                     last_sibling = scene.hierarchy[last_sibling].next_sibling)
                {
                }
            }
            scene.hierarchy[last_sibling].next_sibling = node_id;
            scene.hierarchy[parent_first_child].last_sibling = node_id;
        }
    }

    scene.hierarchy[node_id].level = level;

    return 0;
}

void Rndr::Scene::SetNodeName(Rndr::SceneDescription& scene, Rndr::Scene::NodeId node, const String& name)
{
    RNDR_ASSERT(IsValidNodeId(scene, node));
    scene.node_id_to_name[node] = static_cast<uint32_t>(scene.node_names.size());
    scene.node_names.emplace_back(name);
}

bool Rndr::Scene::IsValidNodeId(const Rndr::SceneDescription& scene, Rndr::Scene::NodeId node)
{
    return node < scene.hierarchy.size();
}

void Rndr::Scene::SetNodeMeshId(Rndr::SceneDescription& scene, Rndr::Scene::NodeId node, uint32_t mesh_id)
{
    RNDR_ASSERT(IsValidNodeId(scene, node));
    scene.node_id_to_mesh_id[node] = mesh_id;
}

void Rndr::Scene::SetNodeMaterialId(Rndr::SceneDescription& scene, Rndr::Scene::NodeId node, uint32_t material_id)
{
    RNDR_ASSERT(IsValidNodeId(scene, node));
    scene.node_id_to_material_id[node] = material_id;
}

void Rndr::Scene::MarkAsChanged(Rndr::SceneDescription& scene, Rndr::Scene::NodeId node)
{
    std::stack<Rndr::Scene::NodeId> stack;
    stack.push(node);

    while (!stack.empty())
    {
        const NodeId node_to_mark = stack.top();
        RNDR_ASSERT(IsValidNodeId(scene, node_to_mark));

        const int32_t level = scene.hierarchy[node_to_mark].level;
        scene.dirty_nodes[level].push_back(node_to_mark);

        for (NodeId child = scene.hierarchy[node].first_child; child != k_invalid_node_id; child = scene.hierarchy[child].next_sibling)
        {
            stack.push(child);
        }
    }
}

void Rndr::Scene::RecalculateWorldTransforms(Rndr::SceneDescription& scene)
{
    // Process root level first
    if (!scene.dirty_nodes[0].empty())
    {
        const NodeId root_node = scene.dirty_nodes[0].back();
        scene.world_transforms[root_node] = scene.local_transforms[root_node];
        scene.dirty_nodes[0].clear();
    }

    for (int i = 1; i < k_max_node_level && !scene.dirty_nodes[i].empty(); ++i)
    {
        for (const NodeId node : scene.dirty_nodes[i])
        {
            const NodeId parent = scene.hierarchy[node].parent;
            scene.world_transforms[node] = scene.world_transforms[parent] * scene.local_transforms[node];
        }
        scene.dirty_nodes[i].clear();
    }
}
