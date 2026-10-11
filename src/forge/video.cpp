#include "rndr/forge/video.hpp"

#include "rndr/forge/device.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/log.hpp"

#include "video-profile.hpp"

Rndr::Forge::VideoProfileChain::VideoProfileChain(const VideoProfile& video_profile)
{
    const VkVideoComponentBitDepthFlagsKHR depth =
        video_profile.bit_depth == 10 ? VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR : VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    profile.videoCodecOperation = ToVkCodecOperation(video_profile.codec);
    profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
    profile.lumaBitDepth = depth;
    profile.chromaBitDepth = depth;
    switch (video_profile.codec)
    {
        case VideoCodec::H264:
            h264.stdProfileIdc = static_cast<StdVideoH264ProfileIdc>(video_profile.codec_profile);
            h264.pictureLayout = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR;
            profile.pNext = &h264;
            break;
        case VideoCodec::H265:
            h265.stdProfileIdc = static_cast<StdVideoH265ProfileIdc>(video_profile.codec_profile);
            profile.pNext = &h265;
            break;
        case VideoCodec::Av1:
            av1.stdProfile = static_cast<StdVideoAV1Profile>(video_profile.codec_profile);
            av1.filmGrainSupport = VK_FALSE;
            profile.pNext = &av1;
            break;
    }
    list.profileCount = 1;
    list.pProfiles = &profile;
}

VkVideoCodecOperationFlagBitsKHR Rndr::Forge::ToVkCodecOperation(VideoCodec codec)
{
    switch (codec)
    {
        case VideoCodec::H264:
            return VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
        case VideoCodec::H265:
            return VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
        case VideoCodec::Av1:
            return VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
    }
    return VK_VIDEO_CODEC_OPERATION_NONE_KHR;
}

const char* Rndr::Forge::GetDecodeExtensionName(VideoCodec codec)
{
    switch (codec)
    {
        case VideoCodec::H264:
            return VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME;
        case VideoCodec::H265:
            return VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME;
        case VideoCodec::Av1:
            return VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME;
    }
    return "";
}

Rndr::ErrorCode Rndr::Forge::CheckVideoProfile(const VideoProfile& profile)
{
    if (ToVkCodecOperation(profile.codec) == VK_VIDEO_CODEC_OPERATION_NONE_KHR)
    {
        RNDR_LOG_ERROR("Forge: VideoProfile::codec names no codec Forge decodes");
        return ErrorCode::InvalidArgument;
    }
    if (profile.bit_depth != 8 && profile.bit_depth != 10)
    {
        RNDR_LOG_ERROR("Forge: a video profile has 8 or 10 bits per sample, and this one {}", profile.bit_depth);
        return ErrorCode::InvalidArgument;
    }
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::CheckVideoUsage(const Device& device, const VideoProfile& profile, const char* what)
{
    RNDR_FORGE_CHECK(CheckVideoProfile(profile));
    if (!device.IsExtensionEnabled(VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME))
    {
        RNDR_LOG_ERROR("Forge: {} needs the device created with DeviceDesc::use_decode_queue", what);
        return ErrorCode::InvalidArgument;
    }
    if (!device.IsExtensionEnabled(GetDecodeExtensionName(profile.codec)))
    {
        RNDR_LOG_ERROR("Forge: {} is for a codec the device was not created to decode: DeviceDesc::decode_codecs leaves out {}", what,
                       GetDecodeExtensionName(profile.codec));
        return ErrorCode::InvalidArgument;
    }
    return ErrorCode::Success;
}

namespace
{

/**
 * The formats an image used as `usage` can have under the profile. Empty when the device has none, which the query
 * reports as an error rather than an empty list for some usages.
 */
Opal::DynamicArray<Rndr::PixelFormat> QueryVideoFormats(VkPhysicalDevice physical_device, const Rndr::Forge::VideoProfileChain& chain,
                                                        VkImageUsageFlags usage)
{
    Opal::DynamicArray<Rndr::PixelFormat> formats;
    const VkPhysicalDeviceVideoFormatInfoKHR format_info{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR, .pNext = &chain.list, .imageUsage = usage};
    Rndr::u32 count = 0;
    if (vkGetPhysicalDeviceVideoFormatPropertiesKHR(physical_device, &format_info, &count, nullptr) != VK_SUCCESS || count == 0)
    {
        return formats;
    }
    Opal::DynamicArray<VkVideoFormatPropertiesKHR> properties(count);
    for (VkVideoFormatPropertiesKHR& entry : properties)
    {
        entry = {.sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR};
    }
    if (vkGetPhysicalDeviceVideoFormatPropertiesKHR(physical_device, &format_info, &count, properties.GetData()) != VK_SUCCESS)
    {
        return formats;
    }
    for (Rndr::u32 i = 0; i < count; ++i)
    {
        // A device may list a format once per tiling or flag combination; one entry each is all a caller needs.
        const Rndr::PixelFormat format = Rndr::FromVkFormat(properties[i].format);
        bool listed = false;
        for (const Rndr::PixelFormat existing : formats)
        {
            listed = listed || existing == format;
        }
        if (!listed)
        {
            formats.PushBack(format);
        }
    }
    return formats;
}

}  // namespace

Opal::Expected<Rndr::Forge::VideoCapabilities, Rndr::ErrorCode> Rndr::Forge::GetVideoDecodeCapabilities(const PhysicalDevice& physical_device,
                                                                                                         const VideoProfile& profile)
{
    using Result = Opal::Expected<VideoCapabilities, ErrorCode>;

    RNDR_FORGE_CHECK_EXPECTED(CheckVideoProfile(profile), Result);
    for (const char* extension : {VK_KHR_VIDEO_QUEUE_EXTENSION_NAME, VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME, GetDecodeExtensionName(profile.codec)})
    {
        if (!physical_device.IsExtensionSupported(extension))
        {
            RNDR_LOG_ERROR("Forge: this device does not decode the profile's codec: it has no {}", extension);
            return Result(ErrorCode::FeatureNotSupported);
        }
    }

    const VideoProfileChain chain(profile);
    VkVideoDecodeH264CapabilitiesKHR h264{.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR};
    VkVideoDecodeH265CapabilitiesKHR h265{.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR};
    VkVideoDecodeAV1CapabilitiesKHR av1{.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR};
    VkVideoDecodeCapabilitiesKHR decode{.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR};
    switch (profile.codec)
    {
        case VideoCodec::H264:
            decode.pNext = &h264;
            break;
        case VideoCodec::H265:
            decode.pNext = &h265;
            break;
        case VideoCodec::Av1:
            decode.pNext = &av1;
            break;
    }
    VkVideoCapabilitiesKHR capabilities{.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR, .pNext = &decode};
    const VkResult result = vkGetPhysicalDeviceVideoCapabilitiesKHR(physical_device.GetNativePhysicalDevice(), &chain.profile, &capabilities);
    switch (result)
    {
        case VK_SUCCESS:
            break;
        case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
        case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
        case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
        case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
            RNDR_LOG_ERROR("Forge: this device does not decode the profile (codec profile {}, {} bits): {}", profile.codec_profile,
                           profile.bit_depth, static_cast<i32>(result));
            return Result(ErrorCode::FeatureNotSupported);
        default:
            RNDR_FORGE_VK_CHECK_EXPECTED(result, "vkGetPhysicalDeviceVideoCapabilitiesKHR", Result);
            break;
    }

    VideoCapabilities out;
    out.min_coded_extent = {static_cast<i32>(capabilities.minCodedExtent.width), static_cast<i32>(capabilities.minCodedExtent.height)};
    out.max_coded_extent = {static_cast<i32>(capabilities.maxCodedExtent.width), static_cast<i32>(capabilities.maxCodedExtent.height)};
    out.picture_access_granularity = {static_cast<i32>(capabilities.pictureAccessGranularity.width),
                                      static_cast<i32>(capabilities.pictureAccessGranularity.height)};
    out.max_dpb_slots = capabilities.maxDpbSlots;
    out.max_active_reference_pictures = capabilities.maxActiveReferencePictures;
    out.bitstream_offset_alignment = capabilities.minBitstreamBufferOffsetAlignment;
    out.bitstream_size_alignment = capabilities.minBitstreamBufferSizeAlignment;
    out.separate_reference_images = (capabilities.flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR) != 0;
    out.dpb_and_output_coincide = (decode.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR) != 0;
    out.dpb_and_output_distinct = (decode.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR) != 0;
    switch (profile.codec)
    {
        case VideoCodec::H264:
            out.max_level = static_cast<u32>(h264.maxLevelIdc);
            break;
        case VideoCodec::H265:
            out.max_level = static_cast<u32>(h265.maxLevelIdc);
            break;
        case VideoCodec::Av1:
            out.max_level = static_cast<u32>(av1.maxLevel);
            break;
    }

    const VkPhysicalDevice native = physical_device.GetNativePhysicalDevice();
    out.dpb_formats = QueryVideoFormats(native, chain, VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR);
    // The output is sampled once it is decoded, wherever it is written.
    const VkImageUsageFlags output_usage = out.dpb_and_output_distinct
                                               ? VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_SAMPLED_BIT
                                               : VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR |
                                                     VK_IMAGE_USAGE_SAMPLED_BIT;
    out.output_formats = QueryVideoFormats(native, chain, output_usage);
    return Result(std::move(out));
}
