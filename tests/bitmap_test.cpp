#include <catch2/catch_test_macros.hpp>

#include "rndr/core/bitmap.h"

TEST_CASE("Bitmap", "[init]")
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
