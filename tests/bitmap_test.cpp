#include <catch2/catch_test_macros.hpp>

#include "rndr/core/bitmap.h"

TEST_CASE("Bitmap init", "[init]")
{
    SECTION("Default create and destroy")
    {
        Rndr::Bitmap bitmap(1, 1, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 1);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 4);
        REQUIRE(bitmap.GetRowSize() == 4);
        REQUIRE(bitmap.GetSize() == 4);
        REQUIRE(bitmap.GetData() != nullptr);
    }
    SECTION("Default create and destroy with data")
    {
        const uint8_t data[4] = {1, 2, 3, 4};
        Rndr::Bitmap bitmap(1, 1, Rndr::PixelFormat::R8G8B8A8_UNORM, data);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 1);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 4);
        REQUIRE(bitmap.GetRowSize() == 4);
        REQUIRE(bitmap.GetSize() == 4);
        REQUIRE(bitmap.GetData() != nullptr);
        REQUIRE(bitmap.GetData()[0] == 1);
        REQUIRE(bitmap.GetData()[1] == 2);
        REQUIRE(bitmap.GetData()[2] == 3);
        REQUIRE(bitmap.GetData()[3] == 4);
    }
    SECTION("Create with depth param different then 1 and no data")
    {
        Rndr::Bitmap bitmap(1, 1, 2, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 1);
        REQUIRE(bitmap.GetDepth() == 2);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 4);
        REQUIRE(bitmap.GetRowSize() == 4);
        REQUIRE(bitmap.GetSize() == 4);
        REQUIRE(bitmap.GetData() != nullptr);
    }
    SECTION("Create with depth param different then 1 and data")
    {
        const uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        Rndr::Bitmap bitmap(1, 1, 2, Rndr::PixelFormat::R8G8B8A8_UNORM, data);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 1);
        REQUIRE(bitmap.GetDepth() == 2);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 4);
        REQUIRE(bitmap.GetRowSize() == 4);
        REQUIRE(bitmap.GetSize() == 4);
        REQUIRE(bitmap.GetData() != nullptr);
        REQUIRE(bitmap.GetData()[0] == 1);
        REQUIRE(bitmap.GetData()[1] == 2);
        REQUIRE(bitmap.GetData()[2] == 3);
        REQUIRE(bitmap.GetData()[3] == 4);
    }
    SECTION("Creating invalid bitmaps")
    {
        const Rndr::Bitmap bitmap1(0, 0, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(!bitmap1.IsValid());
        const Rndr::Bitmap bitmap2(-1, 2, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(!bitmap2.IsValid());
        const Rndr::Bitmap bitmap3(2, -1, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(!bitmap3.IsValid());
        const Rndr::Bitmap bitmap4(2, 2, 0, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(!bitmap4.IsValid());
        const Rndr::Bitmap bitmap5(2, 2, -1, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(!bitmap5.IsValid());
        const Rndr::Bitmap bitmap6(2, 2, 2, Rndr::PixelFormat::R32_TYPELESS);
        REQUIRE(!bitmap6.IsValid());
    }
}

TEST_CASE("Bitmap get", "[get]")
{
    SECTION("Default create and pixel get")
    {
        const uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        Rndr::Bitmap bitmap(1, 1, Rndr::PixelFormat::R8G8B8A8_UNORM, data);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 1);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 4);
        REQUIRE(bitmap.GetRowSize() == 4);
        REQUIRE(bitmap.GetSize() == 4);
        REQUIRE(bitmap.GetData() != nullptr);
        const math::Vector4 ref_pixel{1 / 255.0f, 2 / 255.0f, 3 / 255.0f, 4 / 255.0f};
        REQUIRE(math::IsEqual(bitmap.GetPixel(0, 0), ref_pixel, 0.000001f) == true);
    }
    SECTION("Default create and pixel get with float pixel format")
    {
        const float data[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
        const uint8_t* byte_data = reinterpret_cast<const uint8_t*>(data);
        Rndr::Bitmap bitmap(1, 1, Rndr::PixelFormat::R32G32B32A32_FLOAT, byte_data);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 1);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 16);
        REQUIRE(bitmap.GetRowSize() == 16);
        REQUIRE(bitmap.GetSize() == 16);
        REQUIRE(bitmap.GetData() != nullptr);
        const math::Vector4 ref_pixel{1.0f, 2.0f, 3.0f, 4.0f};
        REQUIRE(math::IsEqual(bitmap.GetPixel(0, 0), ref_pixel, 0.000001f) == true);
    }
}

TEST_CASE("Bitmap set", "[set]")
{
    SECTION("Set in unsigned format")
    {
        const uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        Rndr::Bitmap bitmap(1, 1, Rndr::PixelFormat::R8G8B8A8_UNORM, data);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 1);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 4);
        REQUIRE(bitmap.GetRowSize() == 4);
        REQUIRE(bitmap.GetSize() == 4);
        REQUIRE(bitmap.GetData() != nullptr);
        const math::Vector4 ref_pixel{1 / 255.0f, 2 / 255.0f, 3 / 255.0f, 4 / 255.0f};
        REQUIRE(math::IsEqual(bitmap.GetPixel(0, 0), ref_pixel, 0.000001f) == true);
        const math::Vector4 new_pixel{5 / 255.0f, 6 / 255.0f, 7 / 255.0f, 8 / 255.0f};
        bitmap.SetPixel(0, 0, new_pixel);
        REQUIRE(math::IsEqual(bitmap.GetPixel(0, 0), new_pixel, 0.000001f) == true);
    }
    SECTION("Set in float format")
    {
        const float data[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
        const uint8_t* byte_data = reinterpret_cast<const uint8_t*>(data);
        Rndr::Bitmap bitmap(1, 1, Rndr::PixelFormat::R32G32B32A32_FLOAT, byte_data);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 1);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 16);
        REQUIRE(bitmap.GetRowSize() == 16);
        REQUIRE(bitmap.GetSize() == 16);
        REQUIRE(bitmap.GetData() != nullptr);
        const math::Vector4 ref_pixel{1.0f, 2.0f, 3.0f, 4.0f};
        REQUIRE(math::IsEqual(bitmap.GetPixel(0, 0), ref_pixel, 0.000001f) == true);
        const math::Vector4 new_pixel{5.0f, 6.0f, 7.0f, 8.0f};
        bitmap.SetPixel(0, 0, new_pixel);
        REQUIRE(math::IsEqual(bitmap.GetPixel(0, 0), new_pixel, 0.000001f) == true);
    }
}

TEST_CASE("Bitmap check pixel format", "[pixelformatsupported]")
{
    SECTION("Check if pixel format is supported")
    {
        REQUIRE(Rndr::Bitmap::IsPixelFormatSupported(Rndr::PixelFormat::R8G8B8A8_UNORM) == true);
        REQUIRE(Rndr::Bitmap::IsPixelFormatSupported(Rndr::PixelFormat::R32G32B32A32_FLOAT)
                == true);
        REQUIRE(Rndr::Bitmap::IsPixelFormatSupported(Rndr::PixelFormat::R32_TYPELESS) == false);
    }
}
