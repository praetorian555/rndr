#pragma once

#include "volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"

#include "rndr/error-codes.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/math.hpp"
#include "rndr/pixel-format.hpp"
#include "rndr/types.hpp"

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

}  // namespace Rndr::Forge
