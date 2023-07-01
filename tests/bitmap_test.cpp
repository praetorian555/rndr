#include <catch2/catch_test_macros.hpp>

#include "rndr/core/bitmap.h"

TEST_CASE("Bitmap", "[bitmap]")
{
    SECTION("Create with zeroed data")
    {
        Rndr::Bitmap bitmap(3, 4, 5, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 3);
        REQUIRE(bitmap.GetHeight() == 4);
        REQUIRE(bitmap.GetDepth() == 5);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 4);
        REQUIRE(bitmap.GetRowSize() == 12);
        REQUIRE(bitmap.GetSize2D() == 48);
        REQUIRE(bitmap.GetSize3D() == 240);
        REQUIRE(bitmap.GetData() != nullptr);
        REQUIRE(bitmap.GetData()[0] == 0);
        REQUIRE(bitmap.GetData()[60] == 0);
        REQUIRE(bitmap.GetData()[123] == 0);
        REQUIRE(bitmap.GetData()[239] == 0);
    }
    SECTION("Create with custom data that fit and low precision components")
    {
        constexpr int k_width = 1;
        constexpr int k_height = 2;
        constexpr int k_depth = 3;
        constexpr int k_size = k_width * k_height * k_depth * 4;
        Rndr::StackArray<uint8_t, k_size> data;
        for (int i = 0; i < k_size; ++i)
        {
            data[i] = static_cast<uint8_t>(i + 1);
        }
        Rndr::Bitmap bitmap(k_width, k_height, k_depth, Rndr::PixelFormat::R8G8B8A8_UNORM, data);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 2);
        REQUIRE(bitmap.GetDepth() == 3);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 4);
        REQUIRE(bitmap.GetRowSize() == 4);
        REQUIRE(bitmap.GetSize2D() == 8);
        REQUIRE(bitmap.GetSize3D() == 24);
        REQUIRE(bitmap.GetData() != nullptr);
        REQUIRE(bitmap.GetData()[0] == 1);
        REQUIRE(bitmap.GetData()[3] == 4);
        REQUIRE(bitmap.GetData()[9] == 10);
        REQUIRE(bitmap.GetData()[23] == 24);

        SECTION("Get pixel")
        {
            const math::Vector4 ref_pixel{21 / 255.0f, 22 / 255.0f, 23 / 255.0f, 24 / 255.0f};
            REQUIRE(math::IsEqual(bitmap.GetPixel(0, 1, 2), ref_pixel, 0.000001f) == true);
        }
        SECTION("Set pixel")
        {
            const math::Vector4 ref_pixel{21 / 255.0f, 22 / 255.0f, 23 / 255.0f, 24 / 255.0f};
            REQUIRE(math::IsEqual(bitmap.GetPixel(0, 1, 2), ref_pixel, 0.000001f) == true);
            const math::Vector4 new_pixel{5 / 255.0f, 6 / 255.0f, 7 / 255.0f, 8 / 255.0f};
            bitmap.SetPixel(0, 1, 2, new_pixel);
            REQUIRE(math::IsEqual(bitmap.GetPixel(0, 1, 2), new_pixel, 0.000001f) == true);
        }
    }
    SECTION("Create with custom data that fit and high precision components")
    {
        constexpr int k_width = 1;
        constexpr int k_height = 2;
        constexpr int k_depth = 3;
        constexpr int k_size = k_width * k_height * k_depth * 4;
        Rndr::StackArray<float, k_size> data_float;
        for (int i = 0; i < k_size; ++i)
        {
            data_float[i] = static_cast<float>(i + 1);
        }
        Rndr::StackArray<uint8_t, k_size * sizeof(float)> data;
        memcpy(data.data(), data_float.data(), k_size * sizeof(float));
        Rndr::Bitmap bitmap(k_width, k_height, k_depth, Rndr::PixelFormat::R32G32B32A32_FLOAT, data);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 2);
        REQUIRE(bitmap.GetDepth() == 3);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 16);
        REQUIRE(bitmap.GetRowSize() == 16);
        REQUIRE(bitmap.GetSize2D() == 32);
        REQUIRE(bitmap.GetSize3D() == 96);
        REQUIRE(bitmap.GetData() != nullptr);

        SECTION("Get pixel")
        {
            const math::Vector4 ref_pixel{21.0f, 22.0f, 23.0f, 24.0f};
            REQUIRE(math::IsEqual(bitmap.GetPixel(0, 1, 2), ref_pixel, 0.000001f) == true);
        }
        SECTION("Set pixel")
        {
            const math::Vector4 ref_pixel{21.0f, 22.0f, 23.0f, 24.0f};
            REQUIRE(math::IsEqual(bitmap.GetPixel(0, 1, 2), ref_pixel, 0.000001f) == true);
            const math::Vector4 new_pixel{1.0f, 2.0f, 3.0f, 4.0f};
            bitmap.SetPixel(0, 1, 2, new_pixel);
            REQUIRE(math::IsEqual(bitmap.GetPixel(0, 1, 2), new_pixel, 0.000001f) == true);
        }
    }
    SECTION("Create with custom data that is smaller then the bitmap")
    {
        constexpr int k_width = 1;
        constexpr int k_height = 2;
        constexpr int k_depth = 3;
        constexpr int k_size = k_width * k_height * k_depth * 4;
        Rndr::StackArray<uint8_t, k_size / 2> data;
        for (int i = 0; i < k_size / 2; ++i)
        {
            data[i] = static_cast<uint8_t>(i + 1);
        }
        Rndr::Bitmap bitmap(k_width, k_height, k_depth, Rndr::PixelFormat::R8G8B8A8_UNORM, data);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 2);
        REQUIRE(bitmap.GetDepth() == 3);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 4);
        REQUIRE(bitmap.GetRowSize() == 4);
        REQUIRE(bitmap.GetSize2D() == 8);
        REQUIRE(bitmap.GetSize3D() == 24);
        REQUIRE(bitmap.GetData() != nullptr);
        REQUIRE(bitmap.GetData()[0] == 1);
        REQUIRE(bitmap.GetData()[3] == 4);
        REQUIRE(bitmap.GetData()[9] == 10);
        REQUIRE(bitmap.GetData()[20] == 0);
        REQUIRE(bitmap.GetData()[23] == 0);
    }
    SECTION("Create with custom data that is larger then bitmap")
    {
        constexpr int k_width = 1;
        constexpr int k_height = 2;
        constexpr int k_depth = 3;
        constexpr int k_size = k_width * k_height * k_depth * 4;
        Rndr::StackArray<uint8_t, k_size * 2> data;
        for (int i = 0; i < k_size * 2; ++i)
        {
            data[i] = static_cast<uint8_t>(i + 1);
        }
        Rndr::Bitmap bitmap(k_width, k_height, k_depth, Rndr::PixelFormat::R8G8B8A8_UNORM, data);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 1);
        REQUIRE(bitmap.GetHeight() == 2);
        REQUIRE(bitmap.GetDepth() == 3);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 4);
        REQUIRE(bitmap.GetRowSize() == 4);
        REQUIRE(bitmap.GetSize2D() == 8);
        REQUIRE(bitmap.GetSize3D() == 24);
        REQUIRE(bitmap.GetData() != nullptr);
        REQUIRE(bitmap.GetData()[0] == 1);
        REQUIRE(bitmap.GetData()[3] == 4);
        REQUIRE(bitmap.GetData()[9] == 10);
        REQUIRE(bitmap.GetData()[23] == 24);
    }
    SECTION("Creating invalid bitmaps")
    {
        const Rndr::Bitmap bitmap1(0, 0, 0, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(!bitmap1.IsValid());
        const Rndr::Bitmap bitmap2(-1, 2, 2, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(!bitmap2.IsValid());
        const Rndr::Bitmap bitmap3(2, -1, 2, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(!bitmap3.IsValid());
        const Rndr::Bitmap bitmap4(2, 2, 0, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(!bitmap4.IsValid());
        const Rndr::Bitmap bitmap5(2, 2, -1, Rndr::PixelFormat::R8G8B8A8_UNORM);
        REQUIRE(!bitmap5.IsValid());
        const Rndr::Bitmap bitmap6(2, 2, 2, Rndr::PixelFormat::R32_TYPELESS);
        REQUIRE(!bitmap6.IsValid());
    }
    SECTION("Check if pixel format is supported")
    {
        REQUIRE(Rndr::Bitmap::IsPixelFormatSupported(Rndr::PixelFormat::R8G8B8A8_UNORM) == true);
        REQUIRE(Rndr::Bitmap::IsPixelFormatSupported(Rndr::PixelFormat::R32G32B32A32_FLOAT)
                == true);
        REQUIRE(Rndr::Bitmap::IsPixelFormatSupported(Rndr::PixelFormat::R32_TYPELESS) == false);
    }
}
