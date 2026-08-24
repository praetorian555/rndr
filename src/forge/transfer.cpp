#include "rndr/forge/transfer.hpp"

#include "rndr/forge/buffer.hpp"
#include "rndr/forge/texture.hpp"
#include "rndr/pixel-format.hpp"

#include "rndr/log.hpp"

Opal::Expected<Rndr::u64, Rndr::ErrorCode> Rndr::Forge::GetMipLevelSize(const TextureDesc& desc, u32 mip_level)
{
    using Result = Opal::Expected<u64, ErrorCode>;

    const u32 pixel_size = GetPixelSize(desc.format);
    if (pixel_size == 0)
    {
        RNDR_LOG_ERROR("Forge: the size of a mip level of a compressed format is not known to Forge");
        return Result(ErrorCode::UnsupportedFormat);
    }
    if (mip_level >= desc.mip_level_count)
    {
        RNDR_LOG_ERROR("Forge: the texture has no mip level {}, it has {}", mip_level, desc.mip_level_count);
        return Result(ErrorCode::OutOfBounds);
    }
    const u64 width = Opal::Max(1u, desc.width >> mip_level);
    const u64 height = Opal::Max(1u, desc.height >> mip_level);
    const u64 depth = Opal::Max(1u, desc.depth >> mip_level);
    return Result(static_cast<u64>(pixel_size) * width * height * depth * desc.array_layer_count);
}

Rndr::ErrorCode Rndr::Forge::UploadToBuffer(const Device& device, DeviceQueue& queue, const Buffer& destination,
                                            Opal::ArrayView<const u8> data, u64 offset)
{
    if (data.IsEmpty())
    {
        return ErrorCode::Success;
    }
    Opal::Expected<Buffer, ErrorCode> staging_buffer =
        Buffer::Create(device, {.size = data.GetSize(), .usage = BufferUsageBits::TransferSource});
    if (!staging_buffer.HasValue())
    {
        return staging_buffer.GetError();
    }
    RNDR_FORGE_CHECK(staging_buffer.GetValue().Update(data));
    const BufferCopyRegion region{.source_offset = 0, .destination_offset = offset, .size = data.GetSize()};
    ErrorCode record_status = ErrorCode::Success;
    const ErrorCode submit_status =
        ImmediateSubmit(device, queue, [&](CommandBuffer& command_buffer)
                        { record_status = command_buffer.CmdCopyBuffer(staging_buffer.GetValue(), destination, {&region, 1}); });
    RNDR_FORGE_CHECK(record_status);
    return submit_status;
}

Rndr::ErrorCode Rndr::Forge::ReadBackBuffer(const Device& device, DeviceQueue& queue, const Buffer& source, Opal::ArrayView<u8> out,
                                            u64 offset)
{
    if (out.IsEmpty())
    {
        return ErrorCode::Success;
    }
    Opal::Expected<Buffer, ErrorCode> staging_buffer =
        Buffer::Create(device, {.size = out.GetSize(), .usage = BufferUsageBits::TransferDestination, .host_access = HostAccess::Random});
    if (!staging_buffer.HasValue())
    {
        return staging_buffer.GetError();
    }
    const BufferCopyRegion region{.source_offset = offset, .destination_offset = 0, .size = out.GetSize()};
    ErrorCode record_status = ErrorCode::Success;
    const ErrorCode submit_status =
        ImmediateSubmit(device, queue, [&](CommandBuffer& command_buffer)
                        { record_status = command_buffer.CmdCopyBuffer(source, staging_buffer.GetValue(), {&region, 1}); });
    RNDR_FORGE_CHECK(record_status);
    RNDR_FORGE_CHECK(submit_status);
    // No barrier to the host is needed: signalling the fence the submit waited on makes every write the device
    // performed available to the host domain, and Buffer::Read invalidates the cache on top of that.
    return staging_buffer.GetValue().Read(out);
}

Rndr::ErrorCode Rndr::Forge::ReadBackTexture(const Device& device, DeviceQueue& queue, Texture& source, Opal::ArrayView<u8> out,
                                             u32 mip_level, ImageLayout final_layout)
{
    const TextureDesc& desc = source.GetDesc();
    const Opal::Expected<u64, ErrorCode> mip_size = GetMipLevelSize(desc, mip_level);
    if (!mip_size.HasValue())
    {
        return mip_size.GetError();
    }
    if (out.GetSize() != mip_size.GetValue())
    {
        RNDR_LOG_ERROR("Forge: reading a texture back needs a view the exact size of the mip level, {} rather than {}", mip_size.GetValue(),
                       out.GetSize());
        return ErrorCode::InvalidArgument;
    }
    Opal::Expected<Buffer, ErrorCode> staging_buffer = Buffer::Create(
        device, {.size = mip_size.GetValue(), .usage = BufferUsageBits::TransferDestination, .host_access = HostAccess::Random});
    if (!staging_buffer.HasValue())
    {
        return staging_buffer.GetError();
    }
    const BufferTextureCopyRegion region{.texture_subresource = {.mip_level = mip_level, .array_layer_count = desc.array_layer_count}};
    ErrorCode record_status = ErrorCode::Success;
    const ErrorCode submit_status =
        ImmediateSubmit(device, queue,
                        [&](CommandBuffer& command_buffer)
                        {
                            record_status = command_buffer.CmdTextureBarrier(TextureBarrier::ToTransferSource(source));
                            if (record_status != ErrorCode::Success)
                            {
                                return;
                            }
                            record_status = command_buffer.CmdCopyTextureToBuffer(source, staging_buffer.GetValue(), {&region, 1});
                            if (record_status != ErrorCode::Success)
                            {
                                return;
                            }
                            if (final_layout != ImageLayout::Undefined && final_layout != ImageLayout::TransferSource)
                            {
                                record_status = command_buffer.CmdTransition(source, final_layout);
                            }
                        });
    RNDR_FORGE_CHECK(record_status);
    RNDR_FORGE_CHECK(submit_status);
    return staging_buffer.GetValue().Read(out);
}
