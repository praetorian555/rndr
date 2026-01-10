#pragma once

#include "vma/vk_mem_alloc.h"
#include "volk/volk.h"

#include "opal/container/ref.h"
#include "opal/container/array-view.h"

#include "rndr/types.hpp"

namespace Rndr
{

struct AdvancedBufferDesc
{
    size_t size;
    VkBufferUsageFlags usage;
};

class AdvancedBuffer
{
public:
    AdvancedBuffer() = default;
    explicit AdvancedBuffer(const class AdvancedDevice& device, const AdvancedBufferDesc& desc = {}, Opal::ArrayView<u8> initial_data = {});
    ~AdvancedBuffer();

    AdvancedBuffer(const AdvancedBuffer&) = delete;
    AdvancedBuffer& operator=(const AdvancedBuffer&) = delete;
    AdvancedBuffer(AdvancedBuffer&&) noexcept;
    AdvancedBuffer& operator=(AdvancedBuffer&&) noexcept;

    void Destroy();

    [[nodiscard]] VkBuffer GetNativeBuffer() const { return m_buffer; }

    void Update(Opal::ArrayView<u8> data, size_t offset) const;

private:
    AdvancedBufferDesc m_desc;
    Opal::Ref<const class AdvancedDevice> m_device;
    VkBuffer m_buffer;
    VmaAllocation m_allocation;
};

}  // namespace Rndr