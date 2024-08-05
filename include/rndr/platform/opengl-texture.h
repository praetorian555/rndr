#pragma once

#include "rndr/definitions.h"
#include "rndr/error-codes.h"
#include "rndr/graphics-types.h"
#include "rndr/platform/opengl-forward-def.h"

namespace Rndr
{

class GraphicsContext;
class Bitmap;

/**
 * Represents a texture on the GPU.
 */
class Texture
{
public:
    /**
     * Default constructor. Creates an invalid image.
     */
    Texture() = default;

    /**
     * Creates a new image. Only creates Texture2D so any other type will result in invalid image.
     * @param graphics_context The graphics context to create the image with.
     * @param texture_desc The description of the image to create.
     * @param sampler_desc The sampler description to use for the image.
     * @param init_data The initial data to fill the image with. Default is empty.
     */
    Texture(const GraphicsContext& graphics_context, const TextureDesc& texture_desc, const SamplerDesc& sampler_desc = SamplerDesc{},
            const Opal::Span<const u8>& init_data = {});

    /**
     * Initializes the texture on the GPU.
     * @param graphics_context The graphics context to create the image with.
     * @param texture_desc The description of the texture to create.
     * @param sampler_desc The sampler description to use for the texture.
     * @param init_data The initial data to fill the texture with. If empty, the contents of the allocated texture will be undefined.
     * Default is empty.
     * @return Returns ErrorCode::Success if the texture was successfully created, ErrorCode::GraphicsAPIError if there was an error in one
     * of the OpenGL calls. ErrorCode::InvalidArgument if the texture description is invalid. ErrorCode::OutOfMemory if there was not enough
     * memory to allocate the texture.
     * @note If the setup fails, the internal
     */
    ErrorCode Initialize(const GraphicsContext& graphics_context, const TextureDesc& texture_desc,
                         const SamplerDesc& sampler_desc = SamplerDesc{}, const Opal::Span<const u8>& init_data = {});

    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const TextureDesc& GetTextureDesc() const;
    [[nodiscard]] const SamplerDesc& GetSamplerDesc() const;
    [[nodiscard]] GLuint GetNativeTexture() const;
    [[nodiscard]] uint64_t GetBindlessHandle() const;

private:
    TextureDesc m_texture_desc;
    SamplerDesc m_sampler_desc;
    GLuint m_native_texture = k_invalid_opengl_object;
    uint64_t m_bindless_handle = k_invalid_opengl_object;

    i32 m_max_mip_levels = 0;
};

}  // namespace Rndr
