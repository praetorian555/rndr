#include <catch2/catch2.hpp>

#include "rndr/bitmap.hpp"
#include "rndr/error-codes.hpp"
#include "rndr/file.hpp"

namespace
{

// A 2x2 RGBA8 PNG: red, green on the top row, blue and half-transparent white below.
constexpr Rndr::u8 k_rgba8_png[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
                                    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xB6, 0x0D,
                                    0x24, 0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
                                    0x1F, 0x0C, 0x19, 0x18, 0xFE, 0x83, 0x40, 0x03, 0x00, 0x49, 0x49, 0x09, 0x78, 0x9C, 0x3D, 0x3C,
                                    0xBE, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

// A 1x1 16-bit RGB PNG of 0x1234, 0xABCD, 0xFFFF.
constexpr Rndr::u8 k_rgb16_png[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44,
                                    0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x10, 0x02, 0x00, 0x00, 0x00, 0xC0,
                                    0xE7, 0x8F, 0x9D, 0x00, 0x00, 0x00, 0x0F, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x10,
                                    0x32, 0x59, 0x7D, 0xF6, 0xFF, 0x7F, 0x00, 0x09, 0x87, 0x03, 0xBD, 0xE5, 0x0C, 0x77, 0x05,
                                    0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

// A PNG header that claims 100000x4 pixels, with no pixel data after it.
constexpr Rndr::u8 k_huge_png_header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44,
                                          0x52, 0x00, 0x01, 0x86, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x08, 0x06, 0x00, 0x00, 0x00, 0x0D,
                                          0xB3, 0x87, 0x66, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

}  // namespace

TEST_CASE("An image in memory decodes to RGBA", "[bitmap]")
{
    SECTION("An 8-bit PNG keeps its pixels, top row first")
    {
        Opal::Expected<Rndr::Bitmap, Rndr::ErrorCode> decoded = Rndr::File::DecodeImage({k_rgba8_png, sizeof(k_rgba8_png)}, false, false);
        REQUIRE(decoded.HasValue());
        const Rndr::Bitmap& bitmap = decoded.GetValue();
        REQUIRE(bitmap.GetWidth() == 2);
        REQUIRE(bitmap.GetHeight() == 2);
        REQUIRE(bitmap.GetPixelFormat() == Rndr::PixelFormat::R8G8B8A8_SRGB);
        const Rndr::u8 expected[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 128};
        for (int i = 0; i < 16; ++i)
        {
            REQUIRE(bitmap.GetData()[i] == expected[i]);
        }
    }
    SECTION("Flipping puts the bottom row first")
    {
        Opal::Expected<Rndr::Bitmap, Rndr::ErrorCode> decoded = Rndr::File::DecodeImage({k_rgba8_png, sizeof(k_rgba8_png)}, true, false);
        REQUIRE(decoded.HasValue());
        REQUIRE(decoded.GetValue().GetData()[2] == 255);
        REQUIRE(decoded.GetValue().GetData()[0] == 0);
    }
    SECTION("A 16-bit PNG keeps 16 bits a channel, opaque alpha added")
    {
        Opal::Expected<Rndr::Bitmap, Rndr::ErrorCode> decoded = Rndr::File::DecodeImage({k_rgb16_png, sizeof(k_rgb16_png)}, false, false);
        REQUIRE(decoded.HasValue());
        REQUIRE(decoded.GetValue().GetPixelFormat() == Rndr::PixelFormat::R16G16B16A16_UNORM);
        const Rndr::u16* channels = reinterpret_cast<const Rndr::u16*>(decoded.GetValue().GetData());
        REQUIRE(channels[0] == 0x1234);
        REQUIRE(channels[1] == 0xABCD);
        REQUIRE(channels[2] == 0xFFFF);
        REQUIRE(channels[3] == 0xFFFF);
    }
}

TEST_CASE("Bytes that are not an image, or too large a one, are refused", "[bitmap]")
{
    const Rndr::u8 text[] = {'n', 'o', 't', ' ', 'a', 'n', ' ', 'i', 'm', 'a', 'g', 'e'};
    REQUIRE(Rndr::File::DecodeImage({text, sizeof(text)}, false, false).GetError() == Rndr::ErrorCode::CorruptData);
    REQUIRE(Rndr::File::DecodeImage({k_rgba8_png, 40}, false, false).GetError() == Rndr::ErrorCode::CorruptData);
    REQUIRE(Rndr::File::DecodeImage({}, false, false).GetError() == Rndr::ErrorCode::InvalidArgument);
    REQUIRE(Rndr::File::DecodeImage({k_huge_png_header, sizeof(k_huge_png_header)}, false, false).GetError() ==
            Rndr::ErrorCode::OutOfBounds);
}
