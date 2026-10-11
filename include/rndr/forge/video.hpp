#pragma once

#include "volk.h"

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/container/ref.h"

#include "rndr/error-codes.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/math.hpp"
#include "rndr/pixel-format.hpp"
#include "rndr/types.hpp"

// Forward declare handle to avoid vma includes in headers.
using VmaAllocation = struct VmaAllocation_T*;

namespace Rndr::Forge
{

/** Which codec a video profile is for. */
enum class VideoCodec : u8
{
    H264,
    H265,
    Av1,
};

/**
 * What a stream is, as far as the device is concerned: the codec, its profile and the depth of its samples. A video
 * session, the images it decodes into and the buffer it decodes from are all created for one, and have to agree.
 * Chroma is always 4:2:0 and an H.264 stream always progressive, which is what the player decodes.
 */
struct VideoProfile
{
    VideoCodec codec = VideoCodec::H264;
    /**
     * The codec's own profile: profile_idc for H.264 (66 Baseline, 77 Main, 100 High) and H.265 (1 Main, 2 Main 10),
     * seq_profile for AV1 (0 Main).
     */
    u32 codec_profile = 100;
    /** Bits per sample, luma and chroma alike: 8, or 10 for HEVC Main 10 and 10-bit AV1. */
    u32 bit_depth = 8;
};

/** What a device can decode under one profile, from GetVideoDecodeCapabilities. */
struct VideoCapabilities
{
    /** The smallest and the largest picture a session for the profile can decode, in pixels. */
    Vector2i min_coded_extent;
    Vector2i max_coded_extent;
    /** The granularity a decoded picture's extent is rounded up to inside the images it is written to. */
    Vector2i picture_access_granularity;
    /** How many reference pictures a session can keep, and how many one decode can use. */
    u32 max_dpb_slots = 0;
    u32 max_active_reference_pictures = 0;
    /** What the offset and the size of the bitstream handed to one decode have to be multiples of. */
    u64 bitstream_offset_alignment = 1;
    u64 bitstream_size_alignment = 1;
    /** Whether each reference picture may be an image of its own rather than a layer of one shared image. */
    bool separate_reference_images = false;
    /** Whether a decoded picture can be written straight into the reference picture image and read from there. */
    bool dpb_and_output_coincide = false;
    /** Whether a decoded picture can be written to an output image apart from the reference picture images. */
    bool dpb_and_output_distinct = false;
    /** The highest level the device decodes: level_idc for H.264 and H.265, seq_level_idx for AV1. */
    u32 max_level = 0;
    /** The version of the codec's standard headers the driver implements, which a session is created with. */
    VkExtensionProperties std_header_version = {};
    /** The formats a reference picture image can have. */
    Opal::DynamicArray<PixelFormat> dpb_formats;
    /**
     * The formats an output image can have, sampled afterwards: the ones that are also reference picture formats when
     * the two coincide, and the ones an image apart from them can have otherwise.
     */
    Opal::DynamicArray<PixelFormat> output_formats;
};

/**
 * Ask the device what it decodes under a profile.
 * @param physical_device The device.
 * @param profile The stream's codec, profile and sample depth.
 * @return The capabilities, ErrorCode::InvalidArgument for a profile Forge does not describe (a bit depth other than 8
 *         or 10), or ErrorCode::FeatureNotSupported when the device lacks the extensions or does not decode the profile -
 *         the log says which.
 */
[[nodiscard]] Opal::Expected<VideoCapabilities, ErrorCode> GetVideoDecodeCapabilities(const PhysicalDevice& physical_device,
                                                                                      const VideoProfile& profile);

/** What a video session decodes: the stream's profile, the largest picture and how many reference pictures it keeps. */
struct VideoSessionDesc
{
    VideoProfile profile;
    /** The largest picture the session decodes, in pixels: the stream's coded size, whole macroblocks or blocks. */
    Vector2i max_coded_extent = {1920, 1088};
    /** The format of the pictures it writes out, one of VideoCapabilities::output_formats. */
    PixelFormat picture_format = PixelFormat::G8_B8R8_2PLANE_420_UNORM;
    /** The format of the reference pictures it keeps, one of VideoCapabilities::dpb_formats. */
    PixelFormat reference_picture_format = PixelFormat::G8_B8R8_2PLANE_420_UNORM;
    /** How many reference picture slots it has: H.264 needs one per reference frame and one for the picture decoded. */
    u32 max_dpb_slots = 17;
    /** How many references one decode can use. */
    u32 max_active_reference_pictures = 16;
};

/**
 * The decoder's own state for one stream, on the decode queue: what it remembers between pictures, in memory the
 * device asks for and Forge binds. Every decode command runs inside a CmdBeginVideoCoding over a session, with the
 * session parameters of the stream.
 */
class VideoSession
{
public:
    VideoSession() = default;
    ~VideoSession();

    /**
     * @param desc The profile, the largest picture and the reference picture slots.
     * @return The session, ErrorCode::InvalidArgument when the device was not created to decode the profile's codec or
     *         the desc asks for more than VideoCapabilities allows - the log names which - ErrorCode::FeatureNotSupported
     *         when the device does not decode the profile, or whatever the failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<VideoSession, ErrorCode> Create(const Device& device, const VideoSessionDesc& desc = {});

    VideoSession(const VideoSession&) = delete;
    VideoSession& operator=(const VideoSession&) = delete;
    VideoSession(VideoSession&& other) noexcept;
    VideoSession& operator=(VideoSession&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_session != VK_NULL_HANDLE; }
    [[nodiscard]] VkVideoSessionKHR GetNativeSession() const { return m_session; }
    [[nodiscard]] const VideoSessionDesc& GetDesc() const { return m_desc; }
    /** How much memory the device asked for and the session holds, in bytes. */
    [[nodiscard]] u64 GetMemorySize() const { return m_memory_size; }

private:
    Opal::Ref<const Device> m_device;
    VkVideoSessionKHR m_session = VK_NULL_HANDLE;
    Opal::DynamicArray<VmaAllocation> m_allocations;
    u64 m_memory_size = 0;
    VideoSessionDesc m_desc;
};

/**
 * H.264 parameter sets as the stream carries them, already parsed: what VideoSessionParameters holds. Vulkan copies them
 * during the call, so what they point at need only last until it returns.
 */
struct H264ParameterSets
{
    Opal::ArrayView<const StdVideoH264SequenceParameterSet> sequence_parameter_sets;
    Opal::ArrayView<const StdVideoH264PictureParameterSet> picture_parameter_sets;
};

/**
 * The parameter sets a stream carries, kept by the device for one session so that a decode names them by id. A stream
 * may bring new ones as it goes, which Add hands over; one id is given once. H.264 only, so far.
 */
class VideoSessionParameters
{
public:
    VideoSessionParameters() = default;
    ~VideoSessionParameters();

    /** The most sequence and picture parameter sets an H.264 stream has, which is what the parameters make room for. */
    static constexpr u32 k_max_h264_sequence_parameter_sets = 32;
    static constexpr u32 k_max_h264_picture_parameter_sets = 256;

    /**
     * @param session The session the parameters are for, an H.264 one. It has to outlive them.
     * @param sets The parameter sets the stream starts with; may be empty.
     * @return The parameters, ErrorCode::InvalidArgument for a session that is empty or not H.264, an id given twice or
     *         one past what H.264 allows, or whatever the failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<VideoSessionParameters, ErrorCode> CreateH264(const Device& device, const VideoSession& session,
                                                                                      const H264ParameterSets& sets = {});

    VideoSessionParameters(const VideoSessionParameters&) = delete;
    VideoSessionParameters& operator=(const VideoSessionParameters&) = delete;
    VideoSessionParameters(VideoSessionParameters&& other) noexcept;
    VideoSessionParameters& operator=(VideoSessionParameters&& other) noexcept;

    /**
     * Hand over parameter sets the stream brought after the start.
     * @return ErrorCode::InvalidArgument for an id these parameters already hold or one past what H.264 allows - a
     *         changed set needs parameters of its own - or whatever the failing update maps to.
     */
    ErrorCode AddH264(const H264ParameterSets& sets);

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_parameters != VK_NULL_HANDLE; }
    [[nodiscard]] VkVideoSessionParametersKHR GetNativeParameters() const { return m_parameters; }

private:
    /** Check the ids of `sets` against those held and each other, and note them as held. */
    ErrorCode TakeIds(const H264ParameterSets& sets);

    Opal::Ref<const Device> m_device;
    VkVideoSessionParametersKHR m_parameters = VK_NULL_HANDLE;
    /** How many updates have been made, which each update has to count on from. */
    u32 m_update_count = 0;
    /** Which sequence and picture parameter set ids are held, one bit each. */
    u32 m_sps_ids = 0;
    u32 m_pps_ids[k_max_h264_picture_parameter_sets / 32] = {};
};

}  // namespace Rndr::Forge
