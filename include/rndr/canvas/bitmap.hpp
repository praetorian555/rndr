#pragma once

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"

#include "rndr/canvas/context.hpp"
#include "rndr/math.hpp"

namespace Rndr
{
namespace Canvas
{

/**
 * CPU-side image storage. Supports per-pixel read/write access.
 * The depth dimension can be used to represent layers in a 2D array texture.
 */
class Bitmap
{
public:
    Bitmap() = default;

    /**
     * Create a bitmap with the specified dimensions, format and optional initial data.
     * @param width Width in pixels. Must be greater than 0.
     * @param height Height in pixels. Must be greater than 0.
     * @param depth Depth (number of layers). Must be greater than 0. Default is 1.
     * @param format Pixel format. Must be a pixel format (not a vertex or depth format).
     * @param data Optional initial data. If not provided, the bitmap is zeroed.
     */
    Bitmap(i32 width, i32 height, i32 depth, Format format, const Opal::ArrayView<const u8>& data = {});

    [[nodiscard]] bool IsValid() const { return m_width > 0 && m_height > 0 && m_depth > 0; }

    [[nodiscard]] i32 GetWidth() const { return m_width; }
    [[nodiscard]] i32 GetHeight() const { return m_height; }
    [[nodiscard]] i32 GetDepth() const { return m_depth; }
    [[nodiscard]] Format GetFormat() const { return m_format; }
    [[nodiscard]] i32 GetComponentCount() const { return m_comp_count; }

    /** Get size of a single pixel in bytes. */
    [[nodiscard]] i32 GetPixelSize() const { return m_pixel_size; }

    /** Get size of a single row in bytes. */
    [[nodiscard]] u64 GetRowSize() const { return static_cast<u64>(m_width) * m_pixel_size; }

    /** Get size of a single 2D layer in bytes. */
    [[nodiscard]] u64 GetLayerSize() const { return static_cast<u64>(m_width) * m_height * m_pixel_size; }

    /** Get total size of the bitmap data in bytes, including all layers. */
    [[nodiscard]] u64 GetTotalSize() const { return static_cast<u64>(m_width) * m_height * m_depth * m_pixel_size; }

    [[nodiscard]] u8* GetData() { return m_data.GetData(); }
    [[nodiscard]] const u8* GetData() const { return m_data.GetData(); }
    [[nodiscard]] Opal::ArrayView<const u8> GetDataView() const { return {m_data.GetData(), m_data.GetSize()}; }

    /**
     * Read a pixel value as normalized floats.
     * @param x X coordinate. Must be in [0, width).
     * @param y Y coordinate. Must be in [0, height).
     * @param z Layer index. Must be in [0, depth). Default is 0.
     * @return Pixel value with components in [0, 1] for unsigned byte formats, or raw values for float formats.
     */
    [[nodiscard]] Vector4f GetPixel(i32 x, i32 y, i32 z = 0) const;

    /**
     * Write a pixel value.
     * @param x X coordinate. Must be in [0, width).
     * @param y Y coordinate. Must be in [0, height).
     * @param z Layer index. Must be in [0, depth). Default is 0.
     * @param pixel Pixel value. For unsigned byte formats, components are clamped to [0, 1].
     */
    void SetPixel(i32 x, i32 y, i32 z, const Vector4f& pixel);

    /** Check if a format is supported for bitmap storage. */
    [[nodiscard]] static bool IsFormatSupported(Format format);

    /** Get pixel size in bytes for a given format. Returns 0 for unsupported formats. */
    [[nodiscard]] static i32 GetFormatPixelSize(Format format);

    /** Get number of components for a given format. Returns 0 for unsupported formats. */
    [[nodiscard]] static i32 GetFormatComponentCount(Format format);

private:
    [[nodiscard]] Vector4f GetPixelUnsignedByte(i32 x, i32 y, i32 z) const;
    void SetPixelUnsignedByte(i32 x, i32 y, i32 z, const Vector4f& pixel);

    [[nodiscard]] Vector4f GetPixelFloat(i32 x, i32 y, i32 z) const;
    void SetPixelFloat(i32 x, i32 y, i32 z, const Vector4f& pixel);

    using GetPixelFunc = Vector4f (Bitmap::*)(i32 x, i32 y, i32 z) const;
    using SetPixelFunc = void (Bitmap::*)(i32 x, i32 y, i32 z, const Vector4f& pixel);

    i32 m_width = 0;
    i32 m_height = 0;
    i32 m_depth = 0;
    i32 m_comp_count = 0;
    i32 m_pixel_size = 0;
    Format m_format = Format::RGBA8;
    Opal::DynamicArray<u8> m_data;
    GetPixelFunc m_get_pixel_func = nullptr;
    SetPixelFunc m_set_pixel_func = nullptr;
};

}  // namespace Canvas
}  // namespace Rndr
