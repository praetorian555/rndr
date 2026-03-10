#include <catch2/catch2.hpp>

#include "opal/container/scope-ptr.h"
#include "opal/exceptions.h"

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/mesh.hpp"
#include "rndr/exception.hpp"
#include "rndr/generic-window.hpp"

namespace
{

Rndr::Canvas::Context CreateTestContext(Opal::ScopePtr<Rndr::Application>& app, Opal::Ref<Rndr::GenericWindow>& window)
{
    app = Rndr::Application::Create();
    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    window = app->CreateGenericWindow(window_desc);
    return Rndr::Canvas::Context::Init(window->GetNativeHandle());
}

struct MeshTestFixture
{
    Opal::ScopePtr<Rndr::Application> app;
    Opal::Ref<Rndr::GenericWindow> window;
    Rndr::Canvas::Context context;

    MeshTestFixture() : context(CreateTestContext(app, window)) {}
};

Rndr::Canvas::VertexLayout MakePositionLayout()
{
    Rndr::Canvas::VertexLayout layout;
    layout.Add(Rndr::Canvas::Attrib::Position, Rndr::Canvas::Format::Float3);
    return layout;
}

Rndr::Canvas::VertexLayout MakePositionUVLayout()
{
    Rndr::Canvas::VertexLayout layout;
    layout.Add(Rndr::Canvas::Attrib::Position, Rndr::Canvas::Format::Float3);
    layout.Add(Rndr::Canvas::Attrib::UV, Rndr::Canvas::Format::Float2);
    return layout;
}

// Triangle: 3 vertices, position only (float3).
// clang-format off
const float k_triangle_positions[] = {
    0.0f,  0.5f, 0.0f,
   -0.5f, -0.5f, 0.0f,
    0.5f, -0.5f, 0.0f,
};
// clang-format on

const Rndr::u32 k_triangle_indices[] = {0, 1, 2};

// Quad: 4 vertices, position + uv.
// clang-format off
const float k_quad_data[] = {
    -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
     0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
     0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
    -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
};
// clang-format on

const Rndr::u32 k_quad_indices[] = {0, 1, 2, 2, 3, 0};

}  // namespace

TEST_CASE("Canvas Mesh", "[canvas][mesh]")
{
    MeshTestFixture const f;

    SECTION("Default constructed mesh is invalid")
    {
        Rndr::Canvas::Mesh const mesh;
        REQUIRE_FALSE(mesh.IsValid());
        REQUIRE(mesh.GetVertexCount() == 0);
        REQUIRE(mesh.GetIndexCount() == 0);
        REQUIRE_FALSE(mesh.HasIndices());
    }

    SECTION("Create non-indexed mesh")
    {
        Rndr::Canvas::VertexLayout layout = MakePositionLayout();
        const auto* raw = reinterpret_cast<const Rndr::u8*>(k_triangle_positions);
        const Rndr::u64 size = sizeof(k_triangle_positions);

        Rndr::Canvas::Mesh const mesh(layout, {raw, size});
        REQUIRE(mesh.IsValid());
        REQUIRE(mesh.GetNativeHandle() != 0);
        REQUIRE(mesh.GetVertexCount() == 3);
        REQUIRE(mesh.GetIndexCount() == 0);
        REQUIRE_FALSE(mesh.HasIndices());
    }

    SECTION("Create indexed mesh")
    {
        Rndr::Canvas::VertexLayout layout = MakePositionLayout();
        const auto* vraw = reinterpret_cast<const Rndr::u8*>(k_triangle_positions);
        const auto* iraw = reinterpret_cast<const Rndr::u8*>(k_triangle_indices);

        Rndr::Canvas::Mesh const mesh(layout, {vraw, sizeof(k_triangle_positions)}, {iraw, sizeof(k_triangle_indices)});
        REQUIRE(mesh.IsValid());
        REQUIRE(mesh.GetVertexCount() == 3);
        REQUIRE(mesh.GetIndexCount() == 3);
        REQUIRE(mesh.HasIndices());
    }

    SECTION("Create mesh with position + UV layout")
    {
        Rndr::Canvas::VertexLayout layout = MakePositionUVLayout();
        const auto* vraw = reinterpret_cast<const Rndr::u8*>(k_quad_data);
        const auto* iraw = reinterpret_cast<const Rndr::u8*>(k_quad_indices);

        Rndr::Canvas::Mesh const mesh(layout, {vraw, sizeof(k_quad_data)}, {iraw, sizeof(k_quad_indices)});
        REQUIRE(mesh.IsValid());
        REQUIRE(mesh.GetVertexCount() == 4);
        REQUIRE(mesh.GetIndexCount() == 6);
        REQUIRE(mesh.HasIndices());
        REQUIRE(mesh.GetVertexLayout().GetStride() == 20);  // float3 + float2 = 12 + 8
    }

    SECTION("Invalid layout throws")
    {
        Rndr::Canvas::VertexLayout layout;  // empty, invalid
        const auto* raw = reinterpret_cast<const Rndr::u8*>(k_triangle_positions);
        REQUIRE_THROWS(Rndr::Canvas::Mesh(layout, {raw, sizeof(k_triangle_positions)}));
    }

    SECTION("Empty vertex data throws")
    {
        Rndr::Canvas::VertexLayout layout = MakePositionLayout();
        REQUIRE_THROWS(Rndr::Canvas::Mesh(layout, {}));
    }

    SECTION("Vertex data not multiple of stride throws")
    {
        Rndr::Canvas::VertexLayout layout = MakePositionLayout();
        const auto* raw = reinterpret_cast<const Rndr::u8*>(k_triangle_positions);
        // Pass data that's not a multiple of stride (12 bytes).
        REQUIRE_THROWS(Rndr::Canvas::Mesh(layout, {raw, 10}));
    }

    SECTION("Index data not multiple of 4 throws")
    {
        Rndr::Canvas::VertexLayout layout = MakePositionLayout();
        const auto* vraw = reinterpret_cast<const Rndr::u8*>(k_triangle_positions);
        const auto* iraw = reinterpret_cast<const Rndr::u8*>(k_triangle_indices);
        REQUIRE_THROWS(Rndr::Canvas::Mesh(layout, {vraw, sizeof(k_triangle_positions)}, {iraw, 5}));
    }

    SECTION("Destroy makes mesh invalid")
    {
        Rndr::Canvas::VertexLayout layout = MakePositionLayout();
        const auto* raw = reinterpret_cast<const Rndr::u8*>(k_triangle_positions);
        Rndr::Canvas::Mesh mesh(layout, {raw, sizeof(k_triangle_positions)});
        REQUIRE(mesh.IsValid());
        mesh.Destroy();
        REQUIRE_FALSE(mesh.IsValid());
        REQUIRE(mesh.GetVertexCount() == 0);
        REQUIRE(mesh.GetIndexCount() == 0);
    }

    SECTION("Move constructor")
    {
        Rndr::Canvas::VertexLayout layout = MakePositionLayout();
        const auto* raw = reinterpret_cast<const Rndr::u8*>(k_triangle_positions);
        Rndr::Canvas::Mesh mesh(layout, {raw, sizeof(k_triangle_positions)});
        const Rndr::u32 handle = mesh.GetNativeHandle();

        Rndr::Canvas::Mesh const moved(std::move(mesh));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetNativeHandle() == handle);
        REQUIRE(moved.GetVertexCount() == 3);
        REQUIRE_FALSE(mesh.IsValid());
    }

    SECTION("Move assignment")
    {
        Rndr::Canvas::VertexLayout layout = MakePositionLayout();
        const auto* raw = reinterpret_cast<const Rndr::u8*>(k_triangle_positions);
        Rndr::Canvas::Mesh mesh(layout, {raw, sizeof(k_triangle_positions)});
        Rndr::Canvas::Mesh other;

        other = std::move(mesh);
        REQUIRE(other.IsValid());
        REQUIRE(other.GetVertexCount() == 3);
        REQUIRE_FALSE(mesh.IsValid());
    }

    SECTION("Clone")
    {
        Rndr::Canvas::VertexLayout layout = MakePositionLayout();
        const auto* vraw = reinterpret_cast<const Rndr::u8*>(k_triangle_positions);
        const auto* iraw = reinterpret_cast<const Rndr::u8*>(k_triangle_indices);
        Rndr::Canvas::Mesh const mesh(layout, {vraw, sizeof(k_triangle_positions)}, {iraw, sizeof(k_triangle_indices)});
        REQUIRE(mesh.IsValid());

        Rndr::Canvas::Mesh const clone = mesh.Clone();
        REQUIRE(clone.IsValid());
        REQUIRE(clone.GetNativeHandle() != mesh.GetNativeHandle());
        REQUIRE(clone.GetVertexCount() == mesh.GetVertexCount());
        REQUIRE(clone.GetIndexCount() == mesh.GetIndexCount());
        REQUIRE(mesh.IsValid());
    }

    SECTION("Clone of invalid mesh returns invalid")
    {
        Rndr::Canvas::Mesh const mesh;
        Rndr::Canvas::Mesh const clone = mesh.Clone();
        REQUIRE_FALSE(clone.IsValid());
    }
}
