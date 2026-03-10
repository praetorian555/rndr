#include <catch2/catch2.hpp>

#include "opal/container/scope-ptr.h"
#include "opal/exceptions.h"

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/texture.hpp"
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

struct TextureTestFixture
{
    Opal::ScopePtr<Rndr::Application> app;
    Opal::Ref<Rndr::GenericWindow> window;
    Rndr::Canvas::Context context;

    TextureTestFixture() : context(CreateTestContext(app, window)) {}
};

}  // namespace

TEST_CASE("Canvas Texture enums", "[canvas][texture]")
{
    SECTION("TextureType EnumCount")
    {
        constexpr auto count = static_cast<Rndr::u8>(Rndr::Canvas::TextureType::EnumCount);
        REQUIRE(count == 3);
    }

    SECTION("TextureFilter EnumCount")
    {
        constexpr auto count = static_cast<Rndr::u8>(Rndr::Canvas::TextureFilter::EnumCount);
        REQUIRE(count == 2);
    }

    SECTION("TextureWrap EnumCount")
    {
        constexpr auto count = static_cast<Rndr::u8>(Rndr::Canvas::TextureWrap::EnumCount);
        REQUIRE(count == 5);
    }

    SECTION("BorderColor EnumCount")
    {
        constexpr auto count = static_cast<Rndr::u8>(Rndr::Canvas::BorderColor::EnumCount);
        REQUIRE(count == 3);
    }
}

TEST_CASE("Canvas Texture", "[canvas][texture]")
{
    TextureTestFixture f;

    SECTION("Default constructed texture is invalid")
    {
        Rndr::Canvas::Texture tex;
        REQUIRE_FALSE(tex.IsValid());
    }

    SECTION("Create Texture2D with defaults")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 64;
        desc.height = 64;

        Rndr::Canvas::Texture tex(f.context, desc);
        REQUIRE(tex.IsValid());
        REQUIRE(tex.GetDesc().width == 64);
        REQUIRE(tex.GetDesc().height == 64);
        REQUIRE(tex.GetDesc().type == Rndr::Canvas::TextureType::Texture2D);
        REQUIRE(tex.GetDesc().format == Rndr::Canvas::Format::RGBA8);
        REQUIRE(tex.GetNativeHandle() != 0);
    }

    SECTION("Zero dimensions throw")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 0;
        desc.height = 64;
        REQUIRE_THROWS_AS(Rndr::Canvas::Texture(f.context, desc), Opal::InvalidArgumentException);
    }

    SECTION("Negative dimensions throw")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 64;
        desc.height = -1;
        REQUIRE_THROWS_AS(Rndr::Canvas::Texture(f.context, desc), Opal::InvalidArgumentException);
    }

    SECTION("Texture2DArray with zero array_size throws")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 32;
        desc.height = 32;
        desc.type = Rndr::Canvas::TextureType::Texture2DArray;
        desc.array_size = 0;
        REQUIRE_THROWS_AS(Rndr::Canvas::Texture(f.context, desc), Opal::InvalidArgumentException);
    }

    SECTION("Create with debug name")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 16;
        desc.height = 16;

        Rndr::Canvas::Texture tex(f.context, desc, {}, "TestTexture");
        REQUIRE(tex.IsValid());
        REQUIRE(tex.GetName() == "TestTexture");
    }

    SECTION("Create with initial data")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 2;
        desc.height = 2;
        desc.format = Rndr::Canvas::Format::RGBA8;

        // 2x2 RGBA8 = 16 bytes.
        const Rndr::u8 pixels[16] = {};
        Rndr::Canvas::Texture tex(f.context, desc, Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)));
        REQUIRE(tex.IsValid());
    }

    SECTION("Destroy makes texture invalid")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 8;
        desc.height = 8;

        Rndr::Canvas::Texture tex(f.context, desc);
        REQUIRE(tex.IsValid());
        tex.Destroy();
        REQUIRE_FALSE(tex.IsValid());
    }

    SECTION("Move constructor")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 32;
        desc.height = 32;

        Rndr::Canvas::Texture tex(f.context, desc, {}, "MoveTex");
        REQUIRE(tex.IsValid());

        Rndr::Canvas::Texture moved(std::move(tex));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetDesc().width == 32);
        REQUIRE(moved.GetDesc().height == 32);
        REQUIRE_FALSE(tex.IsValid());
    }

    SECTION("Move assignment")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 16;
        desc.height = 16;

        Rndr::Canvas::Texture tex(f.context, desc);
        Rndr::Canvas::Texture other;

        other = std::move(tex);
        REQUIRE(other.IsValid());
        REQUIRE(other.GetDesc().width == 16);
        REQUIRE_FALSE(tex.IsValid());
    }

    SECTION("Clone")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 16;
        desc.height = 16;
        desc.format = Rndr::Canvas::Format::RGBA8;

        Rndr::Canvas::Texture tex(f.context, desc, {}, "CloneSrc");

        Rndr::Canvas::Texture clone = tex.Clone();
        REQUIRE(clone.IsValid());
        REQUIRE(clone.GetDesc().width == desc.width);
        REQUIRE(clone.GetDesc().height == desc.height);
        REQUIRE(clone.GetDesc().format == desc.format);
        // Original still valid.
        REQUIRE(tex.IsValid());
        // Different native handles.
        REQUIRE(clone.GetNativeHandle() != tex.GetNativeHandle());
    }

    SECTION("Clone of invalid texture returns invalid texture")
    {
        Rndr::Canvas::Texture tex;
        Rndr::Canvas::Texture clone = tex.Clone();
        REQUIRE_FALSE(clone.IsValid());
    }

    SECTION("Update Texture2D")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 4;
        desc.height = 4;
        desc.format = Rndr::Canvas::Format::RGBA8;

        Rndr::Canvas::Texture tex(f.context, desc);
        REQUIRE(tex.IsValid());

        // 4x4 RGBA8 = 64 bytes.
        const Rndr::u8 pixels[64] = {};
        tex.Update(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)));
    }

    SECTION("Update invalid texture throws")
    {
        Rndr::Canvas::Texture tex;
        const Rndr::u8 pixels[4] = {};
        REQUIRE_THROWS(tex.Update(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels))));
    }
}

TEST_CASE("Canvas Texture pixel formats", "[canvas][texture]")
{
    TextureTestFixture f;

    const Rndr::Canvas::Format formats[] = {
        Rndr::Canvas::Format::R8,    Rndr::Canvas::Format::RG8,    Rndr::Canvas::Format::RGB8,
        Rndr::Canvas::Format::RGBA8, Rndr::Canvas::Format::R16F,   Rndr::Canvas::Format::RG16F,
        Rndr::Canvas::Format::RGBA16F, Rndr::Canvas::Format::R32F, Rndr::Canvas::Format::RG32F,
        Rndr::Canvas::Format::RGBA32F, Rndr::Canvas::Format::D24S8, Rndr::Canvas::Format::D32F,
    };

    for (auto fmt : formats)
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 4;
        desc.height = 4;
        desc.format = fmt;

        Rndr::Canvas::Texture tex(f.context, desc);
        REQUIRE(tex.IsValid());
        REQUIRE(tex.GetDesc().format == fmt);
    }
}

TEST_CASE("Canvas Texture sampler options", "[canvas][texture]")
{
    TextureTestFixture f;

    SECTION("Nearest filter")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 8;
        desc.height = 8;
        desc.min_filter = Rndr::Canvas::TextureFilter::Nearest;
        desc.mag_filter = Rndr::Canvas::TextureFilter::Nearest;

        Rndr::Canvas::Texture tex(f.context, desc);
        REQUIRE(tex.IsValid());
        REQUIRE(tex.GetDesc().min_filter == Rndr::Canvas::TextureFilter::Nearest);
        REQUIRE(tex.GetDesc().mag_filter == Rndr::Canvas::TextureFilter::Nearest);
    }

    SECTION("Wrap modes")
    {
        const Rndr::Canvas::TextureWrap wraps[] = {
            Rndr::Canvas::TextureWrap::Clamp,    Rndr::Canvas::TextureWrap::Border,
            Rndr::Canvas::TextureWrap::Repeat,   Rndr::Canvas::TextureWrap::MirrorRepeat,
            Rndr::Canvas::TextureWrap::MirrorOnce,
        };

        for (auto wrap : wraps)
        {
            Rndr::Canvas::TextureDesc desc;
            desc.width = 8;
            desc.height = 8;
            desc.wrap_u = wrap;
            desc.wrap_v = wrap;

            Rndr::Canvas::Texture tex(f.context, desc);
            REQUIRE(tex.IsValid());
        }
    }

    SECTION("Mipmaps")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 64;
        desc.height = 64;
        desc.use_mips = true;
        desc.mip_map_filter = Rndr::Canvas::TextureFilter::Linear;

        Rndr::Canvas::Texture tex(f.context, desc);
        REQUIRE(tex.IsValid());
        REQUIRE(tex.GetDesc().use_mips);
    }
}

TEST_CASE("Canvas Texture types", "[canvas][texture]")
{
    TextureTestFixture f;

    SECTION("Texture2DArray")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 16;
        desc.height = 16;
        desc.type = Rndr::Canvas::TextureType::Texture2DArray;
        desc.array_size = 4;

        Rndr::Canvas::Texture tex(f.context, desc);
        REQUIRE(tex.IsValid());
        REQUIRE(tex.GetDesc().type == Rndr::Canvas::TextureType::Texture2DArray);
        REQUIRE(tex.GetDesc().array_size == 4);
    }

    SECTION("Texture2DArray with initial data")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 2;
        desc.height = 2;
        desc.format = Rndr::Canvas::Format::RGBA8;
        desc.type = Rndr::Canvas::TextureType::Texture2DArray;
        desc.array_size = 2;

        // 2 layers * 2x2 * 4 bytes = 32 bytes.
        const Rndr::u8 pixels[32] = {};
        Rndr::Canvas::Texture tex(f.context, desc, Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)));
        REQUIRE(tex.IsValid());
    }

    SECTION("CubeMap")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 16;
        desc.height = 16;
        desc.type = Rndr::Canvas::TextureType::CubeMap;

        Rndr::Canvas::Texture tex(f.context, desc);
        REQUIRE(tex.IsValid());
        REQUIRE(tex.GetDesc().type == Rndr::Canvas::TextureType::CubeMap);
    }

    SECTION("CubeMap with initial data")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 2;
        desc.height = 2;
        desc.format = Rndr::Canvas::Format::RGBA8;
        desc.type = Rndr::Canvas::TextureType::CubeMap;

        // 6 faces * 2x2 * 4 bytes = 96 bytes.
        const Rndr::u8 pixels[96] = {};
        Rndr::Canvas::Texture tex(f.context, desc, Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)));
        REQUIRE(tex.IsValid());
    }
}
