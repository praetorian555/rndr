#include "rndr/canvas/texture.hpp"

#include "stb_image/stb_image.h"

#if defined(RNDR_KTX)
#include "ktx.h"
#endif

#include "glad/glad.h"

#include "opal/container/optional.h"
#include "opal/file-system.h"
#include "opal/paths.h"

#include "rndr/canvas/gl-result.hpp"
#include "rndr/log.hpp"
#include "rndr/trace.hpp"

#include <algorithm>
#include <cmath>

namespace
{

struct GLFormatInfo
{
    GLenum internal_format;
    GLenum format;
    GLenum type;
    Rndr::i32 pixel_size;
};

GLFormatInfo ToGLFormat(Rndr::Canvas::Format format)
{
    switch (format)
    {
        case Rndr::Canvas::Format::R8:
            return {GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1};
        case Rndr::Canvas::Format::RG8:
            return {GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2};
        case Rndr::Canvas::Format::RGB8:
            return {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 3};
        case Rndr::Canvas::Format::RGBA8:
            return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4};
        case Rndr::Canvas::Format::SRGB8:
            return {GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE, 3};
        case Rndr::Canvas::Format::SRGBA8:
            return {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, 4};
        case Rndr::Canvas::Format::R16F:
            return {GL_R16F, GL_RED, GL_HALF_FLOAT, 2};
        case Rndr::Canvas::Format::RG16F:
            return {GL_RG16F, GL_RG, GL_HALF_FLOAT, 4};
        case Rndr::Canvas::Format::RGBA16F:
            return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8};
        case Rndr::Canvas::Format::R32F:
            return {GL_R32F, GL_RED, GL_FLOAT, 4};
        case Rndr::Canvas::Format::RG32F:
            return {GL_RG32F, GL_RG, GL_FLOAT, 8};
        case Rndr::Canvas::Format::RGBA32F:
            return {GL_RGBA32F, GL_RGBA, GL_FLOAT, 16};
        case Rndr::Canvas::Format::D24S8:
            return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4};
        case Rndr::Canvas::Format::D32F:
            return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 4};
        default:
            return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4};
    }
}

GLenum ToGLTarget(Rndr::Canvas::TextureType type, bool multisample)
{
    switch (type)
    {
        case Rndr::Canvas::TextureType::Texture2D:
            return multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        case Rndr::Canvas::TextureType::Texture2DArray:
            return multisample ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
        case Rndr::Canvas::TextureType::CubeMap:
            return GL_TEXTURE_CUBE_MAP;
        default:
            return GL_TEXTURE_2D;
    }
}

GLint ToGLFilter(Rndr::Canvas::TextureFilter filter)
{
    switch (filter)
    {
        case Rndr::Canvas::TextureFilter::Nearest:
            return GL_NEAREST;
        case Rndr::Canvas::TextureFilter::Linear:
            return GL_LINEAR;
        default:
            return GL_LINEAR;
    }
}

GLint ToGLMinFilter(Rndr::Canvas::TextureFilter min_filter, Rndr::Canvas::TextureFilter mip_filter, bool use_mips)
{
    if (!use_mips)
    {
        return ToGLFilter(min_filter);
    }
    switch (min_filter)
    {
        case Rndr::Canvas::TextureFilter::Nearest:
            return mip_filter == Rndr::Canvas::TextureFilter::Nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
        case Rndr::Canvas::TextureFilter::Linear:
            return mip_filter == Rndr::Canvas::TextureFilter::Nearest ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
        default:
            return GL_LINEAR_MIPMAP_LINEAR;
    }
}

GLint ToGLWrap(Rndr::Canvas::TextureWrap wrap)
{
    switch (wrap)
    {
        case Rndr::Canvas::TextureWrap::Clamp:
            return GL_CLAMP_TO_EDGE;
        case Rndr::Canvas::TextureWrap::Border:
            return GL_CLAMP_TO_BORDER;
        case Rndr::Canvas::TextureWrap::Repeat:
            return GL_REPEAT;
        case Rndr::Canvas::TextureWrap::MirrorRepeat:
            return GL_MIRRORED_REPEAT;
        case Rndr::Canvas::TextureWrap::MirrorOnce:
            return GL_MIRROR_CLAMP_TO_EDGE;
        default:
            return GL_CLAMP_TO_EDGE;
    }
}

void ToGLBorderColor(Rndr::Canvas::BorderColor color, float out[4])
{
    switch (color)
    {
        case Rndr::Canvas::BorderColor::TransparentBlack:
            out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 0.0f;
            break;
        case Rndr::Canvas::BorderColor::OpaqueBlack:
            out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 1.0f;
            break;
        case Rndr::Canvas::BorderColor::OpaqueWhite:
            out[0] = 1.0f; out[1] = 1.0f; out[2] = 1.0f; out[3] = 1.0f;
            break;
        default:
            out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 1.0f;
            break;
    }
}

Rndr::i32 CalcMipLevels(Rndr::i32 width, Rndr::i32 height)
{
    Rndr::i32 max_dim = width > height ? width : height;
    Rndr::i32 levels = 1;
    while (max_dim > 1)
    {
        max_dim >>= 1;
        levels++;
    }
    return levels;
}

void ApplySamplerParams(GLuint handle, const Rndr::Canvas::TextureDesc& desc, Rndr::i32 max_mip_levels)
{
    glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(desc.min_filter, desc.mip_map_filter, desc.use_mips));
    glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, ToGLFilter(desc.mag_filter));
    glTextureParameterf(handle, GL_TEXTURE_MAX_ANISOTROPY, desc.max_anisotropy);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_S, ToGLWrap(desc.wrap_u));
    glTextureParameteri(handle, GL_TEXTURE_WRAP_T, ToGLWrap(desc.wrap_v));
    glTextureParameteri(handle, GL_TEXTURE_WRAP_R, ToGLWrap(desc.wrap_w));

    float border[4];
    ToGLBorderColor(desc.border_color, border);
    glTextureParameterfv(handle, GL_TEXTURE_BORDER_COLOR, border);

    glTextureParameteri(handle, GL_TEXTURE_BASE_LEVEL, desc.base_mip_level);
    const Rndr::i32 max_level = desc.max_mip_level == 0 ? max_mip_levels : desc.max_mip_level;
    glTextureParameteri(handle, GL_TEXTURE_MAX_LEVEL, max_level);
    glTextureParameterf(handle, GL_TEXTURE_LOD_BIAS, desc.lod_bias);
    glTextureParameterf(handle, GL_TEXTURE_MIN_LOD, desc.min_lod);
    glTextureParameterf(handle, GL_TEXTURE_MAX_LOD, desc.max_lod);
}

}  // namespace

Opal::Expected<Rndr::Canvas::Texture, Rndr::ErrorCode> Rndr::Canvas::Texture::Create(const TextureDesc& desc,
                                                                                     const Opal::ArrayView<const u8>& init_data,
                                                                                     const Opal::StringUtf8& name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Texture::Create");
    using ResultType = Opal::Expected<Texture, ErrorCode>;

    if (desc.width <= 0 || desc.height <= 0)
    {
        RNDR_LOG_ERROR("Canvas: Texture dimensions must be positive");
        return ResultType(ErrorCode::InvalidArgument);
    }
    if (desc.type == TextureType::Texture2DArray && desc.array_size <= 0)
    {
        RNDR_LOG_ERROR("Canvas: Texture2DArray must have array_size > 0");
        return ResultType(ErrorCode::InvalidArgument);
    }

    Texture texture;
    texture.m_desc = desc;
    texture.m_name = name.Clone();
    texture.m_max_mip_levels = desc.use_mips ? CalcMipLevels(desc.width, desc.height) : 1;

    const bool multisample = desc.sample_count > 1;
    const GLenum target = ToGLTarget(desc.type, multisample);

    glCreateTextures(target, 1, &texture.m_handle);
    if (texture.m_handle == 0)
    {
        RNDR_LOG_ERROR("Canvas: Failed to create GL texture");
        return ResultType(ErrorCode::GraphicsAPIError);
    }

    const GLFormatInfo fmt = ToGLFormat(desc.format);

    // Sampler params are only valid for single-sample textures.
    if (!multisample)
    {
        ApplySamplerParams(texture.m_handle, desc, texture.m_max_mip_levels);
    }

    // Allocate storage. On failure beyond this point, texture's destructor releases the GL texture.
    if (desc.type == TextureType::Texture2D)
    {
        if (multisample)
        {
            glTextureStorage2DMultisample(texture.m_handle, desc.sample_count, fmt.internal_format, desc.width, desc.height, GL_TRUE);
        }
        else
        {
            glTextureStorage2D(texture.m_handle, texture.m_max_mip_levels, fmt.internal_format, desc.width, desc.height);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (!init_data.IsEmpty() && !multisample)
        {
            glTextureSubImage2D(texture.m_handle, 0, 0, 0, desc.width, desc.height, fmt.format, fmt.type, init_data.GetData());
        }
    }
    else if (desc.type == TextureType::Texture2DArray)
    {
        if (multisample)
        {
            glTextureStorage3DMultisample(texture.m_handle, desc.sample_count, fmt.internal_format, desc.width, desc.height,
                                          desc.array_size, GL_TRUE);
        }
        else
        {
            glTextureStorage3D(texture.m_handle, texture.m_max_mip_levels, fmt.internal_format, desc.width, desc.height,
                               desc.array_size);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (!init_data.IsEmpty() && !multisample)
        {
            const i32 layer_size = desc.width * desc.height * fmt.pixel_size;
            for (i32 i = 0; i < desc.array_size; i++)
            {
                const u8* data = init_data.GetData() + i * layer_size;
                glTextureSubImage3D(texture.m_handle, 0, 0, 0, i, desc.width, desc.height, 1, fmt.format, fmt.type, data);
            }
        }
    }
    else if (desc.type == TextureType::CubeMap)
    {
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        if (multisample)
        {
            glTextureStorage2DMultisample(texture.m_handle, desc.sample_count, fmt.internal_format, desc.width, desc.height, GL_TRUE);
        }
        else
        {
            glTextureStorage2D(texture.m_handle, texture.m_max_mip_levels, fmt.internal_format, desc.width, desc.height);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (!init_data.IsEmpty() && !multisample)
        {
            constexpr i32 k_face_count = 6;
            const i32 face_size = desc.width * desc.height * fmt.pixel_size;
            for (i32 i = 0; i < k_face_count; i++)
            {
                const u8* data = init_data.GetData() + i * face_size;
                glTextureSubImage3D(texture.m_handle, 0, 0, 0, i, desc.width, desc.height, 1, fmt.format, fmt.type, data);
            }
        }
    }

    if (desc.use_mips && !multisample)
    {
        glGenerateTextureMipmap(texture.m_handle);
    }

    RNDR_CANVAS_GL_CHECK_EXPECTED("Canvas::Texture::Create", ResultType);

    if (!texture.m_name.IsEmpty())
    {
        glObjectLabel(GL_TEXTURE, texture.m_handle, static_cast<GLsizei>(texture.m_name.GetSize()), texture.m_name.GetData());
    }
    return ResultType(std::move(texture));
}

namespace
{

#if defined(RNDR_KTX)
Opal::Optional<Rndr::Canvas::Format> GlInternalFormatToCanvasFormat(ktx_uint32_t gl_format)
{
    switch (gl_format)
    {
        case 0x8229:  // GL_R8
            return Rndr::Canvas::Format::R8;
        case 0x822B:  // GL_RG8
            return Rndr::Canvas::Format::RG8;
        case 0x8051:  // GL_RGB8
            return Rndr::Canvas::Format::RGB8;
        case 0x8058:  // GL_RGBA8
            return Rndr::Canvas::Format::RGBA8;
        case 0x8C41:  // GL_SRGB8
            return Rndr::Canvas::Format::SRGB8;
        case 0x8C43:  // GL_SRGB8_ALPHA8
            return Rndr::Canvas::Format::SRGBA8;
        case 0x822D:  // GL_R16F
            return Rndr::Canvas::Format::R16F;
        case 0x822F:  // GL_RG16F
            return Rndr::Canvas::Format::RG16F;
        case 0x881A:  // GL_RGBA16F
            return Rndr::Canvas::Format::RGBA16F;
        case 0x822E:  // GL_R32F
            return Rndr::Canvas::Format::R32F;
        case 0x8230:  // GL_RG32F
            return Rndr::Canvas::Format::RG32F;
        case 0x8814:  // GL_RGBA32F
            return Rndr::Canvas::Format::RGBA32F;
        default:
            return Opal::NullOpt;
    }
}
#endif

void CubemapFaceDirection(Rndr::i32 face, float u, float v, float& out_x, float& out_y, float& out_z)
{
    switch (face)
    {
        case 0: out_x =  1.0f; out_y = -v;    out_z = -u;    break;  // +X
        case 1: out_x = -1.0f; out_y = -v;    out_z =  u;    break;  // -X
        case 2: out_x =  u;    out_y =  1.0f; out_z =  v;    break;  // +Y
        case 3: out_x =  u;    out_y = -1.0f; out_z = -v;    break;  // -Y
        case 4: out_x =  u;    out_y = -v;    out_z =  1.0f; break;  // +Z
        default: out_x = -u;   out_y = -v;    out_z = -1.0f; break;  // -Z
    }
}

}  // namespace

Opal::Expected<Rndr::Canvas::Texture, Rndr::ErrorCode> Rndr::Canvas::Texture::FromEquirectangular(const Opal::StringUtf8& file_path,
                                                                                                  i32 face_size, TextureDesc desc,
                                                                                                  Opal::StringUtf8 debug_name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Texture::FromEquirectangular");
    using ResultType = Opal::Expected<Texture, ErrorCode>;

    if (!Opal::Exists(file_path))
    {
        RNDR_LOG_ERROR("Canvas: Image file does not exist: {}", *file_path);
        return ResultType(ErrorCode::FileNotFound);
    }

    if (debug_name.IsEmpty())
    {
        debug_name = file_path.Clone();
    }

    // Load equirectangular image.
    int eq_width = 0;
    int eq_height = 0;
    int channels = 0;
    constexpr int k_desired_channels = 4;

    const bool is_hdr = stbi_is_hdr(*file_path) > 0;
    void* eq_data = nullptr;
    i32 bytes_per_component = 0;

    stbi_set_flip_vertically_on_load(0);

    if (is_hdr)
    {
        eq_data = stbi_loadf(*file_path, &eq_width, &eq_height, &channels, k_desired_channels);
        bytes_per_component = static_cast<i32>(sizeof(f32));
        desc.format = Format::RGBA32F;
    }
    else
    {
        eq_data = stbi_load(*file_path, &eq_width, &eq_height, &channels, k_desired_channels);
        bytes_per_component = static_cast<i32>(sizeof(u8));
        desc.format = Format::SRGBA8;
    }

    if (eq_data == nullptr)
    {
        RNDR_LOG_ERROR("Canvas: Failed to load equirectangular image {}: {}", *file_path, stbi_failure_reason());
        return ResultType(ErrorCode::CorruptData);
    }

    if (face_size <= 0)
    {
        face_size = eq_height / 2;
    }

    constexpr i32 k_face_count = 6;
    const i32 pixel_size = k_desired_channels * bytes_per_component;
    const u64 face_bytes = static_cast<u64>(face_size) * face_size * pixel_size;
    const u64 total_bytes = face_bytes * k_face_count;

    Opal::DynamicArray<u8> cubemap_data;
    cubemap_data.Resize(total_bytes);

    constexpr f32 k_pi = 3.14159265358979323846f;
    constexpr f32 k_two_pi = 2.0f * k_pi;

    for (i32 face = 0; face < k_face_count; face++)
    {
        u8* face_ptr = cubemap_data.GetData() + face * face_bytes;

        for (i32 j = 0; j < face_size; j++)
        {
            for (i32 i = 0; i < face_size; i++)
            {
                const f32 u = 2.0f * (static_cast<f32>(i) + 0.5f) / static_cast<f32>(face_size) - 1.0f;
                const f32 v = 2.0f * (static_cast<f32>(j) + 0.5f) / static_cast<f32>(face_size) - 1.0f;

                float dx = 0;
                float dy = 0;
                float dz = 0;
                CubemapFaceDirection(face, u, v, dx, dy, dz);

                // Normalize direction.
                const f32 len = std::sqrt(dx * dx + dy * dy + dz * dz);
                dx /= len;
                dy /= len;
                dz /= len;

                // Convert to equirectangular UV.
                const f32 theta = std::atan2(dz, dx);
                const f32 phi = std::asin(std::clamp(dy, -1.0f, 1.0f));

                const f32 eq_u = theta / k_two_pi + 0.5f;
                const f32 eq_v = 0.5f - phi / k_pi;

                // Bilinear sample coordinates.
                const f32 px = eq_u * static_cast<f32>(eq_width - 1);
                const f32 py = eq_v * static_cast<f32>(eq_height - 1);

                const i32 x0 = static_cast<i32>(std::floor(px));
                const i32 y0 = static_cast<i32>(std::floor(py));
                const i32 y1 = std::min(y0 + 1, eq_height - 1);

                // Wrap horizontally for seamless sampling.
                const i32 x0w = ((x0 % eq_width) + eq_width) % eq_width;
                const i32 x1w = ((x0 + 1) % eq_width + eq_width) % eq_width;

                const f32 fx = px - std::floor(px);
                const f32 fy = py - std::floor(py);

                const i32 dst_offset = (j * face_size + i) * pixel_size;

                if (is_hdr)
                {
                    const f32* src = static_cast<const f32*>(eq_data);
                    f32* dst = reinterpret_cast<f32*>(face_ptr + dst_offset);

                    for (i32 c = 0; c < k_desired_channels; c++)
                    {
                        const f32 s00 = src[(y0 * eq_width + x0w) * k_desired_channels + c];
                        const f32 s10 = src[(y0 * eq_width + x1w) * k_desired_channels + c];
                        const f32 s01 = src[(y1 * eq_width + x0w) * k_desired_channels + c];
                        const f32 s11 = src[(y1 * eq_width + x1w) * k_desired_channels + c];

                        dst[c] = (1 - fx) * (1 - fy) * s00 + fx * (1 - fy) * s10 +
                                 (1 - fx) * fy * s01 + fx * fy * s11;
                    }
                }
                else
                {
                    const u8* src = static_cast<const u8*>(eq_data);
                    u8* dst = face_ptr + dst_offset;

                    for (i32 c = 0; c < k_desired_channels; c++)
                    {
                        const f32 s00 = static_cast<f32>(src[(y0 * eq_width + x0w) * k_desired_channels + c]);
                        const f32 s10 = static_cast<f32>(src[(y0 * eq_width + x1w) * k_desired_channels + c]);
                        const f32 s01 = static_cast<f32>(src[(y1 * eq_width + x0w) * k_desired_channels + c]);
                        const f32 s11 = static_cast<f32>(src[(y1 * eq_width + x1w) * k_desired_channels + c]);

                        const f32 val = (1 - fx) * (1 - fy) * s00 + fx * (1 - fy) * s10 +
                                        (1 - fx) * fy * s01 + fx * fy * s11;
                        dst[c] = static_cast<u8>(std::clamp(val + 0.5f, 0.0f, 255.0f));
                    }
                }
            }
        }
    }

    stbi_image_free(eq_data);

    desc.type = TextureType::CubeMap;
    desc.width = face_size;
    desc.height = face_size;

    return Create(desc, {cubemap_data.GetData(), total_bytes}, debug_name);
}

Opal::Expected<Rndr::Canvas::Texture, Rndr::ErrorCode> Rndr::Canvas::Texture::FromFile(const Opal::StringUtf8& file_path,
                                                                                       TextureDesc desc, bool flip_vertically,
                                                                                       Opal::StringUtf8 debug_name)
{
    using ResultType = Opal::Expected<Texture, ErrorCode>;
    if (!Opal::Exists(file_path))
    {
        RNDR_LOG_ERROR("Canvas: Image file does not exist: {}", *file_path);
        return ResultType(ErrorCode::FileNotFound);
    }

    if (debug_name.IsEmpty())
    {
        debug_name = file_path.Clone();
    }

    const Opal::StringUtf8 extension = Opal::Paths::GetExtension(file_path).GetValue();

#if defined(RNDR_KTX)
    if (extension == ".ktx")
    {
        ktxTexture1* ktx_texture = nullptr;
        const KTX_error_code result = ktxTexture1_CreateFromNamedFile(*file_path, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
        if (result != KTX_SUCCESS || ktx_texture == nullptr)
        {
            RNDR_LOG_ERROR("Canvas: Failed to load KTX texture from file: {}", *file_path);
            return ResultType(ErrorCode::CorruptData);
        }

        const Opal::Optional<Format> ktx_format = GlInternalFormatToCanvasFormat(ktx_texture->glInternalformat);
        if (!ktx_format.HasValue())
        {
            RNDR_LOG_ERROR("Canvas: Unsupported GL internal format in KTX file: {}", *file_path);
            ktxTexture_Destroy(reinterpret_cast<ktxTexture*>(ktx_texture));
            return ResultType(ErrorCode::UnsupportedFormat);
        }
        desc.format = ktx_format.GetValue();
        desc.width = static_cast<i32>(ktx_texture->baseWidth);
        desc.height = static_cast<i32>(ktx_texture->baseHeight);

        const i32 depth = static_cast<i32>(ktx_texture->baseDepth);
        const i32 layers = static_cast<i32>(ktx_texture->numLayers);
        if (layers > 1 || depth > 1)
        {
            desc.type = TextureType::Texture2DArray;
            desc.array_size = layers > 1 ? layers : depth;
        }

        const u8* data = ktxTexture_GetData(reinterpret_cast<ktxTexture*>(ktx_texture));
        const u64 data_size = ktxTexture_GetDataSize(reinterpret_cast<ktxTexture*>(ktx_texture));

        ResultType tex_result = Create(desc, {data, data_size}, debug_name);
        ktxTexture_Destroy(reinterpret_cast<ktxTexture*>(ktx_texture));
        return tex_result;
    }
#endif

    stbi_set_flip_vertically_on_load(flip_vertically ? 1 : 0);

    int width = 0;
    int height = 0;
    int channels_in_file = 0;
    constexpr int k_desired_channels = 4;

    u8* pixel_data = nullptr;
    u64 data_size = 0;

    if (stbi_is_hdr(*file_path) > 0)
    {
        f32* data = stbi_loadf(*file_path, &width, &height, &channels_in_file, k_desired_channels);
        if (data == nullptr)
        {
            RNDR_LOG_ERROR("Canvas: Failed to load HDR image {}: {}", *file_path, stbi_failure_reason());
            return ResultType(ErrorCode::CorruptData);
        }
        pixel_data = reinterpret_cast<u8*>(data);
        desc.format = Format::RGBA32F;
        data_size = static_cast<u64>(width) * height * k_desired_channels * sizeof(f32);
    }
    else
    {
        u8* data = stbi_load(*file_path, &width, &height, &channels_in_file, k_desired_channels);
        if (data == nullptr)
        {
            RNDR_LOG_ERROR("Canvas: Failed to load image {}: {}", *file_path, stbi_failure_reason());
            return ResultType(ErrorCode::CorruptData);
        }
        pixel_data = data;
        desc.format = Format::SRGBA8;
        data_size = static_cast<u64>(width) * height * k_desired_channels * sizeof(u8);
    }

    desc.width = width;
    desc.height = height;

    ResultType tex_result = Create(desc, {pixel_data, data_size}, debug_name);
    stbi_image_free(pixel_data);
    return tex_result;
}

Rndr::Canvas::Texture::~Texture()
{
    Destroy();
}

Rndr::Canvas::Texture::Texture(Texture&& other) noexcept
    : m_desc(other.m_desc), m_handle(other.m_handle), m_max_mip_levels(other.m_max_mip_levels), m_name(std::move(other.m_name))
{
    other.m_handle = 0;
    other.m_desc = {};
    other.m_max_mip_levels = 0;
}

Rndr::Canvas::Texture& Rndr::Canvas::Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_handle = other.m_handle;
        m_max_mip_levels = other.m_max_mip_levels;
        m_name = std::move(other.m_name);
        other.m_handle = 0;
        other.m_desc = {};
        other.m_max_mip_levels = 0;
    }
    return *this;
}

Opal::Expected<Rndr::Canvas::Texture, Rndr::ErrorCode> Rndr::Canvas::Texture::Clone() const
{
    using ResultType = Opal::Expected<Texture, ErrorCode>;
    if (!IsValid())
    {
        RNDR_LOG_ERROR("Canvas: Cannot clone an invalid texture");
        return ResultType(ErrorCode::InvalidArgument);
    }

    Texture clone;
    clone.m_desc = m_desc;
    clone.m_max_mip_levels = m_max_mip_levels;
    clone.m_name = m_name.Clone();

    const bool multisample = m_desc.sample_count > 1;
    const GLenum target = ToGLTarget(m_desc.type, multisample);
    const GLFormatInfo fmt = ToGLFormat(m_desc.format);

    glCreateTextures(target, 1, &clone.m_handle);
    if (clone.m_handle == 0)
    {
        RNDR_LOG_ERROR("Canvas: Failed to create GL texture for clone");
        return ResultType(ErrorCode::GraphicsAPIError);
    }

    if (!multisample)
    {
        ApplySamplerParams(clone.m_handle, m_desc, m_max_mip_levels);
    }

    if (m_desc.type == TextureType::Texture2D || m_desc.type == TextureType::CubeMap)
    {
        if (multisample)
        {
            glTextureStorage2DMultisample(clone.m_handle, m_desc.sample_count, fmt.internal_format, m_desc.width, m_desc.height, GL_TRUE);
        }
        else
        {
            glTextureStorage2D(clone.m_handle, m_max_mip_levels, fmt.internal_format, m_desc.width, m_desc.height);
        }
    }
    else if (m_desc.type == TextureType::Texture2DArray)
    {
        if (multisample)
        {
            glTextureStorage3DMultisample(clone.m_handle, m_desc.sample_count, fmt.internal_format, m_desc.width, m_desc.height,
                                          m_desc.array_size, GL_TRUE);
        }
        else
        {
            glTextureStorage3D(clone.m_handle, m_max_mip_levels, fmt.internal_format, m_desc.width, m_desc.height, m_desc.array_size);
        }
    }

    if (!clone.m_name.IsEmpty())
    {
        glObjectLabel(GL_TEXTURE, clone.m_handle, static_cast<GLsizei>(clone.m_name.GetSize()), clone.m_name.GetData());
    }

    // Determine the depth for the copy.
    i32 depth = 1;
    if (m_desc.type == TextureType::Texture2DArray)
    {
        depth = m_desc.array_size;
    }
    else if (m_desc.type == TextureType::CubeMap)
    {
        depth = 6;
    }

    glCopyImageSubData(m_handle, target, 0, 0, 0, 0, clone.m_handle, target, 0, 0, 0, 0, m_desc.width, m_desc.height, depth);
    RNDR_CANVAS_GL_CHECK_EXPECTED("Canvas::Texture::Clone", ResultType);

    return ResultType(std::move(clone));
}

void Rndr::Canvas::Texture::Destroy()
{
    if (m_handle != 0)
    {
        glDeleteTextures(1, &m_handle);
        m_handle = 0;
        m_desc = {};
        m_max_mip_levels = 0;
    }
}

namespace
{

Rndr::ErrorCode ValidateCpuAccess(Rndr::u32 handle, const Rndr::Canvas::TextureDesc& desc)
{
    if (handle == 0)
    {
        RNDR_LOG_ERROR("Canvas: Cannot transfer pixel data for an invalid texture");
        return Rndr::ErrorCode::InvalidArgument;
    }
    if (desc.sample_count > 1)
    {
        RNDR_LOG_ERROR("Canvas: Cannot transfer pixel data for a multi-sample texture");
        return Rndr::ErrorCode::InvalidArgument;
    }
    return Rndr::ErrorCode::Success;
}

Rndr::i32 MipDimension(Rndr::i32 base, Rndr::i32 mip_level)
{
    const Rndr::i32 value = base >> mip_level;
    return value > 1 ? value : 1;
}

}  // namespace

Rndr::ErrorCode Rndr::Canvas::Texture::Update(const Opal::ArrayView<const u8>& data) const
{
    return UpdateRegion(data, 0, 0, m_desc.width, m_desc.height, 0);
}

Rndr::ErrorCode Rndr::Canvas::Texture::UpdateRegion(const Opal::ArrayView<const u8>& data, i32 x, i32 y, i32 width, i32 height,
                                                    i32 mip_level) const
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Texture::UpdateRegion");

    RNDR_CANVAS_CHECK(ValidateCpuAccess(m_handle, m_desc));

    if (m_desc.type != TextureType::Texture2D)
    {
        RNDR_LOG_ERROR("Canvas: UpdateRegion is only supported for Texture2D");
        return ErrorCode::InvalidArgument;
    }
    if (mip_level < 0 || mip_level >= m_max_mip_levels)
    {
        RNDR_LOG_ERROR("Canvas: Mip level is out of range");
        return ErrorCode::OutOfBounds;
    }

    const i32 mip_width = MipDimension(m_desc.width, mip_level);
    const i32 mip_height = MipDimension(m_desc.height, mip_level);
    if (x < 0 || y < 0 || width <= 0 || height <= 0 || x + width > mip_width || y + height > mip_height)
    {
        RNDR_LOG_ERROR("Canvas: Update region is out of bounds");
        return ErrorCode::OutOfBounds;
    }

    const GLFormatInfo fmt = ToGLFormat(m_desc.format);
    const u64 expected_size = static_cast<u64>(width) * height * fmt.pixel_size;
    if (data.GetSize() != expected_size)
    {
        RNDR_LOG_ERROR("Canvas: Update data size does not match the region");
        return ErrorCode::InvalidArgument;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(m_handle, mip_level, x, y, width, height, fmt.format, fmt.type, data.GetData());
    RNDR_CANVAS_GL_CHECK("glTextureSubImage2D");
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Canvas::Texture::UpdateLayer(const Opal::ArrayView<const u8>& data, i32 layer, i32 mip_level) const
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Texture::UpdateLayer");

    RNDR_CANVAS_CHECK(ValidateCpuAccess(m_handle, m_desc));

    constexpr i32 k_cubemap_face_count = 6;
    i32 layer_count = 0;
    if (m_desc.type == TextureType::Texture2DArray)
    {
        layer_count = m_desc.array_size;
    }
    else if (m_desc.type == TextureType::CubeMap)
    {
        layer_count = k_cubemap_face_count;
    }
    else
    {
        RNDR_LOG_ERROR("Canvas: UpdateLayer is only supported for Texture2DArray and CubeMap");
        return ErrorCode::InvalidArgument;
    }

    if (layer < 0 || layer >= layer_count)
    {
        RNDR_LOG_ERROR("Canvas: Layer is out of range");
        return ErrorCode::OutOfBounds;
    }
    if (mip_level < 0 || mip_level >= m_max_mip_levels)
    {
        RNDR_LOG_ERROR("Canvas: Mip level is out of range");
        return ErrorCode::OutOfBounds;
    }

    const i32 mip_width = MipDimension(m_desc.width, mip_level);
    const i32 mip_height = MipDimension(m_desc.height, mip_level);

    const GLFormatInfo fmt = ToGLFormat(m_desc.format);
    const u64 expected_size = static_cast<u64>(mip_width) * mip_height * fmt.pixel_size;
    if (data.GetSize() != expected_size)
    {
        RNDR_LOG_ERROR("Canvas: Update data size does not match the layer");
        return ErrorCode::InvalidArgument;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage3D(m_handle, mip_level, 0, 0, layer, mip_width, mip_height, 1, fmt.format, fmt.type, data.GetData());
    RNDR_CANVAS_GL_CHECK("glTextureSubImage3D");
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Canvas::Texture::UpdateLayerRegion(const Opal::ArrayView<const u8>& data, i32 layer, i32 x, i32 y, i32 width,
                                                         i32 height, i32 mip_level) const
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Texture::UpdateLayerRegion");

    RNDR_CANVAS_CHECK(ValidateCpuAccess(m_handle, m_desc));

    constexpr i32 k_cubemap_face_count = 6;
    i32 layer_count = 0;
    if (m_desc.type == TextureType::Texture2DArray)
    {
        layer_count = m_desc.array_size;
    }
    else if (m_desc.type == TextureType::CubeMap)
    {
        layer_count = k_cubemap_face_count;
    }
    else
    {
        RNDR_LOG_ERROR("Canvas: UpdateLayerRegion is only supported for Texture2DArray and CubeMap");
        return ErrorCode::InvalidArgument;
    }

    if (layer < 0 || layer >= layer_count)
    {
        RNDR_LOG_ERROR("Canvas: Layer is out of range");
        return ErrorCode::OutOfBounds;
    }
    if (mip_level < 0 || mip_level >= m_max_mip_levels)
    {
        RNDR_LOG_ERROR("Canvas: Mip level is out of range");
        return ErrorCode::OutOfBounds;
    }

    const i32 mip_width = MipDimension(m_desc.width, mip_level);
    const i32 mip_height = MipDimension(m_desc.height, mip_level);
    if (x < 0 || y < 0 || width <= 0 || height <= 0 || x + width > mip_width || y + height > mip_height)
    {
        RNDR_LOG_ERROR("Canvas: Update region is out of bounds");
        return ErrorCode::OutOfBounds;
    }

    const GLFormatInfo fmt = ToGLFormat(m_desc.format);
    const u64 expected_size = static_cast<u64>(width) * height * fmt.pixel_size;
    if (data.GetSize() != expected_size)
    {
        RNDR_LOG_ERROR("Canvas: Update data size does not match the region");
        return ErrorCode::InvalidArgument;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage3D(m_handle, mip_level, x, y, layer, width, height, 1, fmt.format, fmt.type, data.GetData());
    RNDR_CANVAS_GL_CHECK("glTextureSubImage3D");
    return ErrorCode::Success;
}

Opal::Expected<Opal::DynamicArray<Rndr::u8>, Rndr::ErrorCode> Rndr::Canvas::Texture::Read(i32 mip_level) const
{
    const i32 mip_width = MipDimension(m_desc.width, mip_level);
    const i32 mip_height = MipDimension(m_desc.height, mip_level);
    return ReadRegion(0, 0, mip_width, mip_height, mip_level);
}

Opal::Expected<Opal::DynamicArray<Rndr::u8>, Rndr::ErrorCode> Rndr::Canvas::Texture::ReadRegion(i32 x, i32 y, i32 width, i32 height,
                                                                                                i32 mip_level) const
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Texture::ReadRegion");
    using ResultType = Opal::Expected<Opal::DynamicArray<u8>, ErrorCode>;

    RNDR_CANVAS_CHECK_EXPECTED(ValidateCpuAccess(m_handle, m_desc), ResultType);

    if (m_desc.type != TextureType::Texture2D)
    {
        RNDR_LOG_ERROR("Canvas: ReadRegion is only supported for Texture2D");
        return ResultType(ErrorCode::InvalidArgument);
    }
    if (mip_level < 0 || mip_level >= m_max_mip_levels)
    {
        RNDR_LOG_ERROR("Canvas: Mip level is out of range");
        return ResultType(ErrorCode::OutOfBounds);
    }

    const i32 mip_width = MipDimension(m_desc.width, mip_level);
    const i32 mip_height = MipDimension(m_desc.height, mip_level);
    if (x < 0 || y < 0 || width <= 0 || height <= 0 || x + width > mip_width || y + height > mip_height)
    {
        RNDR_LOG_ERROR("Canvas: Read region is out of bounds");
        return ResultType(ErrorCode::OutOfBounds);
    }

    const GLFormatInfo fmt = ToGLFormat(m_desc.format);
    const u64 size = static_cast<u64>(width) * height * fmt.pixel_size;

    Opal::DynamicArray<u8> result;
    result.Resize(size);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTextureSubImage(m_handle, mip_level, x, y, 0, width, height, 1, fmt.format, fmt.type, static_cast<GLsizei>(size),
                         result.GetData());
    RNDR_CANVAS_GL_CHECK_EXPECTED("glGetTextureSubImage", ResultType);

    return ResultType(std::move(result));
}

Opal::Expected<Opal::DynamicArray<Rndr::u8>, Rndr::ErrorCode> Rndr::Canvas::Texture::ReadLayer(i32 layer, i32 mip_level) const
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Texture::ReadLayer");
    using ResultType = Opal::Expected<Opal::DynamicArray<u8>, ErrorCode>;

    RNDR_CANVAS_CHECK_EXPECTED(ValidateCpuAccess(m_handle, m_desc), ResultType);

    constexpr i32 k_cubemap_face_count = 6;
    i32 layer_count = 0;
    if (m_desc.type == TextureType::Texture2DArray)
    {
        layer_count = m_desc.array_size;
    }
    else if (m_desc.type == TextureType::CubeMap)
    {
        layer_count = k_cubemap_face_count;
    }
    else
    {
        RNDR_LOG_ERROR("Canvas: ReadLayer is only supported for Texture2DArray and CubeMap");
        return ResultType(ErrorCode::InvalidArgument);
    }

    if (layer < 0 || layer >= layer_count)
    {
        RNDR_LOG_ERROR("Canvas: Layer is out of range");
        return ResultType(ErrorCode::OutOfBounds);
    }
    if (mip_level < 0 || mip_level >= m_max_mip_levels)
    {
        RNDR_LOG_ERROR("Canvas: Mip level is out of range");
        return ResultType(ErrorCode::OutOfBounds);
    }

    const i32 mip_width = MipDimension(m_desc.width, mip_level);
    const i32 mip_height = MipDimension(m_desc.height, mip_level);

    const GLFormatInfo fmt = ToGLFormat(m_desc.format);
    const u64 size = static_cast<u64>(mip_width) * mip_height * fmt.pixel_size;

    Opal::DynamicArray<u8> result;
    result.Resize(size);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTextureSubImage(m_handle, mip_level, 0, 0, layer, mip_width, mip_height, 1, fmt.format, fmt.type,
                         static_cast<GLsizei>(size), result.GetData());
    RNDR_CANVAS_GL_CHECK_EXPECTED("glGetTextureSubImage", ResultType);

    return ResultType(std::move(result));
}

bool Rndr::Canvas::Texture::IsValid() const
{
    return m_handle != 0;
}

const Rndr::Canvas::TextureDesc& Rndr::Canvas::Texture::GetDesc() const
{
    return m_desc;
}

Rndr::i32 Rndr::Canvas::Texture::GetMipLevelCount() const
{
    return m_max_mip_levels;
}

const Opal::StringUtf8& Rndr::Canvas::Texture::GetName() const
{
    return m_name;
}

Rndr::u32 Rndr::Canvas::Texture::GetNativeHandle() const
{
    return m_handle;
}