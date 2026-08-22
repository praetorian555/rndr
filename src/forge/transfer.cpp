#include "rndr/forge/transfer.hpp"

#include "rndr/forge/buffer.hpp"
#include "rndr/forge/texture.hpp"
#include "rndr/pixel-format.hpp"

Rndr::u64 Rndr::Forge::GetMipLevelSize(const TextureDesc& desc, u32 mip_level)
{
    const u32 pixel_size = GetPixelSize(desc.format);
    if (pixel_size == 0)
    {
        throw Opal::Exception("The size of a mip level of a compressed format is not known to Forge!");
    }
    if (mip_level >= desc.mip_level_count)
    {
        throw Opal::Exception("The texture does not have that mip level!");
    }
    const u64 width = Opal::Max(1u, desc.width >> mip_level);
    const u64 height = Opal::Max(1u, desc.height >> mip_level);
    const u64 depth = Opal::Max(1u, desc.depth >> mip_level);
    return static_cast<u64>(pixel_size) * width * height * depth * desc.array_layer_count;
}

void Rndr::Forge::UploadToBuffer(const Device& device, DeviceQueue& queue, const Buffer& destination, Opal::ArrayView<const u8> data,
                                 u64 offset)
{
    if (data.IsEmpty())
    {
        return;
    }
    const Buffer staging_buffer(device, {.size = data.GetSize(), .usage = BufferUsageBits::TransferSource});
    staging_buffer.Update(data);
    const BufferCopyRegion region{.source_offset = 0, .destination_offset = offset, .size = data.GetSize()};
    ImmediateSubmit(device, queue,
                    [&](CommandBuffer& command_buffer) { command_buffer.CmdCopyBuffer(staging_buffer, destination, {&region, 1}); });
}

void Rndr::Forge::ReadBackBuffer(const Device& device, DeviceQueue& queue, const Buffer& source, Opal::ArrayView<u8> out, u64 offset)
{
    if (out.IsEmpty())
    {
        return;
    }
    const Buffer staging_buffer(
        device, {.size = out.GetSize(), .usage = BufferUsageBits::TransferDestination, .host_access = HostAccess::Random});
    const BufferCopyRegion region{.source_offset = offset, .destination_offset = 0, .size = out.GetSize()};
    ImmediateSubmit(device, queue,
                    [&](CommandBuffer& command_buffer) { command_buffer.CmdCopyBuffer(source, staging_buffer, {&region, 1}); });
    // No barrier to the host is needed: signalling the fence the submit waited on makes every write the device
    // performed available to the host domain, and Buffer::Read invalidates the cache on top of that.
    staging_buffer.Read(out);
}

void Rndr::Forge::ReadBackTexture(const Device& device, DeviceQueue& queue, Texture& source, Opal::ArrayView<u8> out, u32 mip_level,
                                  ImageLayout final_layout)
{
    const TextureDesc& desc = source.GetDesc();
    const u64 mip_size = GetMipLevelSize(desc, mip_level);
    if (out.GetSize() != mip_size)
    {
        throw Opal::Exception("Reading a texture back needs a view the exact size of the mip level!");
    }
    const Buffer staging_buffer(device,
                                {.size = mip_size, .usage = BufferUsageBits::TransferDestination, .host_access = HostAccess::Random});
    const BufferTextureCopyRegion region{
        .texture_subresource = {.mip_level = mip_level, .array_layer_count = desc.array_layer_count}};
    ImmediateSubmit(device, queue,
                    [&](CommandBuffer& command_buffer)
                    {
                        command_buffer.CmdTextureBarrier(TextureBarrier::ToTransferSource(source));
                        command_buffer.CmdCopyTextureToBuffer(source, staging_buffer, {&region, 1});
                        if (final_layout != ImageLayout::Undefined && final_layout != ImageLayout::TransferSource)
                        {
                            command_buffer.CmdTransition(source, final_layout);
                        }
                    });
    staging_buffer.Read(out);
}
