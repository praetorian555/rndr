#pragma once

#include "volk.h"

#include "rndr/error-codes.hpp"
#include "rndr/forge/video.hpp"

namespace Rndr::Forge
{

/**
 * The Vulkan structures a VideoProfile becomes: the profile, the codec's own profile chained to it, and a list of the one
 * profile for the images and buffers created for it. Points at its own members, so it is built in place and never
 * copied or moved.
 */
struct VideoProfileChain
{
    VkVideoDecodeH264ProfileInfoKHR h264{.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR};
    VkVideoDecodeH265ProfileInfoKHR h265{.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR};
    VkVideoDecodeAV1ProfileInfoKHR av1{.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR};
    VkVideoProfileInfoKHR profile{.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR};
    VkVideoProfileListInfoKHR list{.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR};

    explicit VideoProfileChain(const VideoProfile& video_profile);
    VideoProfileChain(const VideoProfileChain&) = delete;
    VideoProfileChain& operator=(const VideoProfileChain&) = delete;
};

/** The codec's decode operation, as VkVideoCodecOperationFlagBitsKHR. */
[[nodiscard]] VkVideoCodecOperationFlagBitsKHR ToVkCodecOperation(VideoCodec codec);

/** The device extension that decodes the codec. */
[[nodiscard]] const char* GetDecodeExtensionName(VideoCodec codec);

/**
 * Check that Forge can describe the profile to Vulkan: a known codec and a bit depth of 8 or 10.
 * @return ErrorCode::InvalidArgument, with the reason logged, for one it cannot.
 */
[[nodiscard]] ErrorCode CheckVideoProfile(const VideoProfile& profile);

/**
 * Check what a buffer or texture created for video decode needs beyond its profile: a device created with a decode queue,
 * which is what enables the video extensions, and the codec's extension among them.
 * @param what What is being created, for the log: "a VideoDecodeSource buffer".
 * @return ErrorCode::InvalidArgument, with the reason logged, when either is missing.
 */
[[nodiscard]] ErrorCode CheckVideoUsage(const Device& device, const VideoProfile& profile, const char* what);

}  // namespace Rndr::Forge
