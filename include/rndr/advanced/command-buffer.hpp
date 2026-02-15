#pragma once

#include "volk/volk.h"

#include "opal/container/ref.h"

#include "rndr/advanced/synchronization.hpp"

namespace Rndr
{

enum class AttachmentLoadOperation : u8
{
    Load,
    Clear,
    DontCare
};

enum class AttachmentStoreOperation : u8
{
    Store,
    DontCare
};

struct AdvancedRenderingAttachmentDesc
{
    VkImageView image_view;
    ImageLayout image_layout;
    AttachmentLoadOperation load_operation;
    AttachmentStoreOperation store_operation;
    union
    {
        Vector4f color;
        struct
        {
            f32 depth;
            u32 stencil;
        } depth_stencil;
    } clear_value;
};

struct AdvancedRenderingDesc
{
    Vector2i render_area_extent;
    Opal::DynamicArray<AdvancedRenderingAttachmentDesc> color_attachments;
    AdvancedRenderingAttachmentDesc depth_attachment;
};

class AdvancedCommandBuffer
{
public:
    AdvancedCommandBuffer() = default;
    AdvancedCommandBuffer(const class AdvancedDevice& device, class AdvancedDeviceQueue& queue);
    ~AdvancedCommandBuffer();

    AdvancedCommandBuffer(const AdvancedCommandBuffer&) = delete;
    AdvancedCommandBuffer& operator=(const AdvancedCommandBuffer&) = delete;

    AdvancedCommandBuffer(AdvancedCommandBuffer&& other) noexcept;
    AdvancedCommandBuffer& operator=(AdvancedCommandBuffer&& other) noexcept;

    void Destroy();

    [[nodiscard]] VkCommandBuffer GetNativeCommandBuffer() const { return m_native_command_buffer; }

    void Begin(bool submit_one_time = true) const;
    void End() const;
    void Reset() const;

    void CmdImageBarrier(const AdvancedImageBarrier& image_barrier);
    void CmdImageBarriers(const Opal::ArrayView<AdvancedImageBarrier>& image_barriers);
    void CmdCopyBufferToImage(const class AdvancedBuffer& buffer, const Bitmap& bitmap, AdvancedTexture& texture);
    void CmdBeginRendering(const AdvancedRenderingDesc& desc);
    void CmdEndRendering();

private:
    Opal::Ref<const class AdvancedDevice> m_device;
    Opal::Ref<class AdvancedDeviceQueue> m_queue;
    VkCommandBuffer m_native_command_buffer = VK_NULL_HANDLE;
};

}  // namespace Rndr