#pragma once

#include "opal/container/ref.h"

#include "rndr/advanced/synchronization.hpp"

namespace Rndr
{

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

    void CmdImageBarrier(const AdvancedImageBarrier& image_barrier);

private:
    Opal::Ref<const class AdvancedDevice> m_device;
    Opal::Ref<class AdvancedDeviceQueue> m_queue;
    VkCommandBuffer m_native_command_buffer = VK_NULL_HANDLE;
};

}  // namespace Rndr