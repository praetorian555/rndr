#pragma once

#include "opal/container/array-view.h"
#include "opal/container/string.h"

#include "rndr/canvas/format.hpp"

namespace Rndr
{
namespace Canvas
{

/** Type of the texture resource. */
enum class TextureType : u8
{
    Texture2D,
    Texture2DArray,
    CubeMap,
    EnumCount
};

/** Texture sampling filter. */
enum class TextureFilter : u8
{
    Nearest,
    Linear,
    EnumCount
};

/** Texture coordinate wrapping mode. */
enum class TextureWrap : u8
{
    Clamp,
    Border,
    Repeat,
    MirrorRepeat,
    MirrorOnce,
    EnumCount
};

/** Border color used when wrap mode is Border. */
enum class BorderColor : u8
{
    TransparentBlack,
    OpaqueBlack,
    OpaqueWhite,
    EnumCount
};

struct TextureDesc
{
    /** Width of the texture in pixels. */
    i32 width = 0;

    /** Height of the texture in pixels. */
    i32 height = 0;

    /** Number of layers. Only used for Texture2DArray. */
    i32 array_size = 1;

    /** Type of the texture. */
    TextureType type = TextureType::Texture2D;

    /** Pixel format of the texture. */
    Format format = Format::RGBA8;

    /** Minification filter. */
    TextureFilter min_filter = TextureFilter::Linear;

    /** Magnification filter. */
    TextureFilter mag_filter = TextureFilter::Linear;

    /** Filter used when selecting mip level. Only used when use_mips is true. */
    TextureFilter mip_map_filter = TextureFilter::Linear;

    /** Maximum anisotropy level. Values > 1 enable anisotropic filtering. */
    f32 max_anisotropy = 1.0f;

    /** Horizontal wrap mode. */
    TextureWrap wrap_u = TextureWrap::Clamp;

    /** Vertical wrap mode. */
    TextureWrap wrap_v = TextureWrap::Clamp;

    /** Depth wrap mode. Used for Texture2DArray and CubeMap. */
    TextureWrap wrap_w = TextureWrap::Clamp;

    /** Border color when wrap mode is Border. */
    BorderColor border_color = BorderColor::OpaqueBlack;

    /** Generate mipmaps. */
    bool use_mips = false;

    /** Number of samples per pixel. Values > 1 create a multi-sample texture. */
    i32 sample_count = 1;

    /** Bias added to computed mip level. */
    f32 lod_bias = 0.0f;

    /** Minimum mip level to use. */
    i32 base_mip_level = 0;

    /** Maximum mip level to use. 0 means use all available levels. */
    i32 max_mip_level = 0;

    /** Minimum LOD value. */
    f32 min_lod = 0.0f;

    /** Maximum LOD value. */
    f32 max_lod = 0.0f;
};

/**
 * GPU texture resource. Move-only, RAII.
 */
class Texture
{
public:
    Texture() = default;
    /**
     * Create a GPU texture. Requires an active Canvas context on the calling thread.
     * @param desc Texture descriptor.
     * @param init_data Optional initial pixel data.
     * @param name Debug name for GPU debugging tools.
     */
    explicit Texture(const TextureDesc& desc, const Opal::ArrayView<const u8>& init_data = {},
                     const Opal::StringUtf8& name = {});
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    /**
     * Load a texture from an image file. Supports PNG, JPEG, HDR (via stbi).
     * Width, height, and format are determined from the file. Sampling parameters (filters, wrap modes, etc.)
     * are taken from the provided descriptor. Requires an active Canvas context on the calling thread.
     * @param file_path Path to the image file.
     * @param desc Texture descriptor for sampling parameters. Width, height, and format fields are overridden.
     * @param flip_vertically If true, flip the image vertically. Only applies to stbi-loaded images.
     * @param debug_name Debug name for GPU debugging tools.
     * @return A valid Texture.
     * @throw Opal::Exception if the file does not exist or cannot be loaded.
     */
    [[nodiscard]] static Texture FromFile(const Opal::StringUtf8& file_path, TextureDesc desc = {},
                                          bool flip_vertically = false, Opal::StringUtf8 debug_name = {});

    /**
     * Create a cubemap texture from an equirectangular image file. The image is loaded, converted
     * to 6 cubemap faces using bilinear sampling, and uploaded as a CubeMap texture. Requires an
     * active Canvas context on the calling thread.
     * @param file_path Path to the equirectangular image file (PNG, JPEG, HDR).
     * @param face_size Size of each cubemap face in pixels. If 0, defaults to half the image height.
     * @param desc Texture descriptor for sampling parameters. Width, height, format, and type fields are overridden.
     * @param debug_name Debug name for GPU debugging tools.
     * @return A valid CubeMap Texture.
     * @throw Opal::Exception if the file does not exist or cannot be loaded.
     */
    [[nodiscard]] static Texture FromEquirectangular(const Opal::StringUtf8& file_path, i32 face_size = 0,
                                                     TextureDesc desc = {}, Opal::StringUtf8 debug_name = {});

    [[nodiscard]] Texture Clone() const;
    void Destroy();

    /**
     * Upload pixel data to the entire base mip level of a single-sample Texture2D.
     * @param data Pixel data to upload. Must be exactly width * height * pixel_size bytes.
     * @throw Opal::InvalidArgumentException if the texture is not a Texture2D or the data size is wrong.
     * @throw GraphicsAPIException if the texture is invalid or multi-sample.
     */
    void Update(const Opal::ArrayView<const u8>& data) const;

    /**
     * Upload pixel data to a rectangular sub-region of a single-sample Texture2D mip level.
     * @param data Pixel data to upload. Must be exactly width * height * pixel_size bytes.
     * @param x Horizontal texel offset of the region within the mip level.
     * @param y Vertical texel offset of the region within the mip level.
     * @param width Width of the region in texels.
     * @param height Height of the region in texels.
     * @param mip_level Target mip level. Defaults to 0.
     * @throw Opal::InvalidArgumentException if the texture is not a Texture2D, the region or mip level is
     *        out of bounds, or the data size does not match the region.
     * @throw GraphicsAPIException if the texture is invalid or multi-sample.
     */
    void UpdateRegion(const Opal::ArrayView<const u8>& data, i32 x, i32 y, i32 width, i32 height, i32 mip_level = 0) const;

    /**
     * Upload pixel data to one full layer of a Texture2DArray or one face of a CubeMap, at a mip level.
     * @param data Pixel data to upload. Must be exactly mip_width * mip_height * pixel_size bytes, where the
     *        mip dimensions are the texture dimensions halved per mip level (clamped to 1).
     * @param layer Array layer index for Texture2DArray, or face index [0, 5] for CubeMap.
     * @param mip_level Target mip level. Defaults to 0.
     * @throw Opal::InvalidArgumentException if the texture is not a Texture2DArray or CubeMap, the layer or
     *        mip level is out of bounds, or the data size does not match the layer.
     * @throw GraphicsAPIException if the texture is invalid or multi-sample.
     */
    void UpdateLayer(const Opal::ArrayView<const u8>& data, i32 layer, i32 mip_level = 0) const;

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] const TextureDesc& GetDesc() const;
    [[nodiscard]] const Opal::StringUtf8& GetName() const;
    [[nodiscard]] u32 GetNativeHandle() const;

private:
    TextureDesc m_desc;
    u32 m_handle = 0;
    i32 m_max_mip_levels = 0;
    Opal::StringUtf8 m_name;
};

}  // namespace Canvas
}  // namespace Rndr
