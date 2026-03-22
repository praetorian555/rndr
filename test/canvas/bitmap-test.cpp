#include <catch2/catch2.hpp>

#include "opal/container/in-place-array.h"

#include "rndr/canvas/bitmap.hpp"

TEST_CASE("Canvas Bitmap", "[canvas][bitmap]")
{
    SECTION("Create with zeroed data")
    {
        Rndr::Canvas::Bitmap bitmap(3, 4, 1, Rndr::Canvas::Format::RGBA8);
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetWidth() == 3);
        REQUIRE(bitmap.GetHeight() == 4);
        REQUIRE(bitmap.GetDepth() == 1);
        REQUIRE(bitmap.GetComponentCount() == 4);
        REQUIRE(bitmap.GetPixelSize() == 4);
        REQUIRE(bitmap.GetRowSize() == 12);
        REQUIRE(bitmap.GetLayerSize() == 48);
        REQUIRE(bitmap.GetTotalSize() == 48);
        REQUIRE(bitmap.GetData() != nullptr);
        REQUIRE(bitmap.GetData()[0] == 0);
        REQUIRE(bitmap.GetData()[47] == 0);
    }

    SECTION("Create with initial data")
    {
        constexpr int k_width = 2;
        constexpr int k_height = 3;
        constexpr int k_size = k_width * k_height * 4;
        Opal::InPlaceArray<Rndr::u8, k_size> data;
        for (int i = 0; i < k_size; ++i)
        {
            data[i] = static_cast<Rndr::u8>(i + 1);
        }
        Rndr::Canvas::Bitmap bitmap(k_width, k_height, 1, Rndr::Canvas::Format::RGBA8, Opal::AsBytes(data));
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetData()[0] == 1);
        REQUIRE(bitmap.GetData()[k_size - 1] == k_size);
    }

    SECTION("Create with data smaller than bitmap")
    {
        constexpr int k_width = 2;
        constexpr int k_height = 2;
        constexpr int k_total = k_width * k_height * 4;
        constexpr int k_half = k_total / 2;
        Opal::InPlaceArray<Rndr::u8, k_half> data;
        for (int i = 0; i < k_half; ++i)
        {
            data[i] = static_cast<Rndr::u8>(i + 1);
        }
        Rndr::Canvas::Bitmap bitmap(k_width, k_height, 1, Rndr::Canvas::Format::RGBA8, Opal::AsBytes(data));
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetData()[0] == 1);
        REQUIRE(bitmap.GetData()[k_half - 1] == k_half);
        REQUIRE(bitmap.GetData()[k_half] == 0);
        REQUIRE(bitmap.GetData()[k_total - 1] == 0);
    }

    SECTION("Create with data larger than bitmap")
    {
        constexpr int k_total = 4;
        Opal::InPlaceArray<Rndr::u8, k_total * 2> data;
        for (int i = 0; i < k_total * 2; ++i)
        {
            data[i] = static_cast<Rndr::u8>(i + 1);
        }
        Rndr::Canvas::Bitmap bitmap(1, 1, 1, Rndr::Canvas::Format::RGBA8, Opal::AsBytes(data));
        REQUIRE(bitmap.IsValid());
        REQUIRE(bitmap.GetTotalSize() == k_total);
        REQUIRE(bitmap.GetData()[0] == 1);
        REQUIRE(bitmap.GetData()[3] == 4);
    }

    SECTION("Invalid creation throws")
    {
        REQUIRE_THROWS(Rndr::Canvas::Bitmap(0, 1, 1, Rndr::Canvas::Format::RGBA8));
        REQUIRE_THROWS(Rndr::Canvas::Bitmap(1, 0, 1, Rndr::Canvas::Format::RGBA8));
        REQUIRE_THROWS(Rndr::Canvas::Bitmap(1, 1, 0, Rndr::Canvas::Format::RGBA8));
        REQUIRE_THROWS(Rndr::Canvas::Bitmap(-1, 1, 1, Rndr::Canvas::Format::RGBA8));
        REQUIRE_THROWS(Rndr::Canvas::Bitmap(1, -1, 1, Rndr::Canvas::Format::RGBA8));
        REQUIRE_THROWS(Rndr::Canvas::Bitmap(1, 1, -1, Rndr::Canvas::Format::RGBA8));
        REQUIRE_THROWS(Rndr::Canvas::Bitmap(1, 1, 1, Rndr::Canvas::Format::D24S8));
        REQUIRE_THROWS(Rndr::Canvas::Bitmap(1, 1, 1, Rndr::Canvas::Format::Float1));
    }

    SECTION("Get and set pixel unsigned byte")
    {
        constexpr int k_width = 2;
        constexpr int k_height = 2;
        constexpr int k_size = k_width * k_height * 4;
        Opal::InPlaceArray<Rndr::u8, k_size> data;
        for (int i = 0; i < k_size; ++i)
        {
            data[i] = static_cast<Rndr::u8>(i * 16);
        }
        Rndr::Canvas::Bitmap bitmap(k_width, k_height, 1, Rndr::Canvas::Format::RGBA8, Opal::AsBytes(data));

        // Read pixel (1, 1) which is the last pixel.
        // Its data is at offset 12: bytes 192, 208, 224, 240.
        const Rndr::Vector4f pixel = bitmap.GetPixel(1, 1);
        REQUIRE(Opal::IsEqual(pixel.x, 192 / 255.0f, 0.01f));
        REQUIRE(Opal::IsEqual(pixel.y, 208 / 255.0f, 0.01f));
        REQUIRE(Opal::IsEqual(pixel.z, 224 / 255.0f, 0.01f));
        REQUIRE(Opal::IsEqual(pixel.w, 240 / 255.0f, 0.01f));

        // Write pixel (0, 0).
        const Rndr::Vector4f new_pixel{0.5f, 0.25f, 0.75f, 1.0f};
        bitmap.SetPixel(0, 0, 0, new_pixel);
        const Rndr::Vector4f read_back = bitmap.GetPixel(0, 0);
        REQUIRE(Opal::IsEqual(read_back.x, 0.5f, 0.01f));
        REQUIRE(Opal::IsEqual(read_back.y, 0.25f, 0.01f));
        REQUIRE(Opal::IsEqual(read_back.z, 0.75f, 0.01f));
        REQUIRE(Opal::IsEqual(read_back.w, 1.0f, 0.01f));
    }

    SECTION("Get and set pixel float")
    {
        Rndr::Canvas::Bitmap bitmap(2, 1, 1, Rndr::Canvas::Format::RGBA32F);

        const Rndr::Vector4f pixel{1.5f, 2.5f, 3.5f, 4.5f};
        bitmap.SetPixel(1, 0, 0, pixel);

        const Rndr::Vector4f read_back = bitmap.GetPixel(1, 0);
        REQUIRE(read_back.x == pixel.x);
        REQUIRE(read_back.y == pixel.y);
        REQUIRE(read_back.z == pixel.z);
        REQUIRE(read_back.w == pixel.w);

        // Pixel (0, 0) should still be zero.
        const Rndr::Vector4f zero_pixel = bitmap.GetPixel(0, 0);
        REQUIRE(zero_pixel.x == 0.0f);
        REQUIRE(zero_pixel.y == 0.0f);
        REQUIRE(zero_pixel.z == 0.0f);
        REQUIRE(zero_pixel.w == 0.0f);
    }

    SECTION("Single component format")
    {
        Rndr::Canvas::Bitmap bitmap(2, 2, 1, Rndr::Canvas::Format::R8);
        REQUIRE(bitmap.GetComponentCount() == 1);
        REQUIRE(bitmap.GetPixelSize() == 1);
        REQUIRE(bitmap.GetTotalSize() == 4);

        bitmap.SetPixel(1, 0, 0, Rndr::Vector4f{0.5f, 0.0f, 0.0f, 0.0f});
        const Rndr::Vector4f pixel = bitmap.GetPixel(1, 0);
        REQUIRE(Opal::IsEqual(pixel.x, 0.5f, 0.01f));
    }

    SECTION("Two component format")
    {
        Rndr::Canvas::Bitmap bitmap(2, 2, 1, Rndr::Canvas::Format::RG8);
        REQUIRE(bitmap.GetComponentCount() == 2);
        REQUIRE(bitmap.GetPixelSize() == 2);
        REQUIRE(bitmap.GetTotalSize() == 8);
    }

    SECTION("Format support checks")
    {
        REQUIRE(Rndr::Canvas::Bitmap::IsFormatSupported(Rndr::Canvas::Format::RGBA8));
        REQUIRE(Rndr::Canvas::Bitmap::IsFormatSupported(Rndr::Canvas::Format::R8));
        REQUIRE(Rndr::Canvas::Bitmap::IsFormatSupported(Rndr::Canvas::Format::RGBA32F));
        REQUIRE(Rndr::Canvas::Bitmap::IsFormatSupported(Rndr::Canvas::Format::R16F));
        REQUIRE_FALSE(Rndr::Canvas::Bitmap::IsFormatSupported(Rndr::Canvas::Format::D24S8));
        REQUIRE_FALSE(Rndr::Canvas::Bitmap::IsFormatSupported(Rndr::Canvas::Format::Float1));
        REQUIRE_FALSE(Rndr::Canvas::Bitmap::IsFormatSupported(Rndr::Canvas::Format::Int1));
    }

    SECTION("GetDataView")
    {
        Rndr::Canvas::Bitmap bitmap(2, 2, 1, Rndr::Canvas::Format::RGBA8);
        auto view = bitmap.GetDataView();
        REQUIRE(view.GetSize() == 16);
        REQUIRE(view.GetData() == bitmap.GetData());
    }

    SECTION("Depth multiple layers")
    {
        constexpr int k_width = 2;
        constexpr int k_height = 2;
        constexpr int k_depth = 3;
        Rndr::Canvas::Bitmap bitmap(k_width, k_height, k_depth, Rndr::Canvas::Format::RGBA8);
        REQUIRE(bitmap.GetDepth() == k_depth);
        REQUIRE(bitmap.GetLayerSize() == k_width * k_height * 4);
        REQUIRE(bitmap.GetTotalSize() == k_width * k_height * k_depth * 4);

        // Write to each layer and verify isolation.
        const Rndr::Vector4f pixel0{0.1f, 0.2f, 0.3f, 0.4f};
        const Rndr::Vector4f pixel1{0.5f, 0.6f, 0.7f, 0.8f};
        const Rndr::Vector4f pixel2{0.9f, 1.0f, 0.0f, 0.1f};
        bitmap.SetPixel(0, 0, 0, pixel0);
        bitmap.SetPixel(0, 0, 1, pixel1);
        bitmap.SetPixel(0, 0, 2, pixel2);

        const Rndr::Vector4f read0 = bitmap.GetPixel(0, 0, 0);
        const Rndr::Vector4f read1 = bitmap.GetPixel(0, 0, 1);
        const Rndr::Vector4f read2 = bitmap.GetPixel(0, 0, 2);
        REQUIRE(Opal::IsEqual(read0.x, pixel0.x, 0.01f));
        REQUIRE(Opal::IsEqual(read1.x, pixel1.x, 0.01f));
        REQUIRE(Opal::IsEqual(read2.x, pixel2.x, 0.01f));
    }

    SECTION("Depth with float format")
    {
        Rndr::Canvas::Bitmap bitmap(1, 1, 2, Rndr::Canvas::Format::RGBA32F);
        REQUIRE(bitmap.GetTotalSize() == 2 * 16);

        const Rndr::Vector4f layer0{1.0f, 2.0f, 3.0f, 4.0f};
        const Rndr::Vector4f layer1{5.0f, 6.0f, 7.0f, 8.0f};
        bitmap.SetPixel(0, 0, 0, layer0);
        bitmap.SetPixel(0, 0, 1, layer1);

        const Rndr::Vector4f read0 = bitmap.GetPixel(0, 0, 0);
        const Rndr::Vector4f read1 = bitmap.GetPixel(0, 0, 1);
        REQUIRE(read0.x == 1.0f);
        REQUIRE(read0.w == 4.0f);
        REQUIRE(read1.x == 5.0f);
        REQUIRE(read1.w == 8.0f);
    }
}
