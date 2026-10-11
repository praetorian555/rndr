#include "rndr/forge/video.hpp"

#include "vk_mem_alloc.h"

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
    out.std_header_version = capabilities.stdHeaderVersion;
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


// VideoSession

namespace
{

bool ContainsFormat(const Opal::DynamicArray<Rndr::PixelFormat>& formats, Rndr::PixelFormat format)
{
    for (const Rndr::PixelFormat candidate : formats)
    {
        if (candidate == format)
        {
            return true;
        }
    }
    return false;
}

}  // namespace

Opal::Expected<Rndr::Forge::VideoSession, Rndr::ErrorCode> Rndr::Forge::VideoSession::Create(const Device& device, const VideoSessionDesc& desc)
{
    using Result = Opal::Expected<VideoSession, ErrorCode>;

    RNDR_FORGE_CHECK_EXPECTED(CheckVideoUsage(device, desc.profile, "a VideoSession"), Result);
    const Opal::Expected<VideoCapabilities, ErrorCode> capabilities_result = GetVideoDecodeCapabilities(device.GetPhysicalDevice(), desc.profile);
    if (!capabilities_result.HasValue())
    {
        return Result(capabilities_result.GetError());
    }
    const VideoCapabilities& capabilities = capabilities_result.GetValue();

    // What the device allows, checked here so that a desc it cannot take is a line saying which part, rather than a
    // validation error naming a field of a structure the caller never wrote.
    const char* broken = nullptr;
    if (desc.max_coded_extent.x < capabilities.min_coded_extent.x || desc.max_coded_extent.y < capabilities.min_coded_extent.y ||
        desc.max_coded_extent.x > capabilities.max_coded_extent.x || desc.max_coded_extent.y > capabilities.max_coded_extent.y)
    {
        broken = "max_coded_extent is outside what the device decodes (VideoCapabilities::min_coded_extent to max_coded_extent)";
    }
    else if (desc.max_dpb_slots > capabilities.max_dpb_slots)
    {
        broken = "max_dpb_slots is more than VideoCapabilities::max_dpb_slots";
    }
    else if (desc.max_active_reference_pictures > capabilities.max_active_reference_pictures ||
             desc.max_active_reference_pictures > desc.max_dpb_slots)
    {
        broken = "max_active_reference_pictures is more than VideoCapabilities::max_active_reference_pictures or than max_dpb_slots";
    }
    else if (!ContainsFormat(capabilities.output_formats, desc.picture_format))
    {
        broken = "picture_format is not one of VideoCapabilities::output_formats";
    }
    else if (!ContainsFormat(capabilities.dpb_formats, desc.reference_picture_format))
    {
        broken = "reference_picture_format is not one of VideoCapabilities::dpb_formats";
    }
    if (broken != nullptr)
    {
        RNDR_LOG_ERROR("Forge: a video session the device cannot hold: {}", broken);
        return Result(ErrorCode::InvalidArgument);
    }

    const Opal::Expected<const DeviceQueue&, ErrorCode> decode_queue = device.GetQueue(QueueFamily::Decode);
    if (!decode_queue.HasValue())
    {
        return Result(decode_queue.GetError());
    }

    VideoSession session;
    session.m_device = device;
    session.m_desc = desc;
    session.m_bitstream_offset_alignment = capabilities.bitstream_offset_alignment;
    session.m_bitstream_size_alignment = capabilities.bitstream_size_alignment;
    const VideoProfileChain chain(desc.profile);
    const VkVideoSessionCreateInfoKHR create_info{
        .sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR,
        .queueFamilyIndex = decode_queue.GetValue().GetQueueFamilyIndex(),
        .pVideoProfile = &chain.profile,
        .pictureFormat = ToVkFormat(desc.picture_format),
        .maxCodedExtent = {.width = static_cast<u32>(desc.max_coded_extent.x), .height = static_cast<u32>(desc.max_coded_extent.y)},
        .referencePictureFormat = ToVkFormat(desc.reference_picture_format),
        .maxDpbSlots = desc.max_dpb_slots,
        .maxActiveReferencePictures = desc.max_active_reference_pictures,
        .pStdHeaderVersion = &capabilities.std_header_version,
    };
    const VkDevice native_device = device.GetNativeDevice();
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateVideoSessionKHR(native_device, &create_info, nullptr, &session.m_session), "vkCreateVideoSessionKHR",
                                 Result);

    // The device says how much memory the session needs and in how many pieces; each is allocated on its own and the
    // lot bound in one call. Anything allocated so far is freed by the destructor if a later step fails.
    u32 requirement_count = 0;
    RNDR_FORGE_VK_CHECK_EXPECTED(vkGetVideoSessionMemoryRequirementsKHR(native_device, session.m_session, &requirement_count, nullptr),
                                 "vkGetVideoSessionMemoryRequirementsKHR", Result);
    Opal::DynamicArray<VkVideoSessionMemoryRequirementsKHR> requirements(requirement_count);
    for (VkVideoSessionMemoryRequirementsKHR& requirement : requirements)
    {
        requirement = {.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR};
    }
    RNDR_FORGE_VK_CHECK_EXPECTED(
        vkGetVideoSessionMemoryRequirementsKHR(native_device, session.m_session, &requirement_count, requirements.GetData()),
        "vkGetVideoSessionMemoryRequirementsKHR", Result);
    Opal::DynamicArray<VkBindVideoSessionMemoryInfoKHR> binds(requirement_count);
    for (u32 i = 0; i < requirement_count; ++i)
    {
        const VmaAllocationCreateInfo allocation_create_info{.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                             .memoryTypeBits = requirements[i].memoryRequirements.memoryTypeBits};
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocation_info{};
        RNDR_FORGE_VK_CHECK_EXPECTED(vmaAllocateMemory(device.GetGPUAllocator(), &requirements[i].memoryRequirements,
                                                       &allocation_create_info, &allocation, &allocation_info),
                                     "vmaAllocateMemory", Result);
        session.m_allocations.PushBack(allocation);
        session.m_memory_size += requirements[i].memoryRequirements.size;
        binds[i] = {.sType = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR,
                    .memoryBindIndex = requirements[i].memoryBindIndex,
                    .memory = allocation_info.deviceMemory,
                    .memoryOffset = allocation_info.offset,
                    .memorySize = requirements[i].memoryRequirements.size};
    }
    if (requirement_count > 0)
    {
        RNDR_FORGE_VK_CHECK_EXPECTED(vkBindVideoSessionMemoryKHR(native_device, session.m_session, requirement_count, binds.GetData()),
                                     "vkBindVideoSessionMemoryKHR", Result);
    }
    return Result(std::move(session));
}

Rndr::Forge::VideoSession::~VideoSession()
{
    Destroy();
}

Rndr::Forge::VideoSession::VideoSession(VideoSession&& other) noexcept
    : m_device(std::move(other.m_device)),
      m_session(other.m_session),
      m_allocations(std::move(other.m_allocations)),
      m_memory_size(other.m_memory_size),
      m_bitstream_offset_alignment(other.m_bitstream_offset_alignment),
      m_bitstream_size_alignment(other.m_bitstream_size_alignment),
      m_desc(other.m_desc)
{
    other.m_session = VK_NULL_HANDLE;
    other.m_allocations.Clear();
    other.m_memory_size = 0;
    other.m_device = nullptr;
}

Rndr::Forge::VideoSession& Rndr::Forge::VideoSession::operator=(VideoSession&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_session = other.m_session;
        m_allocations = std::move(other.m_allocations);
        m_memory_size = other.m_memory_size;
        m_bitstream_offset_alignment = other.m_bitstream_offset_alignment;
        m_bitstream_size_alignment = other.m_bitstream_size_alignment;
        m_desc = other.m_desc;
        other.m_session = VK_NULL_HANDLE;
        other.m_allocations.Clear();
        other.m_memory_size = 0;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::Forge::VideoSession::Destroy()
{
    if (m_session != VK_NULL_HANDLE)
    {
        vkDestroyVideoSessionKHR(m_device->GetNativeDevice(), m_session, nullptr);
        m_session = VK_NULL_HANDLE;
    }
    for (VmaAllocation allocation : m_allocations)
    {
        vmaFreeMemory(m_device->GetGPUAllocator(), allocation);
    }
    m_allocations.Clear();
    m_memory_size = 0;
}

// VideoSessionParameters

Rndr::ErrorCode Rndr::Forge::VideoSessionParameters::TakeIds(const H264ParameterSets& sets)
{
    u32 sps_ids = m_sps_ids;
    u32 pps_ids[k_max_h264_picture_parameter_sets / 32] = {};
    for (u32 i = 0; i < k_max_h264_picture_parameter_sets / 32; ++i)
    {
        pps_ids[i] = m_pps_ids[i];
    }
    for (const StdVideoH264SequenceParameterSet& sps : sets.sequence_parameter_sets)
    {
        const u32 id = sps.seq_parameter_set_id;
        if (id >= k_max_h264_sequence_parameter_sets || (sps_ids & (1u << id)) != 0)
        {
            RNDR_LOG_ERROR("Forge: sequence parameter set {} is past what H.264 allows or already held; a changed set needs parameters of its own",
                           id);
            return ErrorCode::InvalidArgument;
        }
        sps_ids |= 1u << id;
    }
    for (const StdVideoH264PictureParameterSet& pps : sets.picture_parameter_sets)
    {
        const u32 id = pps.pic_parameter_set_id;
        if (id >= k_max_h264_picture_parameter_sets || (pps_ids[id / 32] & (1u << (id % 32))) != 0)
        {
            RNDR_LOG_ERROR("Forge: picture parameter set {} is past what H.264 allows or already held; a changed set needs parameters of its own",
                           id);
            return ErrorCode::InvalidArgument;
        }
        pps_ids[id / 32] |= 1u << (id % 32);
    }
    m_sps_ids = sps_ids;
    for (u32 i = 0; i < k_max_h264_picture_parameter_sets / 32; ++i)
    {
        m_pps_ids[i] = pps_ids[i];
    }
    return ErrorCode::Success;
}

Opal::Expected<Rndr::Forge::VideoSessionParameters, Rndr::ErrorCode> Rndr::Forge::VideoSessionParameters::CreateH264(
    const Device& device, const VideoSession& session, const H264ParameterSets& sets)
{
    using Result = Opal::Expected<VideoSessionParameters, ErrorCode>;

    if (!session.IsValid() || session.GetDesc().profile.codec != VideoCodec::H264)
    {
        RNDR_LOG_ERROR("Forge: H.264 session parameters need an H.264 video session");
        return Result(ErrorCode::InvalidArgument);
    }
    VideoSessionParameters parameters;
    parameters.m_device = device;
    RNDR_FORGE_CHECK_EXPECTED(parameters.TakeIds(sets), Result);

    const VkVideoDecodeH264SessionParametersAddInfoKHR add_info{
        .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR,
        .stdSPSCount = static_cast<u32>(sets.sequence_parameter_sets.GetSize()),
        .pStdSPSs = sets.sequence_parameter_sets.GetData(),
        .stdPPSCount = static_cast<u32>(sets.picture_parameter_sets.GetSize()),
        .pStdPPSs = sets.picture_parameter_sets.GetData(),
    };
    const VkVideoDecodeH264SessionParametersCreateInfoKHR h264_info{
        .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR,
        .maxStdSPSCount = k_max_h264_sequence_parameter_sets,
        .maxStdPPSCount = k_max_h264_picture_parameter_sets,
        .pParametersAddInfo = &add_info,
    };
    const VkVideoSessionParametersCreateInfoKHR create_info{.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR,
                                                            .pNext = &h264_info,
                                                            .videoSession = session.GetNativeSession()};
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateVideoSessionParametersKHR(device.GetNativeDevice(), &create_info, nullptr, &parameters.m_parameters),
                                 "vkCreateVideoSessionParametersKHR", Result);
    return Result(std::move(parameters));
}

Rndr::ErrorCode Rndr::Forge::VideoSessionParameters::AddH264(const H264ParameterSets& sets)
{
    if (!IsValid())
    {
        RNDR_LOG_ERROR("Forge: AddH264 on empty session parameters");
        return ErrorCode::InvalidArgument;
    }
    RNDR_FORGE_CHECK(TakeIds(sets));
    const VkVideoDecodeH264SessionParametersAddInfoKHR add_info{
        .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR,
        .stdSPSCount = static_cast<u32>(sets.sequence_parameter_sets.GetSize()),
        .pStdSPSs = sets.sequence_parameter_sets.GetData(),
        .stdPPSCount = static_cast<u32>(sets.picture_parameter_sets.GetSize()),
        .pStdPPSs = sets.picture_parameter_sets.GetData(),
    };
    // Each update counts on by one from the last, starting at one.
    const VkVideoSessionParametersUpdateInfoKHR update_info{
        .sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_UPDATE_INFO_KHR, .pNext = &add_info, .updateSequenceCount = m_update_count + 1};
    RNDR_FORGE_VK_CHECK(vkUpdateVideoSessionParametersKHR(m_device->GetNativeDevice(), m_parameters, &update_info),
                        "vkUpdateVideoSessionParametersKHR");
    ++m_update_count;
    return ErrorCode::Success;
}

Rndr::Forge::VideoSessionParameters::~VideoSessionParameters()
{
    Destroy();
}

Rndr::Forge::VideoSessionParameters::VideoSessionParameters(VideoSessionParameters&& other) noexcept
    : m_device(std::move(other.m_device)), m_parameters(other.m_parameters), m_update_count(other.m_update_count), m_sps_ids(other.m_sps_ids)
{
    for (u32 i = 0; i < k_max_h264_picture_parameter_sets / 32; ++i)
    {
        m_pps_ids[i] = other.m_pps_ids[i];
    }
    other.m_parameters = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::Forge::VideoSessionParameters& Rndr::Forge::VideoSessionParameters::operator=(VideoSessionParameters&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_parameters = other.m_parameters;
        m_update_count = other.m_update_count;
        m_sps_ids = other.m_sps_ids;
        for (u32 i = 0; i < k_max_h264_picture_parameter_sets / 32; ++i)
        {
            m_pps_ids[i] = other.m_pps_ids[i];
        }
        other.m_parameters = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::Forge::VideoSessionParameters::Destroy()
{
    if (m_parameters != VK_NULL_HANDLE)
    {
        vkDestroyVideoSessionParametersKHR(m_device->GetNativeDevice(), m_parameters, nullptr);
        m_parameters = VK_NULL_HANDLE;
    }
    m_update_count = 0;
    m_sps_ids = 0;
    for (u32& word : m_pps_ids)
    {
        word = 0;
    }
}

// VideoStatusQueryPool

Opal::Expected<Rndr::Forge::VideoStatusQueryPool, Rndr::ErrorCode> Rndr::Forge::VideoStatusQueryPool::Create(
    const Device& device, const VideoStatusQueryPoolDesc& desc)
{
    using Result = Opal::Expected<VideoStatusQueryPool, ErrorCode>;

    if (desc.query_count == 0)
    {
        RNDR_LOG_ERROR("Forge: a video status query pool needs at least one query");
        return Result(ErrorCode::InvalidArgument);
    }
    RNDR_FORGE_CHECK_EXPECTED(CheckVideoUsage(device, desc.profile, "a VideoStatusQueryPool"), Result);
    const Opal::Expected<const DeviceQueue&, ErrorCode> decode_queue = device.GetQueue(QueueFamily::Decode);
    if (!decode_queue.HasValue())
    {
        return Result(decode_queue.GetError());
    }
    const u32 family = decode_queue.GetValue().GetQueueFamilyIndex();
    if (!device.GetPhysicalDevice().SupportsQueryResultStatus(family))
    {
        RNDR_LOG_ERROR("Forge: the decode queue's family {} writes no result status, so it has no video status queries", family);
        return Result(ErrorCode::FeatureNotSupported);
    }

    VideoStatusQueryPool pool;
    pool.m_device = device;
    pool.m_desc = desc;
    const VideoProfileChain chain(desc.profile);
    const VkQueryPoolCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                            .pNext = &chain.profile,
                                            .queryType = VK_QUERY_TYPE_RESULT_STATUS_ONLY_KHR,
                                            .queryCount = desc.query_count};
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateQueryPool(device.GetNativeDevice(), &create_info, nullptr, &pool.m_query_pool),
                                 "vkCreateQueryPool", Result);
    return Result(std::move(pool));
}

Rndr::Forge::VideoStatusQueryPool::~VideoStatusQueryPool()
{
    Destroy();
}

Rndr::Forge::VideoStatusQueryPool::VideoStatusQueryPool(VideoStatusQueryPool&& other) noexcept
    : m_device(std::move(other.m_device)), m_query_pool(other.m_query_pool), m_desc(other.m_desc)
{
    other.m_query_pool = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::Forge::VideoStatusQueryPool& Rndr::Forge::VideoStatusQueryPool::operator=(VideoStatusQueryPool&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_query_pool = other.m_query_pool;
        m_desc = other.m_desc;
        other.m_query_pool = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::Forge::VideoStatusQueryPool::Destroy()
{
    if (m_query_pool != VK_NULL_HANDLE)
    {
        vkDestroyQueryPool(m_device->GetNativeDevice(), m_query_pool, nullptr);
        m_query_pool = VK_NULL_HANDLE;
    }
}

Opal::Expected<Rndr::Forge::VideoDecodeStatus, Rndr::ErrorCode> Rndr::Forge::VideoStatusQueryPool::GetStatus(u32 query) const
{
    return ReadStatus(query, true);
}

Opal::Expected<Rndr::Forge::VideoDecodeStatus, Rndr::ErrorCode> Rndr::Forge::VideoStatusQueryPool::TryGetStatus(u32 query) const
{
    return ReadStatus(query, false);
}

Opal::Expected<Rndr::Forge::VideoDecodeStatus, Rndr::ErrorCode> Rndr::Forge::VideoStatusQueryPool::ReadStatus(u32 query, bool wait) const
{
    using Result = Opal::Expected<VideoDecodeStatus, ErrorCode>;

    if (!IsValid() || query >= m_desc.query_count)
    {
        RNDR_LOG_ERROR("Forge: reading status query {} of a video status query pool holding {}", query, IsValid() ? m_desc.query_count : 0);
        return Result(ErrorCode::OutOfBounds);
    }
    // VkQueryResultStatusKHR: negative for a failed decode, zero for one not done yet, positive for one that succeeded.
    i64 status = 0;
    const VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_STATUS_BIT_KHR |
                                     (wait ? static_cast<VkQueryResultFlags>(VK_QUERY_RESULT_WAIT_BIT) : 0u);
    const VkResult result =
        vkGetQueryPoolResults(m_device->GetNativeDevice(), m_query_pool, query, 1, sizeof(status), &status, sizeof(status), flags);
    if (result == VK_NOT_READY && !wait)
    {
        return Result(VideoDecodeStatus::NotReady);
    }
    if (result != VK_SUCCESS)
    {
        RNDR_LOG_ERROR("Forge: {} failed: {}", "vkGetQueryPoolResults", VkResultToString(result));
        return Result(VkResultToErrorCode(result));
    }
    if (status < 0)
    {
        return Result(VideoDecodeStatus::Error);
    }
    return Result(status > 0 ? VideoDecodeStatus::Complete : VideoDecodeStatus::NotReady);
}
