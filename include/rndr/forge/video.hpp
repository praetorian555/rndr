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

    bool operator==(const VideoProfile& other) const = default;
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

/** What the device made of one decode, as a status query reports it. */
enum class VideoDecodeStatus : u8
{
    /** The device has not finished the decode yet. */
    NotReady,
    /** The picture was decoded. */
    Complete,
    /** The decode failed, and what it wrote is not a picture of the stream - a bitstream the device could not decode. */
    Error,
};

/** A pool of status queries for one profile. */
struct VideoStatusQueryPoolDesc
{
    /** The profile of the session whose decodes the queries report on, which has to be the same. */
    VideoProfile profile;
    /** How many queries fit: one per decode whose status is wanted before the pool is read and reset. */
    u32 query_count = 1;
};

/**
 * Status queries of video decodes: VideoDecodeDesc::status_query names one, the decode writes into it whether it
 * succeeded, and GetStatus reads that back. A device decodes whatever bitstream it is given without a Vulkan error, so
 * this is the one place a picture the device could not make sense of shows up as such.
 *
 * Like any query, one holds an undefined value until CommandBuffer::CmdResetQueryPool resets it, which happens outside a
 * video coding scope - in the same command buffer before CmdBeginVideoCoding, or an earlier one. Only a decode queue
 * whose family reports result status support (PhysicalDevice::SupportsQueryResultStatus) writes them.
 */
class VideoStatusQueryPool
{
public:
    VideoStatusQueryPool() = default;
    ~VideoStatusQueryPool();

    /**
     * @param desc The profile and how many queries.
     * @return The pool, ErrorCode::InvalidArgument for no queries or a profile the device was not created to decode,
     *         ErrorCode::FeatureNotSupported when the decode queue's family writes no result status, or whatever the
     *         failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<VideoStatusQueryPool, ErrorCode> Create(const Device& device,
                                                                                const VideoStatusQueryPoolDesc& desc = {});

    VideoStatusQueryPool(const VideoStatusQueryPool&) = delete;
    VideoStatusQueryPool& operator=(const VideoStatusQueryPool&) = delete;
    VideoStatusQueryPool(VideoStatusQueryPool&& other) noexcept;
    VideoStatusQueryPool& operator=(VideoStatusQueryPool&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_query_pool != VK_NULL_HANDLE; }
    [[nodiscard]] VkQueryPool GetNativeQueryPool() const { return m_query_pool; }
    [[nodiscard]] const VideoStatusQueryPoolDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] u32 GetQueryCount() const { return m_desc.query_count; }

    /**
     * Read one query's status, waiting until the device has written it. A query no decode wrote waits forever, so this
     * is for one a submitted decode named.
     * @param query Which query of the pool.
     * @return Complete or Error, ErrorCode::OutOfBounds for a query past the pool, or whatever the failing read maps to.
     */
    [[nodiscard]] Opal::Expected<VideoDecodeStatus, ErrorCode> GetStatus(u32 query) const;

    /**
     * Read one query's status without waiting.
     * @param query Which query of the pool.
     * @return NotReady while the device has not written it, Complete or Error once it has, ErrorCode::OutOfBounds for a
     *         query past the pool, or whatever the failing read maps to.
     */
    [[nodiscard]] Opal::Expected<VideoDecodeStatus, ErrorCode> TryGetStatus(u32 query) const;

private:
    [[nodiscard]] Opal::Expected<VideoDecodeStatus, ErrorCode> ReadStatus(u32 query, bool wait) const;

    Opal::Ref<const Device> m_device;
    VkQueryPool m_query_pool = VK_NULL_HANDLE;
    VideoStatusQueryPoolDesc m_desc;
};

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
    /** What the offset and the size of the bitstream of one decode have to be multiples of. */
    [[nodiscard]] u64 GetBitstreamOffsetAlignment() const { return m_bitstream_offset_alignment; }
    [[nodiscard]] u64 GetBitstreamSizeAlignment() const { return m_bitstream_size_alignment; }

private:
    Opal::Ref<const Device> m_device;
    VkVideoSessionKHR m_session = VK_NULL_HANDLE;
    Opal::DynamicArray<VmaAllocation> m_allocations;
    u64 m_memory_size = 0;
    u64 m_bitstream_offset_alignment = 1;
    u64 m_bitstream_size_alignment = 1;
    VideoSessionDesc m_desc;
};

/** A picture a video decode reads or writes: the texture holding it, seen through a view with a decode usage. */
struct VideoPicture
{
    /**
     * A view of the texture, made with TextureUsageBits::VideoDecodeDestination or VideoDecodeDpb in TextureViewDesc::usage,
     * which is what a YCbCr texture can be seen as without a conversion.
     */
    Opal::Ref<const TextureView> view;
    /** The size of the picture in the texture: the stream's coded size, which may be smaller than the texture. */
    Vector2i coded_extent;
};

/** A reference picture slot of a session, with the picture it holds. */
struct VideoReferenceSlot
{
    /**
     * The slot's index in the session, below VideoSessionDesc::max_dpb_slots. In CmdBeginVideoCoding, -1 for the slot a decode
     * in the scope is about to fill for the first time.
     */
    i32 slot_index = -1;
    VideoPicture picture;
    /** H.264: what the stream says of the picture as a reference - its frame number, its picture order counts. */
    StdVideoDecodeH264ReferenceInfo h264_reference{};
    /** H.265: the same - its picture order count, and whether it is a long-term reference. */
    StdVideoDecodeH265ReferenceInfo h265_reference{};
};

/** What a video coding scope is over, from CmdBeginVideoCoding to CmdEndVideoCoding. */
struct VideoCodingDesc
{
    Opal::Ref<const VideoSession> session;
    /** The stream's parameter sets. May be empty for a scope that only resets the session. */
    Opal::Ref<const VideoSessionParameters> parameters;
    /** Every slot the decodes in the scope read or fill, with the picture each holds or is about to. */
    Opal::ArrayView<const VideoReferenceSlot> reference_slots;
};

/** One picture to decode, inside a video coding scope. */
struct VideoDecodeDesc
{
    /** The buffer holding the picture's bitstream, a BufferUsageBits::VideoDecodeSource one created for the session's profile. */
    Opal::Ref<const Buffer> bitstream;
    /** Where the picture's bitstream starts and how long it is, multiples of the session's bitstream alignments. */
    u64 bitstream_offset = 0;
    u64 bitstream_size = 0;
    /** Where the decoded picture is written. */
    VideoPicture output;
    /**
     * The slot the picture is kept in as a reference for later ones, with the picture as it will be referred to; a
     * slot_index of -1 for a picture nothing refers to. Where the device's output and reference pictures coincide, its
     * picture is the output's.
     */
    VideoReferenceSlot setup;
    /** The reference pictures this one is predicted from, each in the slot it was kept in. */
    Opal::ArrayView<const VideoReferenceSlot> references;
    /** Where each slice (H.264) or slice segment (H.265) of the picture starts in the bitstream range. */
    Opal::ArrayView<const u32> slice_offsets;
    /** H.264: the picture's parameters from its slice header. */
    StdVideoDecodeH264PictureInfo h264_picture{};
    /**
     * H.265: the picture's parameters from its slice segment header, its reference picture set as the slot indices of
     * pictures among `references` - STD_VIDEO_H265_NO_REFERENCE_PICTURE past the end of each list.
     */
    StdVideoDecodeH265PictureInfo h265_picture{};
    /**
     * Where the decode writes whether it succeeded, or empty for nowhere: a pool for the session's profile, and a query
     * of it that has been reset since it was last written.
     */
    Opal::Ref<const VideoStatusQueryPool> status_query_pool;
    u32 status_query = 0;
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

/** H.265 parameter sets, the same way: each with what its pointers point at, which need only last until the call returns. */
struct H265ParameterSets
{
    Opal::ArrayView<const StdVideoH265VideoParameterSet> video_parameter_sets;
    Opal::ArrayView<const StdVideoH265SequenceParameterSet> sequence_parameter_sets;
    Opal::ArrayView<const StdVideoH265PictureParameterSet> picture_parameter_sets;
};

/**
 * The parameter sets a stream carries, kept by the device for one session so that a decode names them by id. A stream
 * may bring new ones as it goes, which Add hands over; one id is given once - for H.265 an SPS is named by its VPS id and
 * its own, and a PPS by those and its own.
 */
class VideoSessionParameters
{
public:
    VideoSessionParameters() = default;
    ~VideoSessionParameters();

    /** The most sequence and picture parameter sets an H.264 stream has, which is what the parameters make room for. */
    static constexpr u32 k_max_h264_sequence_parameter_sets = 32;
    static constexpr u32 k_max_h264_picture_parameter_sets = 256;
    /** The ids H.265 allows, and how many sets of each kind H.265 parameters make room for. */
    static constexpr u32 k_max_h265_video_parameter_sets = 16;
    static constexpr u32 k_max_h265_sequence_parameter_sets = 16;
    static constexpr u32 k_max_h265_picture_parameter_sets = 64;
    static constexpr u32 k_h265_sequence_parameter_set_room = 32;
    static constexpr u32 k_h265_picture_parameter_set_room = 256;

    /**
     * @param session The session the parameters are for, an H.264 one. It has to outlive them.
     * @param sets The parameter sets the stream starts with; may be empty.
     * @return The parameters, ErrorCode::InvalidArgument for a session that is empty or not H.264, an id given twice or
     *         one past what H.264 allows, or whatever the failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<VideoSessionParameters, ErrorCode> CreateH264(const Device& device, const VideoSession& session,
                                                                                      const H264ParameterSets& sets = {});

    /**
     * @param session The session the parameters are for, an H.265 one. It has to outlive them.
     * @param sets The parameter sets the stream starts with; may be empty.
     * @return The parameters, ErrorCode::InvalidArgument for a session that is empty or not H.265, a set given twice, an id
     *         past what H.265 allows or more sets than the parameters make room for, or whatever the failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<VideoSessionParameters, ErrorCode> CreateH265(const Device& device, const VideoSession& session,
                                                                                      const H265ParameterSets& sets = {});

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

    /** AddH264 for H.265 parameters, refusing what CreateH265 does. */
    ErrorCode AddH265(const H265ParameterSets& sets);

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_parameters != VK_NULL_HANDLE; }
    [[nodiscard]] VkVideoSessionParametersKHR GetNativeParameters() const { return m_parameters; }

private:
    /** Check the ids of `sets` against those held and each other, and note them as held. */
    ErrorCode TakeIds(const H264ParameterSets& sets);
    ErrorCode TakeIds(const H265ParameterSets& sets);
    /** Send an update, counting on from the last one. */
    ErrorCode Update(const void* add_info);

    Opal::Ref<const Device> m_device;
    VkVideoSessionParametersKHR m_parameters = VK_NULL_HANDLE;
    /** How many updates have been made, which each update has to count on from. */
    u32 m_update_count = 0;
    /** The sets held, each as a key of its kind and ids, so that none is given twice. */
    Opal::DynamicArray<u32> m_held;
};

}  // namespace Rndr::Forge
