#pragma once

#include "opal/container/array-view.h"
#include "opal/container/string.h"

#include "rndr/canvas/context.hpp"

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
     * Create a GPU texture.
     * @param context Active Canvas context.
     * @param desc Texture descriptor.
     * @param init_data Optional initial pixel data.
     * @param name Debug name for GPU debugging tools.
     */
    explicit Texture(const Context& context, const TextureDesc& desc = {}, const Opal::ArrayView<const u8>& init_data = {},
                     const Opal::StringUtf8& name = {});
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    [[nodiscard]] Texture Clone() const;
    void Destroy();

    /**
     * Upload pixel data to the texture. Only valid for single-sample Texture2D.
     * @param data Pixel data to upload.
     */
    void Update(const Opal::ArrayView<const u8>& data) const;

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
