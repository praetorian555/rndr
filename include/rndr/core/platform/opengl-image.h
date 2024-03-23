#pragma once

#include "rndr/core/definitions.h"
#include "rndr/core/graphics-types.h"
#include "rndr/core/platform/opengl-forward-def.h"

namespace Rndr
{

class GraphicsContext;
class Bitmap;

/**
 * Represents a texture on the GPU.
 */
class Image
{
public:
    /**
     * Default constructor. Creates an invalid image.
     */
    Image() = default;

    /**
     * Creates a new image. Only creates Image2D so any other type will result in invalid image.
     * @param graphics_context The graphics context to create the image with.
     * @param desc The description of the image to create.
     * @param init_data The initial data to fill the image with. Default is empty.
     */
    Image(const GraphicsContext& graphics_context, const ImageDesc& desc, const ConstByteSpan& init_data = ConstByteSpan{});

    /**
     * Creates a new image from a CPU image. Only creates Image2D so any other type will result in
     * invalid image.
     * @param graphics_context The graphics context to create the image with.
     * @param Bitmap The CPU bitmap to create the image with.
     * @param use_mips Whether or not to use mips for the image.
     * @param sampler_desc The sampler description to use for the image.
     */
    Image(const GraphicsContext& graphics_context, Bitmap& bitmap, bool use_mips, const SamplerDesc& sampler_desc);

    ~Image();
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const ImageDesc& GetDesc() const;
    [[nodiscard]] GLuint GetNativeTexture() const;
    [[nodiscard]] uint64_t GetBindlessHandle() const;

private:
    ImageDesc m_desc;
    GLuint m_native_texture = k_invalid_opengl_object;
    uint64_t m_bindless_handle = k_invalid_opengl_object;
};

}  // namespace Rndr
