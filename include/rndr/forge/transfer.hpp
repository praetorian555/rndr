#pragma once

#include "opal/container/array-view.h"
#include "opal/container/expected.h"

#include "rndr/error-codes.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/types.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/types.hpp"

namespace Rndr::Forge
{

/**
 * Record commands into a one-shot command buffer, submit them, and wait until the device is done with them.
 * This is the setup-time counterpart of the frame loop: a texture upload, a readback, anything that happens
 * once and whose result is needed before the next line of C++ runs.
 *
 * @param device Device to allocate the command buffer and the fence from.
 * @param queue Queue to submit to. Its command pool is where the command buffer comes from.
 * @param recorder Callable taking a CommandBuffer&, between Begin and End. What it records reports for
 *        itself: this hands back what the command buffer, the submit and the wait reported, and whatever the
 *        commands themselves said is the recorder's to keep.
 * @return ErrorCode::Success, or the first code any of the steps around the recording reported.
 */
template <typename Recorder>
[[nodiscard]] ErrorCode ImmediateSubmit(const Device& device, DeviceQueue& queue, Recorder&& recorder)
{
    Opal::Expected<CommandBuffer, ErrorCode> command_buffer = CommandBuffer::Create(device, queue);
    if (!command_buffer.HasValue())
    {
        return command_buffer.GetError();
    }
    RNDR_FORGE_CHECK(command_buffer.GetValue().Begin());
    recorder(command_buffer.GetValue());
    RNDR_FORGE_CHECK(command_buffer.GetValue().End());
    Opal::Expected<Fence, ErrorCode> fence = Fence::Create(device, false);
    if (!fence.HasValue())
    {
        return fence.GetError();
    }
    RNDR_FORGE_CHECK(queue.Submit(command_buffer.GetValue(), fence.GetValue()));
    return fence.GetValue().Wait();
}

/**
 * Fill a buffer through a temporary staging buffer, for a buffer whose memory the host cannot write directly.
 * Blocks until the copy is done. Buffer::Update is the cheaper path when the memory is host visible.
 * @param destination Buffer to fill. Needs BufferUsageBits::TransferDestination.
 * @param data Bytes to write.
 * @param offset Byte offset into the destination.
 * @return ErrorCode::Success, or the first code the staging buffer, the copy or the submit reported.
 */
[[nodiscard]] ErrorCode UploadToBuffer(const Device& device, DeviceQueue& queue, const Buffer& destination, Opal::ArrayView<const u8> data,
                                       u64 offset = 0);

/**
 * Copy a range of a buffer back into host memory through a temporary staging buffer, filling the whole view.
 * Blocks until the copy is done.
 * @param source Buffer to read. Needs BufferUsageBits::TransferSource.
 * @param out Where the bytes go. Its size is how much is read.
 * @param offset Byte offset into the source.
 * @return ErrorCode::Success, or the first code the staging buffer, the copy, the submit or the read reported.
 */
[[nodiscard]] ErrorCode ReadBackBuffer(const Device& device, DeviceQueue& queue, const Buffer& source, Opal::ArrayView<u8> out,
                                       u64 offset = 0);

/**
 * Copy one mip level of a texture back into host memory, tightly packed. Blocks until the copy is done, and
 * leaves the texture in final_layout.
 * @param source Texture to read. Needs TextureUsageBits::TransferSource. Where it is now comes off the
 *        texture, so every level of it has to be in the same layout.
 * @param out Where the pixels go. Must be exactly the size of the mip level; anything else is refused.
 * @param mip_level Which mip level to read.
 * @param final_layout Layout to leave the texture in. Undefined leaves it in TransferSource.
 * @return ErrorCode::Success, ErrorCode::InvalidArgument for a view that is not the size of the mip level, or
 *         the first code the staging buffer, the copy, the submit or the read reported.
 */
[[nodiscard]] ErrorCode ReadBackTexture(const Device& device, DeviceQueue& queue, Texture& source, Opal::ArrayView<u8> out,
                                        u32 mip_level = 0, ImageLayout final_layout = ImageLayout::ShaderReadOnly);

/**
 * The size in bytes of one tightly packed mip level of a texture, every array layer included.
 * @return The size, ErrorCode::UnsupportedFormat for a compressed format, whose block layout Forge does not
 *         know, or ErrorCode::OutOfBounds when the texture has no such mip level.
 */
[[nodiscard]] Opal::Expected<u64, ErrorCode> GetMipLevelSize(const TextureDesc& desc, u32 mip_level);

}  // namespace Rndr::Forge
