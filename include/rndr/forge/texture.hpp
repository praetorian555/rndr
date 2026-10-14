#pragma once

#include "volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/container/optional.h"
#include "opal/container/ref.h"

#include "rndr/bitmap.hpp"
#include "rndr/definitions.hpp"
#include "rndr/error-codes.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/types.hpp"
#include "rndr/forge/video.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"

// Forward declare handle to avoid vma includes in headers.
using VmaAllocation = struct VmaAllocation_T*;

#if RNDR_ANDROID
struct AHardwareBuffer;
#endif

namespace Rndr::Forge
{

struct TextureDesc
{
    // Image
    TextureDimension dimension = TextureDimension::Texture2D;
    PixelFormat format = PixelFormat::B8G8R8A8_UNORM;
    u32 width = 0;
    u32 height = 0;
    u32 depth = 1;
    u32 mip_level_count = 1;
    u32 array_layer_count = 1;
    SampleCount sample_count = SampleCount::Count1;
    TextureUsageBits usage = TextureUsageBits::Sampled;

    // Image view
    TextureViewType view_type = TextureViewType::Texture2D;
    ImageSubresourceRange subresource_range;
    /**
     * The stream a texture with TextureUsageBits::VideoDecodeDestination or VideoDecodeDpb is decoded for. Only read with
     * one of those usages.
     */
    VideoProfile video_profile;
};

class Texture
{
public:
    Texture() = default;
    ~Texture();

    /**
     * Create the image and, unless its usage is transfer only, a view of it. A texture of a YCbCr format gets no view
     * of its own, since Vulkan allows one only with a conversion: it is read through a TextureView created with one.
     * Such a texture is two dimensional, one mip level, one layer, one sample, of an even width and height, and
     * sampled or copied only.
     *
     * @param device Device to allocate from. Has to outlive the texture.
     * @param desc Extent, format, usage and what the view covers.
     * @return The texture, ErrorCode::InvalidArgument when the desc names something it cannot - a cube view
     *         over an array that is not a multiple of six, an enum value that maps to nothing - or whatever
     *         the failing allocation maps to.
     */
    [[nodiscard]] static Opal::Expected<Texture, ErrorCode> Create(const Device& device, const TextureDesc& desc = {});

    /**
     * Upload a bitmap into a new texture, blocking until the copy is done.
     *
     * @param bitmap Source pixels. Its extent, format and mip count are taken over the ones in the desc.
     * @param generate_mips Fill the levels below the first by blitting, for a bitmap that carries only mip 0.
     *                      The full mip chain of the extent is created, and both transfer usages are added.
     * @return The texture, or what the creation, the staging buffer or the upload reported.
     */
    [[nodiscard]] static Opal::Expected<Texture, ErrorCode> Create(const Device& device, DeviceQueue& queue, const Bitmap& bitmap,
                                                                   const TextureDesc& desc = {}, bool generate_mips = false);

    /**
     * Wrap an image Forge did not create - a swap chain one - and give it a view. The image is not owned and
     * is not released with the texture; the view is.
     */
    [[nodiscard]] static Opal::Expected<Texture, ErrorCode> Create(const Device& device, VkImage native_image,
                                                                   const TextureDesc& desc = {});

#if RNDR_ANDROID
    /**
     * Import an Android hardware buffer - a frame a video decoder wrote into an AImageReader, a camera image - as a
     * texture shaders sample. Nothing is copied: the texture's memory is the buffer's, in a dedicated allocation.
     *
     * A buffer in a format Vulkan has a name for becomes a texture of that format. One in a format only the driver
     * understands, which is what a decoder usually hands out, becomes a texture of PixelFormat::Undefined whose
     * GetExternalFormat is the driver's value; like a YCbCr texture it has no view of its own and is read through a
     * SamplerYcbcrConversion over that external format (GetHardwareBufferProperties gives one). The usage is Sampled.
     *
     * Vulkan holds a reference to the buffer while the texture lives, but nothing keeps whoever handed it out from
     * writing into it again: an AImage is kept until a fence says the GPU has finished every command reading the
     * texture, and only then given back. A frame that reads the texture acquires it from k_foreign_queue_family first
     * (TextureBarrier::AcquireFromForeign) and releases it back after (TextureBarrier::ReleaseToForeign).
     *
     * @param device Device created with DeviceFeatures::android_hardware_buffer. Has to outlive the texture.
     * @param buffer The buffer: one layer, created with AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE.
     * @return The texture, ErrorCode::InvalidArgument when the device lacks the feature, the buffer is null, has more
     *         than one layer or was not made to be sampled, or a YCbCr format's rules refuse it (see Create above), or
     *         whatever the failing query, creation or allocation maps to.
     */
    [[nodiscard]] static Opal::Expected<Texture, ErrorCode> Create(const Device& device, AHardwareBuffer* buffer);
#endif

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_image != VK_NULL_HANDLE; }
    [[nodiscard]] VkImage GetNativeImage() const { return m_image; }
    /** Null for a texture whose usage is transfer only, since Vulkan allows no view on one. */
    [[nodiscard]] VkImageView GetNativeImageView() const { return m_view; }
    [[nodiscard]] const TextureDesc& GetDesc() const { return m_desc; }
    /**
     * The driver's format of an imported Android hardware buffer that Vulkan has no name for, or zero for every other
     * texture. One that has it is of PixelFormat::Undefined and is read through a conversion over the same value.
     */
    [[nodiscard]] u64 GetExternalFormat() const { return m_external_format; }

    /**
     * Where the image's memory landed. A transient attachment lands in LazilyAllocated memory when the device
     * has a type the image can use, which is the only way to tell whether it did. Empty for an empty texture
     * and for one wrapping an image Forge did not allocate, such as a swap chain's.
     */
    [[nodiscard]] Opal::Optional<MemoryInfo> GetMemoryInfo() const;

    /**
     * The layout every subresource of the texture is in, which is what a barrier transitions out of. Vulkan
     * keeps no such thing, so this is Forge's own bookkeeping: it starts at ImageLayout::Undefined, the way a
     * freshly created image does, and CommandBuffer moves it on as it records barriers.
     *
     * That makes it a record-time answer rather than an execution-time one - see the barriers section of
     * docs/forge.md for where it stops being true.
     *
     * @return The layout the whole texture is in, or ErrorCode::InvalidArgument when the subresources are
     *         not all in the same one - which is what mip generation leaves behind halfway through. Ask per
     *         subresource instead.
     */
    [[nodiscard]] Opal::Expected<ImageLayout, ErrorCode> GetCurrentLayout() const;

    /**
     * The layout of one subresource.
     * @return The layout, or ErrorCode::OutOfBounds when the texture has no such subresource.
     */
    [[nodiscard]] Opal::Expected<ImageLayout, ErrorCode> GetCurrentLayout(u32 mip_level, u32 array_layer = 0) const;

    /** The layout of a range, answering the way the whole-texture form does when the range is not uniform. */
    [[nodiscard]] Opal::Expected<ImageLayout, ErrorCode> GetCurrentLayout(const ImageSubresourceRange& range) const;

private:
    /**
     * Only CommandBuffer records the barriers that move a layout, and only SwapChain re-acquires an image. A
     * TextureView checks its range the way a barrier does.
     */
    friend class CommandBuffer;
    friend class SwapChain;
    friend class TextureView;

    /** Write a layout over every subresource a range covers, resolving the k_all_* counts against the desc. */
    ErrorCode SetCurrentLayout(const ImageSubresourceRange& range, ImageLayout layout);

    /**
     * Whether a range names only subresources this texture has, which is what SetCurrentLayout checks and what
     * a barrier has to pass before it is recorded rather than after.
     * @return ErrorCode::Success, or ErrorCode::OutOfBounds when the range reaches past the texture.
     */
    [[nodiscard]] ErrorCode CheckRange(const ImageSubresourceRange& range) const;

    ErrorCode Init(const Device& device, const TextureDesc& desc);

    TextureDesc m_desc;
    /** One entry per subresource, mip level major: mip_level * array_layer_count + array_layer. */
    Opal::DynamicArray<ImageLayout> m_layouts;
    Opal::Ref<const Device> m_device;
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VmaAllocation m_image_allocation = VK_NULL_HANDLE;
    /** The memory of an imported hardware buffer, which VMA does not know about. */
    VkDeviceMemory m_imported_memory = VK_NULL_HANDLE;
    /** The image was created here for an imported buffer, and is destroyed here with the memory beside it. */
    bool m_is_imported = false;
    u64 m_external_format = 0;
};

/** The matrix a SamplerYcbcrConversion turns YCbCr into RGB with. Mirrors VkSamplerYcbcrModelConversion. */
enum class YcbcrModel : u8
{
    /** No conversion: the samples are RGB already, and only the chroma planes are rebuilt to full size. */
    RgbIdentity = 0,
    /** The range is expanded but no matrix is applied: Y, Cb and Cr come out in G, B and R. */
    YcbcrIdentity = 1,
    /** BT.709, high definition video and most H.264 and 8-bit HEVC. */
    Bt709 = 2,
    /** BT.601, standard definition video. */
    Bt601 = 3,
    /** BT.2020, ultra high definition and HDR video, most 10-bit HEVC. */
    Bt2020 = 4,
};

/** Which values the samples span. Mirrors VkSamplerYcbcrRange. */
enum class YcbcrRange : u8
{
    /** Every code: 0 to 255 in 8 bits. */
    Full = 0,
    /** The broadcast range most video uses: Y from 16 to 235 and chroma from 16 to 240 in 8 bits, 64 to 940 and 960 in 10. */
    Narrow = 1,
};

/** Where a chroma sample sits against the luma samples it covers, on one axis. Mirrors VkChromaLocation. */
enum class ChromaLocation : u8
{
    /** Level with the even luma sample: H.264 and HEVC put the chroma here horizontally. */
    CositedEven = 0,
    /** Halfway between two luma samples: H.264 and HEVC put the chroma here vertically. */
    Midpoint = 1,
};

/**
 * A format only the driver understands, which an Android hardware buffer can be in (GetHardwareBufferProperties). Vulkan
 * samples an image of one only through a conversion created for the same value, and says what that conversion may do
 * in the buffer's format features rather than in the device's, so they travel with the value.
 */
struct ExternalFormat
{
    /** The driver's value. Zero for none, which is every format but an Android hardware buffer's. */
    u64 value = 0;
    /**
     * What a conversion over the format may do. combined_image_sampler_descriptor_count is zero: Vulkan has no query
     * for an external format, and a pool sized for three per descriptor covers a format of up to three planes.
     */
    YcbcrFormatSupport support;
    /** How the driver's components map onto Y, Cb and Cr, which the conversion has to apply. Identity for most. */
    VkComponentMapping components{};
};

/**
 * How a SamplerYcbcrConversion reads a video frame. The defaults are those of high definition H.264 and HEVC: BT.709,
 * narrow range, chroma level with the left luma column and between the two rows, rebuilt linearly.
 */
struct SamplerYcbcrConversionDesc
{
    /**
     * The format of the frames it reads, one that IsYcbcrFormat reports. Views and textures of it have to match.
     * PixelFormat::Undefined with an external_format instead.
     */
    PixelFormat format = PixelFormat::G8_B8R8_2PLANE_420_UNORM;
    /** The external format of an imported Android hardware buffer, which needs format Undefined. Only on Android. */
    ExternalFormat external_format;
    YcbcrModel model = YcbcrModel::Bt709;
    YcbcrRange range = YcbcrRange::Narrow;
    ChromaLocation x_chroma_offset = ChromaLocation::CositedEven;
    ChromaLocation y_chroma_offset = ChromaLocation::Midpoint;
    /** How the chroma is rebuilt between its samples. Linear needs YcbcrFormatSupport::linear_chroma_filter. */
    ImageFilter chroma_filter = ImageFilter::Linear;
    /** Rebuild the chroma as a step of its own where the device would otherwise fold it into the filtering. */
    bool force_explicit_reconstruction = false;
};

/**
 * What turns the planes of a video frame into RGB as it is sampled: the matrix, the range and where the chroma
 * sits. It goes into the two places Vulkan reads it from, a Sampler and a TextureView, which have to carry the
 * same one; a shader then samples the frame as an ordinary texture and gets RGB.
 *
 * A sampler with a conversion can only be used as an immutable sampler of a CombinedImageSampler binding, so the
 * layout, and the pipeline built on it, is made for one kind of stream. A stream with a different matrix or range
 * needs a conversion, a layout and a pipeline of its own.
 */
class SamplerYcbcrConversion
{
public:
    SamplerYcbcrConversion() = default;
    ~SamplerYcbcrConversion();

    /**
     * @param desc The format and how to read it.
     * @return The conversion, or ErrorCode::InvalidArgument when the device was created without
     *         DeviceFeatures::sampler_ycbcr_conversion, the format is not a YCbCr one, an external format is given
     *         beside a format, off Android or without DeviceFeatures::android_hardware_buffer, or the desc asks for
     *         what the device does not allow of the format (see PhysicalDevice::GetYcbcrFormatSupport, or the external
     *         format's support) - the log names which - or whatever the failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<SamplerYcbcrConversion, ErrorCode> Create(const Device& device,
                                                                                  const SamplerYcbcrConversionDesc& desc = {});

    SamplerYcbcrConversion(const SamplerYcbcrConversion&) = delete;
    SamplerYcbcrConversion& operator=(const SamplerYcbcrConversion&) = delete;
    SamplerYcbcrConversion(SamplerYcbcrConversion&& other) noexcept;
    SamplerYcbcrConversion& operator=(SamplerYcbcrConversion&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_conversion != VK_NULL_HANDLE; }
    [[nodiscard]] VkSamplerYcbcrConversion GetNativeConversion() const { return m_conversion; }
    [[nodiscard]] const SamplerYcbcrConversionDesc& GetDesc() const { return m_desc; }
    /** What the format it reads allows: the device's answer for a format, the buffer's for an external one. */
    [[nodiscard]] const YcbcrFormatSupport& GetFormatSupport() const { return m_support; }

private:
    Opal::Ref<const Device> m_device;
    VkSamplerYcbcrConversion m_conversion = VK_NULL_HANDLE;
    SamplerYcbcrConversionDesc m_desc;
    YcbcrFormatSupport m_support;
};

#if RNDR_ANDROID
/** What the driver says of an Android hardware buffer: how Texture::Create imports it, and how it is read. */
struct HardwareBufferProperties
{
    u32 width = 0;
    u32 height = 0;
    /** The buffer's format where Vulkan has a name for it, PixelFormat::Undefined where only the driver knows it. */
    PixelFormat format = PixelFormat::Undefined;
    /**
     * The driver's own format, which a texture of PixelFormat::Undefined is imported in and its conversion created
     * for. Set for every buffer, but only used when format is Undefined.
     */
    ExternalFormat external_format;
    /**
     * A conversion for the buffer, with its format or external format filled in and the matrix, range and chroma siting
     * the driver suggests - what it believes the writer used. A decoder that reports the stream's colour (the colour
     * standard and range of AMediaCodec's output format) knows better. The chroma filter is linear where allowed.
     */
    SamplerYcbcrConversionDesc suggested_conversion;
};

/**
 * Ask the driver about an Android hardware buffer before importing it. A stream's frames come in a handful of buffers
 * that share these, so it is asked once to build the conversion, the layout and the pipeline the frames are drawn with;
 * a change of external format means those are built again.
 *
 * @param device Device created with DeviceFeatures::android_hardware_buffer.
 * @param buffer The buffer.
 * @return The properties, ErrorCode::InvalidArgument when the device lacks the feature or the buffer is null, or what
 *         the failing query maps to.
 */
[[nodiscard]] Opal::Expected<HardwareBufferProperties, ErrorCode> GetHardwareBufferProperties(const Device& device,
                                                                                              AHardwareBuffer* buffer);
#endif

/** What a TextureView sees of its texture: how the image is interpreted, and which part of it. */
struct TextureViewDesc
{
    TextureViewType view_type = TextureViewType::Texture2D;
    /**
     * What the view is used for, a subset of the texture's usage; None for all of it. A view of a YCbCr texture that is
     * not sampled - the picture a video decode writes - needs no conversion.
     */
    TextureUsageBits usage = TextureUsageBits::None;
    /**
     * The mip levels, array layers and aspect the view covers - every one of them by default. A view of a
     * combined depth-stencil format that a shader samples has to name one of the two aspects.
     */
    ImageSubresourceRange subresource_range;
};

/**
 * A second view of a texture's image, apart from the one the texture carries. What a texture needs more than
 * one of: a mip chain written a level at a time as storage images and sampled whole, one layer of an array
 * rendered into while the rest is sampled, one level of a texture rendered into.
 *
 * It does not own the image and keeps no layout of its own. The layout it is used in is the one the texture
 * tracks for the range the view covers, so barriers are still recorded on the texture - with its
 * subresource_range narrowed to this view's - and a view never disagrees with its texture about where the
 * image is. The texture has to outlive every view of it.
 */
class TextureView
{
public:
    TextureView() = default;
    ~TextureView();

    /**
     * @param device Device the texture was created on. Has to outlive the view.
     * @param texture Texture whose image the view is of. Has to outlive the view as well.
     * @param desc The view type and the range.
     * @return The view, ErrorCode::OutOfBounds when the range names mips or layers the texture does not have,
     *         ErrorCode::InvalidArgument for an empty texture, one whose usage allows no view, or a view type
     *         the image cannot take - a cube over a texture not created cube compatible or over a layer count
     *         that is not a multiple of six, a flat view over several layers, a view of another dimension,
     *         a cube array without DeviceFeatures::image_cube_array - or whatever the failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<TextureView, ErrorCode> Create(const Device& device, const Texture& texture,
                                                                       const TextureViewDesc& desc = {});

    /**
     * A view of a video frame, read as RGB through a conversion. The only way to sample a texture of a YCbCr format,
     * which has no view of its own; it is bound through a CombinedImageSampler binding whose immutable sampler carries
     * the same conversion.
     * @param conversion How the frame is read. It has to outlive the view, and its format and external format have to
     *                   be the texture's.
     * @return The view, ErrorCode::InvalidArgument when the conversion is empty or of another format or external
     *         format, or what the overload above refuses.
     */
    [[nodiscard]] static Opal::Expected<TextureView, ErrorCode> Create(const Device& device, const Texture& texture,
                                                                       const SamplerYcbcrConversion& conversion,
                                                                       const TextureViewDesc& desc = {});

    TextureView(const TextureView&) = delete;
    TextureView& operator=(const TextureView&) = delete;
    TextureView(TextureView&& other) noexcept;
    TextureView& operator=(TextureView&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_view != VK_NULL_HANDLE; }
    [[nodiscard]] VkImageView GetNativeImageView() const { return m_view; }
    [[nodiscard]] const TextureViewDesc& GetDesc() const { return m_desc; }
    /** The texture this is a view of, which is where its layout is tracked. */
    [[nodiscard]] const Texture& GetTexture() const { return *m_texture; }
    /** The conversion the view reads through, or VK_NULL_HANDLE for a view without one. */
    [[nodiscard]] VkSamplerYcbcrConversion GetNativeConversion() const { return m_conversion; }

private:
    [[nodiscard]] static Opal::Expected<TextureView, ErrorCode> CreateView(const Device& device, const Texture& texture,
                                                                           const SamplerYcbcrConversion* conversion,
                                                                           const TextureViewDesc& desc);

    Opal::Ref<const Device> m_device;
    Opal::Ref<const Texture> m_texture;
    VkImageView m_view = VK_NULL_HANDLE;
    VkSamplerYcbcrConversion m_conversion = VK_NULL_HANDLE;
    TextureViewDesc m_desc;
};

class Sampler
{
public:
    Sampler() = default;
    ~Sampler();

    /**
     * @param desc Filters, address modes and the anisotropy and LOD range.
     * @return The sampler, ErrorCode::InvalidArgument when the desc asks for something this device was not
     *         created with - anisotropy, MirrorOnce - or names a value that maps to nothing, or whatever the
     *         failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<Sampler, ErrorCode> Create(const Device& device, const SamplerDesc& desc = {});

    /**
     * A sampler that reads video frames as RGB through a conversion. Vulkan takes it only as an immutable sampler of a
     * CombinedImageSampler binding, beside a TextureView made with the same conversion.
     * @param conversion How the frames are read. It has to outlive the sampler.
     * @return The sampler, ErrorCode::InvalidArgument when the conversion is empty or the desc breaks what a sampler
     *         with one has to keep to: every address mode Clamp, no anisotropy, no comparison or reduction, and
     *         min_filter and mag_filter equal to the conversion's chroma_filter unless the format allows a separate
     *         reconstruction filter. The log names which.
     */
    [[nodiscard]] static Opal::Expected<Sampler, ErrorCode> Create(const Device& device, const SamplerDesc& desc,
                                                                   const SamplerYcbcrConversion& conversion);

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
    Sampler(Sampler&& other) noexcept;
    Sampler& operator=(Sampler&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_sampler != VK_NULL_HANDLE; }
    [[nodiscard]] VkSampler GetNativeSampler() const { return m_sampler; }
    /** The conversion the sampler reads through, or VK_NULL_HANDLE for a sampler without one. */
    [[nodiscard]] VkSamplerYcbcrConversion GetNativeConversion() const { return m_conversion; }

private:
    [[nodiscard]] static Opal::Expected<Sampler, ErrorCode> CreateSampler(const Device& device, const SamplerDesc& desc,
                                                                          const SamplerYcbcrConversion* conversion);

    Opal::Ref<const Device> m_device;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkSamplerYcbcrConversion m_conversion = VK_NULL_HANDLE;
};

}  // namespace Rndr::Forge
