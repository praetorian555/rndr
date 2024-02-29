#include "rndr/utility/scene.h"

#include "rndr/core/file.h"

namespace
{
bool WriteMap(Rndr::FileHandler& file, const Rndr::HashMap<uint32_t, uint32_t>& map)
{
    Rndr::Array<uint32_t> flattened_map;
    flattened_map.reserve(map.size() * 2);

    for (const auto& pair : map)
    {
        flattened_map.push_back(pair.first);
        flattened_map.push_back(pair.second);
    }

    const uint32_t flattened_map_size = flattened_map.size();
    file.Write(&flattened_map_size, sizeof(uint32_t), 1);
    file.Write(flattened_map.data(), sizeof(uint32_t), flattened_map.size());
    return true;
}

bool ReadMap(Rndr::FileHandler& file, Rndr::HashMap<uint32_t, uint32_t>& map)
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
    const uint32_t string_count = strings.size();
    file.Write(&string_count, sizeof(uint32_t), 1);
    for (const auto& string : strings)
    {
        const uint32_t string_length = string.size();
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

bool Rndr::Scene::ReadSceneDescription(SceneDescription& out_scene_description, const char* scene_file)
{
    Rndr::FileHandler file(scene_file, "rb");
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
    file.Read(out_scene_description.hierarchy.data(), sizeof(Rndr::HierarchyNode), node_count);

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

bool Rndr::Scene::WriteSceneDescription(const Rndr::SceneDescription& scene_description, const char* scene_file)
{
    Rndr::FileHandler file(scene_file, "wb");
    if (!file.IsValid())
    {
        return false;
    }

    const uint32_t node_count = scene_description.hierarchy.size();
    file.Write(&node_count, sizeof(uint32_t), 1);

    file.Write(scene_description.local_transforms.data(), sizeof(Rndr::Matrix4x4f), node_count);
    file.Write(scene_description.world_transforms.data(), sizeof(Rndr::Matrix4x4f), node_count);
    file.Write(scene_description.hierarchy.data(), sizeof(Rndr::HierarchyNode), node_count);

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
