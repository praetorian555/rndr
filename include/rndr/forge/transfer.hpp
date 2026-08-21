#pragma once

#include "opal/container/array-view.h"

#include "rndr/types.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/types.hpp"

namespace Rndr::Forge
{

/**
 * Record commands into a one-shot command buffer, submit them, and wait until the device is done with them.
 * This is the setup-time counterpart of the frame loop: a texture upload, a readback, anything that happens
 * once and whose result is needed before the next line of C++ runs.
 *
 * @param device Device to allocate the command buffer and the fence from.
 * @param queue Queue to submit to. Its command pool is where the command buffer comes from.
 * @param recorder Callable taking a CommandBuffer&, between Begin and End.
 */
template <typename Recorder>
void ImmediateSubmit(const Device& device, DeviceQueue& queue, Recorder&& recorder)
{
    CommandBuffer command_buffer(device, queue);
    command_buffer.Begin();
    recorder(command_buffer);
    command_buffer.End();
    const Fence fence(device, false);
    queue.Submit(command_buffer, fence);
    fence.Wait();
}

/**
 * Fill a buffer through a temporary staging buffer, for a buffer whose memory the host cannot write directly.
 * Blocks until the copy is done. Buffer::Update is the cheaper path when the memory is host visible.
 * @param destination Buffer to fill. Needs BufferUsageBits::TransferDestination.
 * @param data Bytes to write.
 * @param offset Byte offset into the destination.
 */
void UploadToBuffer(const Device& device, DeviceQueue& queue, const Buffer& destination, Opal::ArrayView<const u8> data, u64 offset = 0);

/**
 * Copy a range of a buffer back into host memory through a temporary staging buffer, filling the whole view.
 * Blocks until the copy is done.
 * @param source Buffer to read. Needs BufferUsageBits::TransferSource.
 * @param out Where the bytes go. Its size is how much is read.
 * @param offset Byte offset into the source.
 */
void ReadBackBuffer(const Device& device, DeviceQueue& queue, const Buffer& source, Opal::ArrayView<u8> out, u64 offset = 0);

/**
 * Copy one mip level of a texture back into host memory, tightly packed. Blocks until the copy is done, and
 * leaves the texture in final_layout.
 * @param source Texture to read. Needs TextureUsageBits::TransferSource.
 * @param current_layout Layout the texture is in now.
 * @param out Where the pixels go. Must be exactly the size of the mip level, which throws when it is not.
 * @param mip_level Which mip level to read.
 * @param final_layout Layout to leave the texture in. Undefined leaves it in TransferSource.
 */
void ReadBackTexture(const Device& device, DeviceQueue& queue, Texture& source, ImageLayout current_layout, Opal::ArrayView<u8> out,
                     u32 mip_level = 0, ImageLayout final_layout = ImageLayout::ShaderReadOnly);

/** The size in bytes of one tightly packed mip level of a texture, every array layer included. */
[[nodiscard]] u64 GetMipLevelSize(const TextureDesc& desc, u32 mip_level);

}  // namespace Rndr::Forge
