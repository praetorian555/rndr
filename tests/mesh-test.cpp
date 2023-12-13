#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "rndr/utility/mesh.h"

TEST_CASE("Single mesh", "[mesh]")
{
    using namespace Rndr;
    MeshDescription mesh_description;
    mesh_description.material_id = 5;
    mesh_description.vertex_offset = 0;
    mesh_description.vertex_count = 4;
    mesh_description.vertex_size = 3 * sizeof(float);
    mesh_description.index_offset = 0;
    mesh_description.lod_count = 1;
    mesh_description.lod_offsets[0] = 0;
    mesh_description.lod_offsets[1] = 6;
    mesh_description.mesh_size = 6 * sizeof(uint32_t) + 4 * 3 * sizeof(float);

    MeshData mesh_data;
    mesh_data.meshes.emplace_back(mesh_description);
    mesh_data.vertex_buffer_data.resize(mesh_description.vertex_count * mesh_description.vertex_size);
    mesh_data.index_buffer_data.resize(mesh_description.GetLodIndicesCount(0) * sizeof(uint32_t));
    mesh_data.bounding_boxes.emplace_back(Point3f(0.0f, 0.0f, 0.0f), Point3f(1.0f, 1.0f, 1.0f));

    StackArray<float, 12> vertices = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    StackArray<uint32_t, 6> indices = {0, 1, 2, 2, 3, 0};
    memcpy(mesh_data.vertex_buffer_data.data(), vertices.data(), vertices.size() * sizeof(float));
    memcpy(mesh_data.index_buffer_data.data(), indices.data(), indices.size() * sizeof(uint32_t));

    const String file_path = "test.rndrmesh";
    CHECK(Mesh::WriteOptimizedData(mesh_data, file_path));

    MeshData out_mesh_data;
    CHECK(Mesh::ReadOptimizedData(out_mesh_data, file_path));

    CHECK(out_mesh_data.meshes.size() == 1);
    CHECK(out_mesh_data.meshes[0].material_id == 5);
    CHECK(out_mesh_data.meshes[0].vertex_offset == 0);
    CHECK(out_mesh_data.meshes[0].vertex_count == 4);
    CHECK(out_mesh_data.meshes[0].vertex_size == 3 * sizeof(float));
    CHECK(out_mesh_data.meshes[0].index_offset == 0);
    CHECK(out_mesh_data.meshes[0].lod_count == 1);
    CHECK(out_mesh_data.meshes[0].lod_offsets[0] == 0);
    CHECK(out_mesh_data.meshes[0].lod_offsets[1] == 6);
    CHECK(out_mesh_data.meshes[0].mesh_size == 6 * sizeof(uint32_t) + 4 * 3 * sizeof(float));

    CHECK(out_mesh_data.vertex_buffer_data.size() == mesh_data.vertex_buffer_data.size());
    CHECK(out_mesh_data.index_buffer_data.size() == mesh_data.index_buffer_data.size());
    CHECK(out_mesh_data.bounding_boxes.size() == mesh_data.bounding_boxes.size());

    CHECK(memcmp(out_mesh_data.vertex_buffer_data.data(), mesh_data.vertex_buffer_data.data(), mesh_data.vertex_buffer_data.size()) == 0);
    CHECK(memcmp(out_mesh_data.index_buffer_data.data(), mesh_data.index_buffer_data.data(), mesh_data.index_buffer_data.size()) == 0);
    CHECK(out_mesh_data.bounding_boxes[0] == mesh_data.bounding_boxes[0]);

    if (std::filesystem::exists(file_path))
    {
        std::filesystem::remove(file_path);
    }
}

TEST_CASE("Single mesh multiple lods", "[mesh]")
{
    using namespace Rndr;
    MeshDescription mesh_description;
    mesh_description.material_id = 5;
    mesh_description.vertex_offset = 0;
    mesh_description.vertex_count = 4;
    mesh_description.vertex_size = 3 * sizeof(float);
    mesh_description.index_offset = 0;
    mesh_description.lod_count = 2;
    mesh_description.lod_offsets[0] = 0;
    mesh_description.lod_offsets[1] = 6;
    mesh_description.lod_offsets[2] = 12;
    mesh_description.mesh_size = 12 * sizeof(uint32_t) + 4 * 3 * sizeof(float);

    MeshData mesh_data;
    mesh_data.meshes.emplace_back(mesh_description);
    mesh_data.vertex_buffer_data.resize(mesh_description.vertex_count * mesh_description.vertex_size);
    mesh_data.index_buffer_data.resize(mesh_description.GetLodIndicesCount(0) * sizeof(uint32_t) +
                                       mesh_description.GetLodIndicesCount(1) * sizeof(uint32_t));
    mesh_data.bounding_boxes.emplace_back(Point3f(0.0f, 0.0f, 0.0f), Point3f(1.0f, 1.0f, 1.0f));

    StackArray<float, 12> vertices = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f};
    StackArray<uint32_t, 12> indices = {0, 1, 2, 2, 3, 0, 0, 1, 2, 2, 3, 0};
    memcpy(mesh_data.vertex_buffer_data.data(), vertices.data(), vertices.size() * sizeof(float));
    memcpy(mesh_data.index_buffer_data.data(), indices.data(), indices.size() * sizeof(uint32_t));

    const String file_path = "test.rndrmesh";
    CHECK(Mesh::WriteOptimizedData(mesh_data, file_path));

    MeshData out_mesh_data;
    CHECK(Mesh::ReadOptimizedData(out_mesh_data, file_path));

    CHECK(out_mesh_data.meshes.size() == 1);
    CHECK(out_mesh_data.meshes[0].material_id == 5);
    CHECK(out_mesh_data.meshes[0].vertex_offset == 0);
    CHECK(out_mesh_data.meshes[0].vertex_count == 4);
    CHECK(out_mesh_data.meshes[0].vertex_size == 3 * sizeof(float));
    CHECK(out_mesh_data.meshes[0].index_offset == 0);
    CHECK(out_mesh_data.meshes[0].lod_count == 2);
    CHECK(out_mesh_data.meshes[0].lod_offsets[0] == 0);
    CHECK(out_mesh_data.meshes[0].lod_offsets[1] == 6);
    CHECK(out_mesh_data.meshes[0].lod_offsets[2] == 12);
    CHECK(out_mesh_data.meshes[0].mesh_size == 12 * sizeof(uint32_t) + 4 * 3 * sizeof(float));

    CHECK(out_mesh_data.vertex_buffer_data.size() == mesh_data.vertex_buffer_data.size());
    CHECK(out_mesh_data.index_buffer_data.size() == mesh_data.index_buffer_data.size());
    CHECK(out_mesh_data.bounding_boxes.size() == mesh_data.bounding_boxes.size());

    CHECK(memcmp(out_mesh_data.vertex_buffer_data.data(), mesh_data.vertex_buffer_data.data(), mesh_data.vertex_buffer_data.size()) == 0);
    CHECK(memcmp(out_mesh_data.index_buffer_data.data(), mesh_data.index_buffer_data.data(), mesh_data.index_buffer_data.size()) == 0);
    CHECK(out_mesh_data.bounding_boxes[0] == mesh_data.bounding_boxes[0]);

    if (std::filesystem::exists(file_path))
    {
        std::filesystem::remove(file_path);
    }
}

TEST_CASE("Multiple meshes", "[mesh]")
{
    using namespace Rndr;
    MeshDescription mesh_description_1;
    mesh_description_1.material_id = 5;
    mesh_description_1.vertex_offset = 0;
    mesh_description_1.vertex_count = 4;
    mesh_description_1.vertex_size = 3 * sizeof(float);
    mesh_description_1.index_offset = 0;
    mesh_description_1.lod_count = 1;
    mesh_description_1.lod_offsets[0] = 0;
    mesh_description_1.lod_offsets[1] = 6;
    mesh_description_1.mesh_size = 6 * sizeof(uint32_t) + 4 * 3 * sizeof(float);

    MeshDescription mesh_description_2;
    mesh_description_2.material_id = 8;
    mesh_description_2.vertex_offset = 4;
    mesh_description_2.vertex_count = 4;
    mesh_description_2.vertex_size = 3 * sizeof(float);
    mesh_description_2.index_offset = 6;
    mesh_description_2.lod_count = 1;
    mesh_description_2.lod_offsets[0] = 0;
    mesh_description_2.lod_offsets[1] = 6;
    mesh_description_2.mesh_size = 6 * sizeof(uint32_t) + 4 * 3 * sizeof(float);

    MeshData mesh_data;
    mesh_data.meshes.emplace_back(mesh_description_1);
    mesh_data.meshes.emplace_back(mesh_description_2);
    mesh_data.vertex_buffer_data.resize(mesh_description_1.vertex_count * mesh_description_1.vertex_size +
                                        mesh_description_2.vertex_count * mesh_description_2.vertex_size);
    mesh_data.index_buffer_data.resize(mesh_description_1.GetLodIndicesCount(0) * sizeof(uint32_t) +
                                       mesh_description_2.GetLodIndicesCount(0) * sizeof(uint32_t));
    mesh_data.bounding_boxes.emplace_back(Point3f(0.0f, 0.0f, 0.0f), Point3f(1.0f, 1.0f, 1.0f));
    mesh_data.bounding_boxes.emplace_back(Point3f(1.0f, 1.0f, 1.0f), Point3f(2.0f, 2.0f, 2.0f));

    StackArray<float, 24> vertices = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                      1.0f, 1.0f, 1.0f, 2.0f, 1.0f, 1.0f, 2.0f, 2.0f, 1.0f, 1.0f, 2.0f, 1.0f};
    StackArray<uint32_t, 12> indices = {0, 1, 2, 2, 3, 0, 0, 1, 2, 2, 3, 0};
    memcpy(mesh_data.vertex_buffer_data.data(), vertices.data(), vertices.size() * sizeof(float));
    memcpy(mesh_data.index_buffer_data.data(), indices.data(), indices.size() * sizeof(uint32_t));

    const String file_path = "test.rndrmesh";
    CHECK(Mesh::WriteOptimizedData(mesh_data, file_path));

    MeshData out_mesh_data;
    CHECK(Mesh::ReadOptimizedData(out_mesh_data, file_path));

    CHECK(out_mesh_data.meshes.size() == 2);
    CHECK(out_mesh_data.meshes[0].material_id == 5);
    CHECK(out_mesh_data.meshes[0].vertex_offset == 0);
    CHECK(out_mesh_data.meshes[0].vertex_count == 4);
    CHECK(out_mesh_data.meshes[0].vertex_size == 3 * sizeof(float));
    CHECK(out_mesh_data.meshes[0].index_offset == 0);
    CHECK(out_mesh_data.meshes[0].lod_count == 1);
    CHECK(out_mesh_data.meshes[0].lod_offsets[0] == 0);
    CHECK(out_mesh_data.meshes[0].lod_offsets[1] == 6);
    CHECK(out_mesh_data.meshes[0].mesh_size == 6 * sizeof(uint32_t) + 4 * 3 * sizeof(float));

    CHECK(out_mesh_data.meshes[1].material_id == 8);
    CHECK(out_mesh_data.meshes[1].vertex_offset == 4);
    CHECK(out_mesh_data.meshes[1].vertex_count == 4);
    CHECK(out_mesh_data.meshes[1].vertex_size == 3 * sizeof(float));
    CHECK(out_mesh_data.meshes[1].index_offset == 6);
    CHECK(out_mesh_data.meshes[1].lod_count == 1);
    CHECK(out_mesh_data.meshes[1].lod_offsets[0] == 0);
    CHECK(out_mesh_data.meshes[1].lod_offsets[1] == 6);
    CHECK(out_mesh_data.meshes[1].mesh_size == 6 * sizeof(uint32_t) + 4 * 3 * sizeof(float));

    CHECK(out_mesh_data.vertex_buffer_data.size() == mesh_data.vertex_buffer_data.size());
    CHECK(out_mesh_data.index_buffer_data.size() == mesh_data.index_buffer_data.size());
    CHECK(out_mesh_data.bounding_boxes.size() == mesh_data.bounding_boxes.size());

    CHECK(memcmp(out_mesh_data.vertex_buffer_data.data(), mesh_data.vertex_buffer_data.data(), mesh_data.vertex_buffer_data.size()) == 0);
    CHECK(memcmp(out_mesh_data.index_buffer_data.data(), mesh_data.index_buffer_data.data(), mesh_data.index_buffer_data.size()) == 0);
    CHECK(out_mesh_data.bounding_boxes[0] == mesh_data.bounding_boxes[0]);
    CHECK(out_mesh_data.bounding_boxes[1] == mesh_data.bounding_boxes[1]);

    if (std::filesystem::exists(file_path))
    {
        std::filesystem::remove(file_path);
    }
}

TEST_CASE("Calculate bounding box", "[mesh]")
{
    using namespace Rndr;
    MeshDescription mesh_description;
    mesh_description.material_id = 5;
    mesh_description.vertex_offset = 0;
    mesh_description.vertex_count = 4;
    mesh_description.vertex_size = 3 * sizeof(float);
    mesh_description.index_offset = 0;
    mesh_description.lod_count = 1;
    mesh_description.lod_offsets[0] = 0;
    mesh_description.lod_offsets[1] = 6;
    mesh_description.mesh_size = 6 * sizeof(uint32_t) + 4 * 3 * sizeof(float);

    MeshData mesh_data;
    mesh_data.meshes.emplace_back(mesh_description);
    mesh_data.vertex_buffer_data.resize(mesh_description.vertex_count * mesh_description.vertex_size);
    mesh_data.index_buffer_data.resize(mesh_description.GetLodIndicesCount(0) * sizeof(uint32_t));
    mesh_data.bounding_boxes.emplace_back(Point3f(0.0f, 0.0f, 0.0f), Point3f(1.0f, 1.0f, 1.0f));

    StackArray<float, 12> vertices = {0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f, -1.0f, -2.0f, -3.0f, 1.0f, 1.0f, 1.0f};
    StackArray<uint32_t, 6> indices = {0, 1, 2, 2, 3, 0};
    memcpy(mesh_data.vertex_buffer_data.data(), vertices.data(), vertices.size() * sizeof(float));
    memcpy(mesh_data.index_buffer_data.data(), indices.data(), indices.size() * sizeof(uint32_t));

    Mesh::UpdateBoundingBoxes(mesh_data);

    CHECK(mesh_data.bounding_boxes[0] == Bounds3f(Point3f(-1.0f, -2.0f, -3.0f), Point3f(1.0f, 2.0f, 3.0f)));
}
