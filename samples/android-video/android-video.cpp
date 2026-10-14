#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <android/hardware_buffer.h>
#include <android/native_activity.h>
#include <media/NdkImageReader.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>

#include "android_native_app_glue.h"

#include "opal/assert.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/scope-ptr.h"
#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/application.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/descriptor-set.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/frame-context.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/pipeline.hpp"
#include "rndr/forge/shader.hpp"
#include "rndr/forge/swap-chain.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/texture.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/input-system.hpp"
#include "rndr/log.hpp"
#include "rndr/platform/android-application.hpp"
#include "rndr/types.hpp"

// Video frames in Android hardware buffers, imported into Forge and drawn through a sampler YCbCr conversion. Two
// sources, a tap switching between them: a test card the CPU writes into a buffer of its own - colour bars over a grey
// ramp in BT.709 narrow range, the picture samples/video-frame draws - and a clip MediaCodec decodes into an
// AImageReader, played in a loop. The clip is pushed to the app's external files directory as clip.mkv or clip.mp4:
//
//     adb push clip.mkv /sdcard/Android/data/dev.rndr.androidvideo/files/clip.mkv
//
// Without one the test card is all there is.

using u8 = Rndr::u8;
using i32 = Rndr::i32;
using i64 = Rndr::i64;
using u32 = Rndr::u32;
using u64 = Rndr::u64;
using f32 = Rndr::f32;
using f64 = Rndr::f64;

namespace
{

template <typename T>
T Require(Opal::Expected<T, Rndr::ErrorCode>&& result)
{
    if (!result.HasValue())
    {
        Opal::HandleContractViolation("A Forge call failed. The log above says which and why.");
    }
    return std::move(result).GetValue();
}

void RequireOk(Rndr::ErrorCode status)
{
    if (status != Rndr::ErrorCode::Success)
    {
        Opal::HandleContractViolation("A Forge call failed. The log above says which and why.");
    }
}

constexpr u32 k_frames_in_flight = 2;
/** The reader's images: those in flight, the one shown, one arriving, and the decoder's own. */
constexpr i32 k_reader_image_count = 6;
/**
 * How many imported buffers a source keeps. More than a decoder cycles through - fourteen on the emulator with a reader
 * of six images - so that each is imported once; an import shares the buffer's memory, so a kept one costs little.
 */
constexpr i64 k_max_imported_buffers = 32;
constexpr u32 k_card_width = 1280;
constexpr u32 k_card_height = 720;

struct Rgb
{
    f32 r;
    f32 g;
    f32 b;
};

/** The colour at a pixel of the test card: eight bars over the top two thirds, a grey ramp under them. */
Rgb TestCard(u32 x, u32 y)
{
    constexpr Rgb k_bars[] = {{1, 1, 1}, {1, 1, 0}, {0, 1, 1}, {0, 1, 0}, {1, 0, 1}, {1, 0, 0}, {0, 0, 1}, {0, 0, 0}};
    if (y < k_card_height * 2 / 3)
    {
        return k_bars[x * 8 / k_card_width];
    }
    const f32 grey = static_cast<f32>(x) / static_cast<f32>(k_card_width - 1);
    return {grey, grey, grey};
}

/**
 * The test card in a YUV 4:2:0 hardware buffer the CPU fills, in BT.709 narrow range. Where the planes sit and how far
 * apart their samples are is the allocator's choice, so each sample is written through the plane's own strides.
 *
 * @return The buffer, or null when the device cannot allocate one the GPU samples and the CPU writes.
 */
AHardwareBuffer* MakeTestCardBuffer()
{
    const AHardwareBuffer_Desc desc{.width = k_card_width,
                                    .height = k_card_height,
                                    .layers = 1,
                                    .format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
                                    .usage = AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE};
    if (AHardwareBuffer_isSupported(&desc) == 0)
    {
        RNDR_LOG_WARNING(
            "android-video: this device cannot allocate a YUV buffer the CPU writes and the GPU samples, so there is no test card");
        return nullptr;
    }
    AHardwareBuffer* buffer = nullptr;
    if (AHardwareBuffer_allocate(&desc, &buffer) != 0)
    {
        RNDR_LOG_WARNING("android-video: the test card's buffer could not be allocated");
        return nullptr;
    }
    AHardwareBuffer_Planes planes{};
    if (AHardwareBuffer_lockPlanes(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1, nullptr, &planes) != 0 || planes.planeCount != 3)
    {
        RNDR_LOG_WARNING("android-video: the test card's buffer could not be locked as three planes");
        AHardwareBuffer_release(buffer);
        return nullptr;
    }

    constexpr f32 k_kr = 0.2126f;
    constexpr f32 k_kb = 0.0722f;
    const auto luma = [](const Rgb& c) { return k_kr * c.r + (1.0f - k_kr - k_kb) * c.g + k_kb * c.b; };
    const auto put = [](const AHardwareBuffer_Plane& plane, u32 x, u32 y, f32 code)
    { static_cast<u8*>(plane.data)[y * plane.rowStride + x * plane.pixelStride] = static_cast<u8>(code + 0.5f); };
    for (u32 y = 0; y < k_card_height; ++y)
    {
        for (u32 x = 0; x < k_card_width; ++x)
        {
            put(planes.planes[0], x, y, 16.0f + 219.0f * luma(TestCard(x, y)));
        }
    }
    for (u32 y = 0; y < k_card_height / 2; ++y)
    {
        for (u32 x = 0; x < k_card_width / 2; ++x)
        {
            const Rgb c = TestCard(x * 2, y * 2);
            const f32 y_value = luma(c);
            put(planes.planes[1], x, y, 128.0f + 224.0f * (c.b - y_value) / (2.0f * (1.0f - k_kb)));
            put(planes.planes[2], x, y, 128.0f + 224.0f * (c.r - y_value) / (2.0f * (1.0f - k_kr)));
        }
    }
    AHardwareBuffer_unlock(buffer, nullptr);
    return buffer;
}

/** The colour MediaCodec says a stream is in, where it says. */
struct StreamColour
{
    bool has_model = false;
    Rndr::Forge::YcbcrModel model = Rndr::Forge::YcbcrModel::Bt709;
    bool has_range = false;
    Rndr::Forge::YcbcrRange range = Rndr::Forge::YcbcrRange::Narrow;
};

/** Read the colour standard and range a format carries, the MediaFormat constants mapped onto the conversion's. */
void ReadStreamColour(AMediaFormat* format, StreamColour& colour)
{
    i32 standard = 0;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_STANDARD, &standard))
    {
        // COLOR_STANDARD_BT709, BT601_PAL, BT601_NTSC and BT2020.
        colour.has_model = standard == 1 || standard == 2 || standard == 4 || standard == 6;
        colour.model = standard == 1   ? Rndr::Forge::YcbcrModel::Bt709
                       : standard == 6 ? Rndr::Forge::YcbcrModel::Bt2020
                                       : Rndr::Forge::YcbcrModel::Bt601;
    }
    i32 range = 0;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_RANGE, &range))
    {
        // COLOR_RANGE_FULL and COLOR_RANGE_LIMITED.
        colour.has_range = range == 1 || range == 2;
        colour.range = range == 1 ? Rndr::Forge::YcbcrRange::Full : Rndr::Forge::YcbcrRange::Narrow;
    }
}

/**
 * A clip MediaCodec decodes into an AImageReader, played in a loop. Each output is rendered into the reader once the
 * wall clock reaches its time, and the reader hands out the newest as an AImage whose hardware buffer is the frame.
 */
class ClipDecoder
{
public:
    ClipDecoder() = default;
    ~ClipDecoder() { Close(); }
    ClipDecoder(const ClipDecoder&) = delete;
    ClipDecoder& operator=(const ClipDecoder&) = delete;

    /** @return Whether the file is there and has a video track the device decodes. */
    bool Open(const char* path)
    {
        m_file = open(path, O_RDONLY);
        if (m_file < 0)
        {
            return false;
        }
        struct stat file_stat{};
        fstat(m_file, &file_stat);
        m_extractor = AMediaExtractor_new();
        if (AMediaExtractor_setDataSourceFd(m_extractor, m_file, 0, file_stat.st_size) != AMEDIA_OK)
        {
            RNDR_LOG_WARNING("android-video: {} is not a container MediaExtractor reads", path);
            return false;
        }
        AMediaFormat* format = nullptr;
        const char* mime = nullptr;
        for (size_t track = 0; track < AMediaExtractor_getTrackCount(m_extractor); ++track)
        {
            AMediaFormat* candidate = AMediaExtractor_getTrackFormat(m_extractor, track);
            if (AMediaFormat_getString(candidate, AMEDIAFORMAT_KEY_MIME, &mime) && strncmp(mime, "video/", 6) == 0)
            {
                format = candidate;
                AMediaExtractor_selectTrack(m_extractor, track);
                break;
            }
            AMediaFormat_delete(candidate);
        }
        if (format == nullptr)
        {
            RNDR_LOG_WARNING("android-video: {} has no video track", path);
            return false;
        }
        i32 width = 0;
        i32 height = 0;
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
        ReadStreamColour(format, m_colour);

        // Images in a format only the producer and the GPU know, which is what lets the decoder write them directly.
        ANativeWindow* window = nullptr;
        if (AImageReader_newWithUsage(width, height, AIMAGE_FORMAT_PRIVATE, AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, k_reader_image_count,
                                      &m_reader) != AMEDIA_OK ||
            AImageReader_getWindow(m_reader, &window) != AMEDIA_OK)
        {
            RNDR_LOG_WARNING("android-video: no image reader for {}x{} frames", width, height);
            AMediaFormat_delete(format);
            return false;
        }
        m_codec = AMediaCodec_createDecoderByType(mime);
        const bool started = m_codec != nullptr && AMediaCodec_configure(m_codec, format, window, nullptr, 0) == AMEDIA_OK &&
                             AMediaCodec_start(m_codec) == AMEDIA_OK;
        AMediaFormat_delete(format);
        if (!started)
        {
            RNDR_LOG_WARNING("android-video: this device has no decoder for {}", mime);
            return false;
        }
        RNDR_LOG_INFO("android-video: playing {}, {} at {}x{}", path, mime, width, height);
        return true;
    }

    void Close()
    {
        if (m_codec != nullptr)
        {
            AMediaCodec_stop(m_codec);
            AMediaCodec_delete(m_codec);
            m_codec = nullptr;
        }
        if (m_reader != nullptr)
        {
            AImageReader_delete(m_reader);
            m_reader = nullptr;
        }
        if (m_extractor != nullptr)
        {
            AMediaExtractor_delete(m_extractor);
            m_extractor = nullptr;
        }
        if (m_file >= 0)
        {
            close(m_file);
            m_file = -1;
        }
    }

    [[nodiscard]] bool IsOpen() const { return m_codec != nullptr; }
    [[nodiscard]] const StreamColour& GetColour() const { return m_colour; }

    /** Feed the decoder what it takes, and render each output into the reader once its time has come. Never blocks. */
    void Pump(f64 now_seconds)
    {
        while (!m_input_done)
        {
            const ssize_t index = AMediaCodec_dequeueInputBuffer(m_codec, 0);
            if (index < 0)
            {
                break;
            }
            size_t capacity = 0;
            u8* data = AMediaCodec_getInputBuffer(m_codec, index, &capacity);
            const ssize_t size = AMediaExtractor_readSampleData(m_extractor, data, capacity);
            if (size < 0)
            {
                AMediaCodec_queueInputBuffer(m_codec, index, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                m_input_done = true;
                break;
            }
            AMediaCodec_queueInputBuffer(m_codec, index, 0, size, AMediaExtractor_getSampleTime(m_extractor), 0);
            AMediaExtractor_advance(m_extractor);
        }
        while (true)
        {
            if (m_pending_output < 0)
            {
                AMediaCodecBufferInfo info{};
                const ssize_t index = AMediaCodec_dequeueOutputBuffer(m_codec, &info, 0);
                if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
                {
                    AMediaFormat* format = AMediaCodec_getOutputFormat(m_codec);
                    ReadStreamColour(format, m_colour);
                    AMediaFormat_delete(format);
                    continue;
                }
                if (index < 0)
                {
                    break;
                }
                if ((info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0)
                {
                    AMediaCodec_releaseOutputBuffer(m_codec, index, false);
                    Rewind();
                    break;
                }
                m_pending_output = index;
                m_pending_seconds = static_cast<f64>(info.presentationTimeUs) * 1e-6;
            }
            if (m_clock_origin_seconds < 0.0)
            {
                m_clock_origin_seconds = now_seconds - m_pending_seconds;
            }
            if (m_clock_origin_seconds + m_pending_seconds > now_seconds)
            {
                break;
            }
            AMediaCodec_releaseOutputBuffer(m_codec, m_pending_output, true);
            m_pending_output = -1;
        }
    }

    /** The newest image the reader holds, which the caller deletes, or null when nothing new arrived. */
    AImage* TakeLatestImage()
    {
        AImage* image = nullptr;
        return AImageReader_acquireLatestImage(m_reader, &image) == AMEDIA_OK ? image : nullptr;
    }

private:
    void Rewind()
    {
        AMediaExtractor_seekTo(m_extractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
        AMediaCodec_flush(m_codec);
        m_input_done = false;
        m_pending_output = -1;
        m_clock_origin_seconds = -1.0;
    }

    int m_file = -1;
    AMediaExtractor* m_extractor = nullptr;
    AMediaCodec* m_codec = nullptr;
    AImageReader* m_reader = nullptr;
    StreamColour m_colour;
    bool m_input_done = false;
    ssize_t m_pending_output = -1;
    f64 m_pending_seconds = 0.0;
    /** The wall clock time the stream's time zero falls on, negative until the first output sets it. */
    f64 m_clock_origin_seconds = -1.0;
};

/** The conversion's output is gamma encoded already, so it goes to a UNORM swap chain as it is. */
Rndr::Forge::SwapChainDesc MakeSwapChainDesc(const Rndr::Forge::Device& device, const Rndr::Forge::Surface& surface)
{
    const Rndr::Forge::SwapChainSupportDetails support = Require(surface.GetSwapChainSupportDetails(device.GetPhysicalDevice()));
    for (const Rndr::PixelFormat candidate : {Rndr::PixelFormat::R8G8B8A8_UNORM, Rndr::PixelFormat::B8G8R8A8_UNORM})
    {
        for (const VkSurfaceFormatKHR& offered : support.formats)
        {
            if (offered.format == Rndr::ToVkFormat(candidate) && offered.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return {.use_depth = false, .pixel_format = candidate};
            }
        }
    }
    Opal::HandleContractViolation("This surface offers neither R8G8B8A8_UNORM nor B8G8R8A8_UNORM.");
    return {};
}

/** A hardware buffer imported once and drawn each time it comes round again. Never moved: the view and the set refer to the texture. */
struct ImportedBuffer
{
    /** With a reference of this sample's own, so that the pointer is not reused for another buffer while cached. */
    AHardwareBuffer* buffer = nullptr;
    Rndr::Forge::Texture texture;
    Rndr::Forge::TextureView view;
    Rndr::Forge::DescriptorSet set;
    u64 last_frame = 0;

    ~ImportedBuffer()
    {
        set.Destroy();
        view.Destroy();
        texture.Destroy();
        if (buffer != nullptr)
        {
            AHardwareBuffer_release(buffer);
        }
    }
};

/** Push constants of the vertex stage, laid out as android-video.slang's Placement. */
struct Placement
{
    f32 rotation[4];
    f32 scale[2];
};

/**
 * Everything one source's buffers are drawn with, and the buffers imported so far. A conversion is baked into the
 * sampler, the sampler into the layout and the layout into the pipeline, so a change of format or colour builds all
 * of them again.
 */
struct Source
{
    const char* name = "";
    bool is_built = false;
    Rndr::PixelFormat format = Rndr::PixelFormat::Undefined;
    u64 external_format = 0;
    Rndr::Forge::YcbcrModel model = Rndr::Forge::YcbcrModel::Bt709;
    Rndr::Forge::YcbcrRange range = Rndr::Forge::YcbcrRange::Narrow;
    // Declared in the order they are built, so that they go in the reverse.
    Rndr::Forge::SamplerYcbcrConversion conversion;
    Rndr::Forge::Sampler sampler;
    Rndr::Forge::DescriptorSetLayout layout;
    Rndr::Forge::Pipeline pipeline;
    Rndr::Forge::DescriptorPool pool;
    Opal::DynamicArray<Opal::ScopePtr<ImportedBuffer>> buffers;
    /** Every import so far; a stream whose buffers the cache keeps imports each of them once. */
    u64 import_count = 0;

    void Clear()
    {
        buffers.Clear();
        pool.Destroy();
        pipeline.Destroy();
        layout.Destroy();
        sampler.Destroy();
        conversion.Destroy();
        is_built = false;
    }
};

/** What the drawing needs that does not change between sources. */
struct Renderer
{
    Rndr::Forge::Device& device;
    const Rndr::Forge::Shader& vertex_shader;
    const Rndr::Forge::Shader& fragment_shader;
    Rndr::PixelFormat target_format;
};

/**
 * Build what a source draws with for a buffer like this one. The colour is the source's to say when it knows; the
 * driver's suggestion stands otherwise.
 */
void BuildSource(Source& source, const Renderer& renderer, AHardwareBuffer* buffer, const Rndr::Forge::HardwareBufferProperties& properties,
                 const StreamColour& colour)
{
    AHardwareBuffer_Desc buffer_desc{};
    AHardwareBuffer_describe(buffer, &buffer_desc);
    RNDR_LOG_INFO("android-video: {} buffers are AHardwareBuffer format {}, usage {}", source.name, buffer_desc.format, buffer_desc.usage);
    Rndr::Forge::SamplerYcbcrConversionDesc desc = properties.suggested_conversion;
    if (colour.has_model)
    {
        desc.model = colour.model;
    }
    if (colour.has_range)
    {
        desc.range = colour.range;
    }
    RNDR_LOG_INFO("android-video: {} frames are {}x{}, format {}, external format {}, model {}, range {}, chroma filter {}", source.name,
                  properties.width, properties.height, static_cast<u32>(properties.format), properties.external_format.value,
                  static_cast<u32>(desc.model), static_cast<u32>(desc.range), static_cast<u32>(desc.chroma_filter));
    source.conversion = Require(Rndr::Forge::SamplerYcbcrConversion::Create(renderer.device, desc));
    // One mip level, so the mip filter changes nothing, but Linear would need linear filtering an external format may lack.
    source.sampler = Require(Rndr::Forge::Sampler::Create(renderer.device,
                                                          {.min_filter = desc.chroma_filter,
                                                           .mag_filter = desc.chroma_filter,
                                                           .mip_map_filter = Rndr::ImageFilter::Nearest,
                                                           .address_mode_u = Rndr::ImageAddressMode::Clamp,
                                                           .address_mode_v = Rndr::ImageAddressMode::Clamp,
                                                           .address_mode_w = Rndr::ImageAddressMode::Clamp},
                                                          source.conversion));
    Rndr::Forge::DescriptorSetLayoutDesc layout_desc;
    const Opal::Ref<const Rndr::Forge::Sampler> immutable(source.sampler);
    RequireOk(
        layout_desc.AddBinding(0, Rndr::Forge::DescriptorType::CombinedImageSampler, 1, Rndr::ShaderTypeBits::Fragment, {&immutable, 1}));
    source.layout = Require(Rndr::Forge::DescriptorSetLayout::Create(renderer.device, layout_desc));

    // Three descriptors per set covers a format of up to three planes, which an external format gives no count for.
    Rndr::Forge::DescriptorPoolDesc pool_desc;
    RequireOk(pool_desc.Add(Rndr::Forge::DescriptorType::CombinedImageSampler, 3 * (k_max_imported_buffers + k_reader_image_count)));
    pool_desc.max_sets = k_max_imported_buffers + k_reader_image_count;
    // A buffer the cache forgets gives its set back.
    pool_desc.free_individual_sets = true;
    source.pool = Require(Rndr::Forge::DescriptorPool::Create(renderer.device, pool_desc));

    const Opal::Ref<const Rndr::Forge::Shader> shaders[] = {renderer.vertex_shader, renderer.fragment_shader};
    const Rndr::Forge::GraphicsPipelineDesc pipeline_desc{.vertex_shader = renderer.vertex_shader,
                                                          .fragment_shader = renderer.fragment_shader,
                                                          .descriptor_set_layouts = {source.layout},
                                                          .push_constant_ranges = Rndr::Forge::PushConstantRangesFromShaders({shaders, 2}),
                                                          .rasterizer = {.cull_mode = Rndr::Face::None},
                                                          .color_blend_attachments = {Rndr::Forge::ColorBlendDesc{}},
                                                          .color_attachment_formats = {renderer.target_format}};
    source.pipeline = Require(Rndr::Forge::Pipeline::Create(renderer.device, pipeline_desc));
    source.format = properties.format;
    source.external_format = properties.external_format.value;
    source.model = desc.model;
    source.range = desc.range;
    source.is_built = true;
}

/**
 * The imported buffer to draw, from the source's cache or imported now, with the source built or built again first
 * when the buffer or the colour does not match what it was built for.
 */
ImportedBuffer& FindOrImport(Source& source, const Renderer& renderer, AHardwareBuffer* buffer, const StreamColour& colour,
                             u64 frame_number)
{
    for (Opal::ScopePtr<ImportedBuffer>& imported : source.buffers)
    {
        if (imported->buffer == buffer)
        {
            return *imported;
        }
    }
    const Rndr::Forge::HardwareBufferProperties properties = Require(Rndr::Forge::GetHardwareBufferProperties(renderer.device, buffer));
    const bool colour_changed = (colour.has_model && colour.model != source.model) || (colour.has_range && colour.range != source.range);
    if (!source.is_built || properties.format != source.format || properties.external_format.value != source.external_format ||
        colour_changed)
    {
        (void)renderer.device.WaitForAll();
        source.Clear();
        BuildSource(source, renderer, buffer, properties, colour);
    }

    // A full cache forgets the buffer drawn longest ago, once no frame in flight reads it. The reader cycles through a
    // fixed set, so this rarely runs.
    if (static_cast<i64>(source.buffers.GetSize()) >= k_max_imported_buffers)
    {
        i64 oldest = 0;
        for (i64 i = 1; i < static_cast<i64>(source.buffers.GetSize()); ++i)
        {
            if (source.buffers[i]->last_frame < source.buffers[oldest]->last_frame)
            {
                oldest = i;
            }
        }
        if (source.buffers[oldest]->last_frame <= frame_number - k_frames_in_flight)
        {
            source.buffers.EraseWithSwap(source.buffers.begin() + oldest);
        }
    }

    Opal::ScopePtr<ImportedBuffer> imported = Opal::MakeScoped<ImportedBuffer>(nullptr);
    if (!imported.IsValid())
    {
        Opal::HandleContractViolation("Out of memory for an imported buffer.");
    }
    AHardwareBuffer_acquire(buffer);
    imported->buffer = buffer;
    imported->texture = Require(Rndr::Forge::Texture::Create(renderer.device, buffer));
    imported->view = Require(Rndr::Forge::TextureView::Create(renderer.device, imported->texture, source.conversion));
    imported->set = Require(Rndr::Forge::DescriptorSet::Create(source.pool, source.layout));
    RequireOk(imported->set.Update(0, imported->view, source.sampler));
    source.buffers.PushBack(std::move(imported));
    ++source.import_count;
    return *source.buffers.Back();
}

/** An image the reader handed out, kept until no frame in flight reads it. */
struct HeldImage
{
    AImage* image = nullptr;
    /** The last frame that drew it, zero for one that was never drawn. */
    u64 last_frame = 0;
};

void Run(android_app* android_application);

}  // namespace

void android_main(android_app* app)
{
    Run(app);
}

namespace
{

void Run(android_app* android_application)
{
    auto rndr_app = Require(Rndr::Application::Create({.enable_input_system = true, .android_application = android_application}));
    auto window = Require(rndr_app->CreateGenericWindow({.orientation = Rndr::ScreenOrientation::Landscape}));

    Opal::StringUtf8 asset_directory(android_application->activity->internalDataPath);
    asset_directory += "/assets";
    RequireOk(Rndr::AndroidApplication::Get()->ExtractAssets("", asset_directory));

    Rndr::Forge::GraphicsContext graphics_context = Require(Rndr::Forge::GraphicsContext::Create({.collect_debug_messages = true}));
    Rndr::Forge::Surface surface = Require(Rndr::Forge::Surface::Create(graphics_context, *window));
    const Rndr::Forge::DeviceDesc device_desc{.features = {.sampler_ycbcr_conversion = true, .android_hardware_buffer = true},
                                              .surface = surface};
    auto physical_devices = Require(graphics_context.EnumeratePhysicalDevices());
    Rndr::Forge::Device device = Require(Rndr::Forge::Device::Create(
        Require(Rndr::Forge::SelectPhysicalDevice(physical_devices, device_desc)), graphics_context, device_desc));
    Rndr::Forge::DeviceQueue& graphics_queue = Require(device.GetQueue(Rndr::Forge::QueueFamily::Graphics));
    Rndr::Forge::DeviceQueue& present_queue = Require(device.GetQueue(Rndr::Forge::QueueFamily::Present));
    Rndr::Forge::SwapChain swap_chain = Require(Rndr::Forge::SwapChain::Create(device, surface, MakeSwapChainDesc(device, surface)));
    Rndr::Forge::FrameContext frame_context = Require(
        Rndr::Forge::FrameContext::Create(device, swap_chain, graphics_queue, present_queue, {.frames_in_flight = k_frames_in_flight}));

    // No Slang on the device: the build compiled both entry points on the host (rndr_compile_shader).
    const Rndr::Forge::Shader vertex_shader = Require(Rndr::Forge::Shader::FromSpirvFile(
        device, Opal::Paths::Combine(asset_directory, "android-video.main_vertex.spv").GetValue(), {.entry_point = "main_vertex"}));
    const Rndr::Forge::Shader fragment_shader = Require(Rndr::Forge::Shader::FromSpirvFile(
        device, Opal::Paths::Combine(asset_directory, "android-video.main_fragment.spv").GetValue(), {.entry_point = "main_fragment"}));
    const Renderer renderer{device, vertex_shader, fragment_shader, swap_chain.GetDesc().pixel_format};

    // A background and a return hand the activity a new window, and everything built on the surface is built again.
    struct Presentation
    {
        Rndr::Forge::GraphicsContext& graphics_context;
        Rndr::Forge::Device& device;
        Rndr::Forge::DeviceQueue& graphics_queue;
        Rndr::Forge::DeviceQueue& present_queue;
        Rndr::Forge::Surface& surface;
        Rndr::Forge::SwapChain& swap_chain;
        Rndr::Forge::FrameContext& frame_context;
    };
    Presentation presentation{graphics_context, device, graphics_queue, present_queue, surface, swap_chain, frame_context};
    rndr_app->on_window_native_handle_change.Bind(
        [p = &presentation](const Rndr::GenericWindow& changed_window)
        {
            (void)p->device.WaitForAll();
            p->frame_context.Destroy();
            p->swap_chain.Destroy();
            p->surface.Destroy();
            if (changed_window.GetNativeHandle() == nullptr)
            {
                return;
            }
            p->surface = Require(Rndr::Forge::Surface::Create(p->graphics_context, changed_window));
            p->swap_chain = Require(Rndr::Forge::SwapChain::Create(p->device, p->surface, MakeSwapChainDesc(p->device, p->surface)));
            p->frame_context = Require(Rndr::Forge::FrameContext::Create(p->device, p->swap_chain, p->graphics_queue, p->present_queue,
                                                                         {.frames_in_flight = k_frames_in_flight}));
        });

    AHardwareBuffer* card_buffer = MakeTestCardBuffer();
    ClipDecoder decoder;
    const char* external_directory = android_application->activity->externalDataPath;
    for (const char* name : {"/clip.mkv", "/clip.mp4"})
    {
        if (external_directory == nullptr || decoder.IsOpen())
        {
            break;
        }
        Opal::StringUtf8 clip_path(external_directory);
        clip_path += name;
        if (!decoder.Open(clip_path.GetData()))
        {
            decoder.Close();
        }
    }
    if (card_buffer == nullptr && !decoder.IsOpen())
    {
        Opal::HandleContractViolation("Neither a test card nor a clip to show.");
    }

    Source card_source{.name = "test card"};
    Source clip_source{.name = "clip"};
    // The card is in BT.709 narrow range whatever the driver suggests.
    const StreamColour card_colour{
        .has_model = true, .model = Rndr::Forge::YcbcrModel::Bt709, .has_range = true, .range = Rndr::Forge::YcbcrRange::Narrow};
    bool show_clip = decoder.IsOpen();
    Rndr::InputContext& input = rndr_app->GetInputSystemChecked().GetCurrentContext();
    const auto toggle = [&show_clip, &decoder, card_buffer]()
    {
        if (decoder.IsOpen() && card_buffer != nullptr)
        {
            show_clip = !show_clip;
        }
    };
    // A tap is the left mouse button; Space is for the emulator's keyboard.
    input.AddAction("Next source on tap")
        .GetValue()
        .Bind(Rndr::MouseButton::Left, Rndr::Trigger::Pressed)
        .OnMouseButton([toggle](Rndr::MouseButton, Rndr::Trigger, const Rndr::Vector2i&) { toggle(); });
    input.AddAction("Next source on key")
        .GetValue()
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed)
        .OnButton([toggle](Rndr::Trigger, bool) { toggle(); });

    Opal::DynamicArray<HeldImage> held_images;
    AImage* current_image = nullptr;
    // Starts past the frames in flight, so that the frame known complete is never below zero.
    u64 frame_number = k_frames_in_flight;
    u64 drawn_frames = 0;
    u64 clip_frames = 0;
    f64 last_log_seconds = 0.0;
    while (!window->IsClosed())
    {
        rndr_app->ProcessSystemEvents(frame_context.IsValid() ? 0 : Rndr::Application::k_infinite_timeout);
        if (!frame_context.IsValid())
        {
            continue;
        }
        rndr_app->GetInputSystemChecked().ProcessSystemEvents(1.0f / 60.0f);

        const f64 now_seconds = Opal::GetSeconds();
        if (decoder.IsOpen())
        {
            decoder.Pump(now_seconds);
            AImage* image = decoder.TakeLatestImage();
            if (image != nullptr)
            {
                held_images.PushBack({.image = image});
                current_image = image;
                ++clip_frames;
            }
        }

        if (Require(frame_context.BeginFrame()) != Rndr::Forge::SwapChainStatus::Success)
        {
            continue;
        }
        // BeginFrame waited for this slot's fence, so every frame up to this one less the frames in flight is done.
        const u64 completed = frame_number - k_frames_in_flight;
        for (i64 i = 0; i < static_cast<i64>(held_images.GetSize());)
        {
            if (held_images[i].image != current_image && held_images[i].last_frame <= completed)
            {
                AImage_delete(held_images[i].image);
                held_images.EraseWithSwap(held_images.begin() + i);
                continue;
            }
            ++i;
        }

        const bool draws_clip = (show_clip || card_buffer == nullptr) && current_image != nullptr;
        AHardwareBuffer* buffer = card_buffer;
        if (draws_clip)
        {
            AImage_getHardwareBuffer(current_image, &buffer);
        }
        auto& command_buffer = Require(frame_context.GetCommandBuffer());
        RequireOk(command_buffer.CmdTextureBarrier(
            Require(Rndr::Forge::TextureBarrier::ToColorAttachment(Require(frame_context.GetColorTexture())))));
        const Rndr::Vector2i render_size = frame_context.GetRenderSize();
        const Rndr::Forge::RenderingDesc rendering_desc{
            .render_area_extent = render_size,
            .color_attachments = {Rndr::Forge::RenderingAttachmentDesc{.texture = Require(frame_context.GetColorTexture()),
                                                                       .load_operation = Rndr::Forge::AttachmentLoadOperation::Clear,
                                                                       .store_operation = Rndr::Forge::AttachmentStoreOperation::Store,
                                                                       .clear_value = Rndr::Vector4f{0.0f, 0.0f, 0.0f, 1.0f}}}};
        if (buffer == nullptr)
        {
            // The clip has not produced its first frame yet.
            RequireOk(command_buffer.CmdBeginRendering(rendering_desc));
            RequireOk(command_buffer.CmdEndRendering());
            (void)Require(frame_context.EndFrame());
            ++frame_number;
            continue;
        }
        Source& source = draws_clip ? clip_source : card_source;
        ImportedBuffer& imported = FindOrImport(source, renderer, buffer, draws_clip ? decoder.GetColour() : card_colour, frame_number);
        imported.last_frame = frame_number;
        for (HeldImage& held : held_images)
        {
            if (draws_clip && held.image == current_image)
            {
                held.last_frame = frame_number;
            }
        }

        // Letterboxed into the window as the viewer sees it, then turned the way the swap chain is pre-rotated.
        const Rndr::Vector2i window_size = window->GetSize();
        const f32 window_aspect = static_cast<f32>(window_size.x) / static_cast<f32>(window_size.y);
        const f32 frame_aspect = static_cast<f32>(imported.texture.GetDesc().width) / static_cast<f32>(imported.texture.GetDesc().height);
        const Rndr::Matrix4x4f rotation = swap_chain.GetPreRotation();
        const Placement placement{
            .rotation = {rotation.elements[0][0], rotation.elements[0][1], rotation.elements[1][0], rotation.elements[1][1]},
            .scale = {Opal::Min(1.0f, frame_aspect / window_aspect), Opal::Min(1.0f, window_aspect / frame_aspect)}};

        RequireOk(command_buffer.CmdTextureBarrier(Rndr::Forge::TextureBarrier::AcquireFromForeign(imported.texture, graphics_queue)));
        RequireOk(command_buffer.CmdBeginRendering(rendering_desc));
        RequireOk(
            command_buffer.CmdSetViewport(Rndr::Vector2f::Zero(), {static_cast<f32>(render_size.x), static_cast<f32>(render_size.y)}));
        RequireOk(command_buffer.CmdSetScissor(Rndr::Vector2i::Zero(), render_size));
        RequireOk(command_buffer.CmdBindPipeline(source.pipeline));
        RequireOk(command_buffer.CmdBindDescriptorSet(source.pipeline, imported.set));
        RequireOk(command_buffer.CmdPushConstants(source.pipeline, Rndr::ShaderTypeBits::Vertex, Opal::AsBytes(placement)));
        RequireOk(command_buffer.CmdDraw(6));
        RequireOk(command_buffer.CmdEndRendering());
        RequireOk(command_buffer.CmdTextureBarrier(Rndr::Forge::TextureBarrier::ReleaseToForeign(imported.texture, graphics_queue)));
        (void)Require(frame_context.EndFrame());
        ++frame_number;
        ++drawn_frames;

        if (now_seconds - last_log_seconds > 2.0)
        {
            last_log_seconds = now_seconds;
            RNDR_LOG_INFO("android-video: {} frames drawn, {} clip frames decoded, showing the {}, {} buffers cached, {} imports",
                          drawn_frames, clip_frames, source.name, source.buffers.GetSize(), source.import_count);
        }
    }

    (void)device.WaitForAll();
    card_source.Clear();
    clip_source.Clear();
    for (const HeldImage& held : held_images)
    {
        AImage_delete(held.image);
    }
    decoder.Close();
    if (card_buffer != nullptr)
    {
        AHardwareBuffer_release(card_buffer);
    }
}

}  // namespace
