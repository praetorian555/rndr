#include <catch2/catch2.hpp>

#include "canvas-test-common.hpp"

#include "canvas-test-common.hpp"

#include "opal/container/scope-ptr.h"
#include "opal/exceptions.h"

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/texture.hpp"
#include "rndr/generic-window.hpp"

namespace
{

struct TextureTestFixture
{
    Opal::ScopePtr<Rndr::Application> app;
    Opal::Ref<Rndr::GenericWindow> window;
    Rndr::Canvas::Context context;

    TextureTestFixture() : context(CanvasTest::CreateTestContext(app, window)) {}
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

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        REQUIRE(tex.IsValid());
        REQUIRE(tex.GetDesc().width == 64);
        REQUIRE(tex.GetDesc().height == 64);
        REQUIRE(tex.GetDesc().type == Rndr::Canvas::TextureType::Texture2D);
        REQUIRE(tex.GetDesc().format == Rndr::Canvas::Format::RGBA8);
        REQUIRE(tex.GetNativeHandle() != 0);
    }

    SECTION("Zero dimensions report InvalidArgument")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 0;
        desc.height = 64;
        REQUIRE(Rndr::Canvas::Texture::Create(desc).GetError() == Rndr::ErrorCode::InvalidArgument);
    }

    SECTION("Negative dimensions report InvalidArgument")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 64;
        desc.height = -1;
        REQUIRE(Rndr::Canvas::Texture::Create(desc).GetError() == Rndr::ErrorCode::InvalidArgument);
    }

    SECTION("Texture2DArray with zero array_size reports InvalidArgument")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 32;
        desc.height = 32;
        desc.type = Rndr::Canvas::TextureType::Texture2DArray;
        desc.array_size = 0;
        REQUIRE(Rndr::Canvas::Texture::Create(desc).GetError() == Rndr::ErrorCode::InvalidArgument);
    }

    SECTION("Create with debug name")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 16;
        desc.height = 16;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc, {}, "TestTexture"));
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
        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc, Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels))));
        REQUIRE(tex.IsValid());
    }

    SECTION("Destroy makes texture invalid")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 8;
        desc.height = 8;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        REQUIRE(tex.IsValid());
        tex.Destroy();
        REQUIRE_FALSE(tex.IsValid());
    }

    SECTION("Move constructor")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 32;
        desc.height = 32;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc, {}, "MoveTex"));
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

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
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

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc, {}, "CloneSrc"));

        Rndr::Canvas::Texture clone = CanvasTest::Unwrap(tex.Clone());
        REQUIRE(clone.IsValid());
        REQUIRE(clone.GetDesc().width == desc.width);
        REQUIRE(clone.GetDesc().height == desc.height);
        REQUIRE(clone.GetDesc().format == desc.format);
        // Original still valid.
        REQUIRE(tex.IsValid());
        // Different native handles.
        REQUIRE(clone.GetNativeHandle() != tex.GetNativeHandle());
    }

    SECTION("Clone of invalid texture reports InvalidArgument")
    {
        Rndr::Canvas::Texture tex;
        REQUIRE(tex.Clone().GetError() == Rndr::ErrorCode::InvalidArgument);
    }

    SECTION("Update Texture2D")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 4;
        desc.height = 4;
        desc.format = Rndr::Canvas::Format::RGBA8;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        REQUIRE(tex.IsValid());

        // 4x4 RGBA8 = 64 bytes.
        const Rndr::u8 pixels[64] = {};
        REQUIRE(tex.Update(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels))) == Rndr::ErrorCode::Success);
    }

    SECTION("Update invalid texture throws")
    {
        Rndr::Canvas::Texture tex;
        const Rndr::u8 pixels[4] = {};
        REQUIRE(tex.Update(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels))) == Rndr::ErrorCode::InvalidArgument);
    }

    SECTION("Update with wrong data size throws")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 4;
        desc.height = 4;
        desc.format = Rndr::Canvas::Format::RGBA8;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        // Should be 64 bytes; provide 32.
        const Rndr::u8 pixels[32] = {};
        REQUIRE(tex.Update(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels))) == Rndr::ErrorCode::InvalidArgument);
    }

    SECTION("UpdateRegion sub-region")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 8;
        desc.height = 8;
        desc.format = Rndr::Canvas::Format::RGBA8;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        REQUIRE(tex.IsValid());

        // Update a 2x2 region at (1, 1): 2*2*4 = 16 bytes.
        const Rndr::u8 pixels[16] = {};
        REQUIRE(tex.UpdateRegion(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)), 1, 1, 2, 2) == Rndr::ErrorCode::Success);
    }

    SECTION("UpdateRegion out of bounds reports OutOfBounds")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 8;
        desc.height = 8;
        desc.format = Rndr::Canvas::Format::RGBA8;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        const Rndr::u8 pixels[16] = {};
        // Region extends past the right edge.
        REQUIRE(tex.UpdateRegion(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)), 7, 0, 2, 2)
                == Rndr::ErrorCode::OutOfBounds);
    }

    SECTION("UpdateRegion at mip level")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 8;
        desc.height = 8;
        desc.format = Rndr::Canvas::Format::RGBA8;
        desc.use_mips = true;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        REQUIRE(tex.IsValid());

        // Mip 1 is 4x4; update the full 4x4 = 64 bytes.
        const Rndr::u8 pixels[64] = {};
        REQUIRE(tex.UpdateRegion(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)), 0, 0, 4, 4, 1) == Rndr::ErrorCode::Success);
    }

    SECTION("UpdateRegion on non-Texture2D reports InvalidArgument")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 8;
        desc.height = 8;
        desc.type = Rndr::Canvas::TextureType::CubeMap;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        const Rndr::u8 pixels[16] = {};
        REQUIRE(tex.UpdateRegion(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)), 0, 0, 2, 2)
                == Rndr::ErrorCode::InvalidArgument);
    }

    SECTION("UpdateLayer of Texture2DArray")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 4;
        desc.height = 4;
        desc.format = Rndr::Canvas::Format::RGBA8;
        desc.type = Rndr::Canvas::TextureType::Texture2DArray;
        desc.array_size = 3;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        REQUIRE(tex.IsValid());

        // One layer is 4x4x4 = 64 bytes.
        const Rndr::u8 pixels[64] = {};
        REQUIRE(tex.UpdateLayer(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)), 2) == Rndr::ErrorCode::Success);
    }

    SECTION("UpdateLayer of CubeMap face")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 4;
        desc.height = 4;
        desc.format = Rndr::Canvas::Format::RGBA8;
        desc.type = Rndr::Canvas::TextureType::CubeMap;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        REQUIRE(tex.IsValid());

        // One face is 4x4x4 = 64 bytes.
        const Rndr::u8 pixels[64] = {};
        REQUIRE(tex.UpdateLayer(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)), 5) == Rndr::ErrorCode::Success);
    }

    SECTION("UpdateLayer out of range throws")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 4;
        desc.height = 4;
        desc.type = Rndr::Canvas::TextureType::CubeMap;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        const Rndr::u8 pixels[64] = {};
        REQUIRE(tex.UpdateLayer(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)), 6)
                == Rndr::ErrorCode::OutOfBounds);
    }

    SECTION("UpdateLayer on Texture2D reports InvalidArgument")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 4;
        desc.height = 4;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        const Rndr::u8 pixels[64] = {};
        REQUIRE(tex.UpdateLayer(Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels)), 0)
                == Rndr::ErrorCode::InvalidArgument);
    }

    SECTION("Read Texture2D round-trips uploaded data")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 2;
        desc.height = 2;
        desc.format = Rndr::Canvas::Format::RGBA8;

        // 2x2 RGBA8 = 16 bytes of distinct values.
        Rndr::u8 pixels[16];
        for (Rndr::i32 i = 0; i < 16; ++i)
        {
            pixels[i] = static_cast<Rndr::u8>(i * 16);
        }

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc, Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels))));
        REQUIRE(tex.IsValid());

        const Opal::DynamicArray<Rndr::u8> read_back = CanvasTest::Unwrap(tex.Read());
        REQUIRE(read_back.GetSize() == sizeof(pixels));
        for (Rndr::i32 i = 0; i < 16; ++i)
        {
            REQUIRE(read_back[i] == pixels[i]);
        }
    }

    SECTION("ReadRegion round-trips an updated sub-region")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 4;
        desc.height = 4;
        desc.format = Rndr::Canvas::Format::RGBA8;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        REQUIRE(tex.IsValid());

        // Write a 2x2 region at (1, 1) with a known value.
        Rndr::u8 region[16];
        for (Rndr::i32 i = 0; i < 16; ++i)
        {
            region[i] = static_cast<Rndr::u8>(200 + i);
        }
        REQUIRE(tex.UpdateRegion(Opal::ArrayView<const Rndr::u8>(region, sizeof(region)), 1, 1, 2, 2) == Rndr::ErrorCode::Success);

        const Opal::DynamicArray<Rndr::u8> read_back = CanvasTest::Unwrap(tex.ReadRegion(1, 1, 2, 2));
        REQUIRE(read_back.GetSize() == sizeof(region));
        for (Rndr::i32 i = 0; i < 16; ++i)
        {
            REQUIRE(read_back[i] == region[i]);
        }
    }

    SECTION("ReadLayer round-trips an updated cubemap face")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 2;
        desc.height = 2;
        desc.format = Rndr::Canvas::Format::RGBA8;
        desc.type = Rndr::Canvas::TextureType::CubeMap;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        REQUIRE(tex.IsValid());

        // One face is 2x2x4 = 16 bytes.
        Rndr::u8 face[16];
        for (Rndr::i32 i = 0; i < 16; ++i)
        {
            face[i] = static_cast<Rndr::u8>(i + 1);
        }
        REQUIRE(tex.UpdateLayer(Opal::ArrayView<const Rndr::u8>(face, sizeof(face)), 4) == Rndr::ErrorCode::Success);

        const Opal::DynamicArray<Rndr::u8> read_back = CanvasTest::Unwrap(tex.ReadLayer(4));
        REQUIRE(read_back.GetSize() == sizeof(face));
        for (Rndr::i32 i = 0; i < 16; ++i)
        {
            REQUIRE(read_back[i] == face[i]);
        }
    }

    SECTION("Read on non-Texture2D reports InvalidArgument")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 4;
        desc.height = 4;
        desc.type = Rndr::Canvas::TextureType::Texture2DArray;
        desc.array_size = 2;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
        REQUIRE(tex.Read().GetError() == Rndr::ErrorCode::InvalidArgument);
    }

    SECTION("Read on invalid texture reports InvalidArgument")
    {
        Rndr::Canvas::Texture tex;
        REQUIRE(tex.Read().GetError() == Rndr::ErrorCode::InvalidArgument);
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

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
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

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
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

            Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
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

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
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

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
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
        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc, Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels))));
        REQUIRE(tex.IsValid());
    }

    SECTION("CubeMap")
    {
        Rndr::Canvas::TextureDesc desc;
        desc.width = 16;
        desc.height = 16;
        desc.type = Rndr::Canvas::TextureType::CubeMap;

        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc));
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
        Rndr::Canvas::Texture tex = CanvasTest::Unwrap(Rndr::Canvas::Texture::Create(desc, Opal::ArrayView<const Rndr::u8>(pixels, sizeof(pixels))));
        REQUIRE(tex.IsValid());
    }
}
