#include <catch2/catch2.hpp>

#include "opal/container/scope-ptr.h"
#include "opal/exceptions.h"

#include "rndr/application.hpp"
#include "rndr/canvas/buffer.hpp"
#include "rndr/canvas/context.hpp"
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
    return Rndr::Canvas::Context::Init(window.Clone());
}

struct BufferTestFixture
{
    Opal::ScopePtr<Rndr::Application> app;
    Opal::Ref<Rndr::GenericWindow> window;
    Rndr::Canvas::Context context;

    BufferTestFixture() : context(CreateTestContext(app, window)) {}
};

}  // namespace

TEST_CASE("Canvas BufferUsage enum", "[canvas][buffer]")
{
    constexpr auto count = static_cast<Rndr::u8>(Rndr::Canvas::BufferUsage::EnumCount);
    REQUIRE(count == 4);
}

TEST_CASE("Canvas Buffer", "[canvas][buffer]")
{
    BufferTestFixture f;

    SECTION("Default constructed buffer is invalid")
    {
        Rndr::Canvas::Buffer buf;
        REQUIRE_FALSE(buf.IsValid());
    }

    SECTION("Create vertex buffer")
    {
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Vertex, 1024);
        REQUIRE(buf.IsValid());
        REQUIRE(buf.GetUsage() == Rndr::Canvas::BufferUsage::Vertex);
        REQUIRE(buf.GetSize() == 1024);
        REQUIRE(buf.GetOffset() == 0);
    }

    SECTION("Create index buffer with offset")
    {
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Index, 512, 64);
        REQUIRE(buf.IsValid());
        REQUIRE(buf.GetUsage() == Rndr::Canvas::BufferUsage::Index);
        REQUIRE(buf.GetSize() == 512);
        REQUIRE(buf.GetOffset() == 64);
    }

    SECTION("Create uniform buffer")
    {
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Uniform, 256);
        REQUIRE(buf.IsValid());
        REQUIRE(buf.GetUsage() == Rndr::Canvas::BufferUsage::Uniform);
    }

    SECTION("Create storage buffer")
    {
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Storage, 2048);
        REQUIRE(buf.IsValid());
        REQUIRE(buf.GetUsage() == Rndr::Canvas::BufferUsage::Storage);
    }

    SECTION("Create buffer with initial data")
    {
        const Rndr::u8 data[] = {1, 2, 3, 4, 5, 6, 7, 8};
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Vertex, sizeof(data), 0, Opal::ArrayView<const Rndr::u8>(data, sizeof(data)));
        REQUIRE(buf.IsValid());
        REQUIRE(buf.GetSize() == sizeof(data));
    }

    SECTION("Create buffer with debug name")
    {
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Vertex, 128, 0, {}, "TestBuffer");
        REQUIRE(buf.IsValid());
        REQUIRE(buf.GetName() == "TestBuffer");
    }

    SECTION("Destroy makes buffer invalid")
    {
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Vertex, 256);
        REQUIRE(buf.IsValid());
        buf.Destroy();
        REQUIRE_FALSE(buf.IsValid());
    }

    SECTION("Move constructor")
    {
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Vertex, 512, 32, {}, "MoveBuf");
        REQUIRE(buf.IsValid());

        Rndr::Canvas::Buffer moved(std::move(buf));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetUsage() == Rndr::Canvas::BufferUsage::Vertex);
        REQUIRE(moved.GetSize() == 512);
        REQUIRE(moved.GetOffset() == 32);
        REQUIRE_FALSE(buf.IsValid());
    }

    SECTION("Move assignment")
    {
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Index, 256);
        Rndr::Canvas::Buffer other;

        other = std::move(buf);
        REQUIRE(other.IsValid());
        REQUIRE(other.GetUsage() == Rndr::Canvas::BufferUsage::Index);
        REQUIRE(other.GetSize() == 256);
        REQUIRE_FALSE(buf.IsValid());
    }

    SECTION("Clone")
    {
        const Rndr::u8 data[] = {10, 20, 30, 40};
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Vertex, sizeof(data), 0, Opal::ArrayView<const Rndr::u8>(data, sizeof(data)),
                                 "CloneSrc");

        Rndr::Canvas::Buffer clone = buf.Clone();
        REQUIRE(clone.IsValid());
        REQUIRE(clone.GetUsage() == buf.GetUsage());
        REQUIRE(clone.GetSize() == buf.GetSize());
        REQUIRE(clone.GetOffset() == buf.GetOffset());
        // Original should still be valid.
        REQUIRE(buf.IsValid());
    }

    SECTION("Clone of invalid buffer returns invalid buffer")
    {
        Rndr::Canvas::Buffer buf;
        Rndr::Canvas::Buffer clone = buf.Clone();
        REQUIRE_FALSE(clone.IsValid());
    }

    SECTION("Update buffer data")
    {
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Vertex, 16);
        REQUIRE(buf.IsValid());

        const Rndr::u8 data[] = {1, 2, 3, 4};
        buf.Update(Opal::ArrayView<const Rndr::u8>(data, sizeof(data)));
    }

    SECTION("Update invalid buffer throws")
    {
        Rndr::Canvas::Buffer buf;
        const Rndr::u8 data[] = {1, 2, 3, 4};
        REQUIRE_THROWS(buf.Update(Opal::ArrayView<const Rndr::u8>(data, sizeof(data))));
    }

    SECTION("Update with data exceeding buffer size throws")
    {
        Rndr::Canvas::Buffer buf(Rndr::Canvas::BufferUsage::Vertex, 4);
        const Rndr::u8 data[] = {1, 2, 3, 4, 5, 6, 7, 8};
        REQUIRE_THROWS(buf.Update(Opal::ArrayView<const Rndr::u8>(data, sizeof(data))));
    }
}
