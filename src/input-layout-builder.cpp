#include "rndr/input-layout-builder.h"

#include "rndr/log.h"
#include "rndr/render-api.h"

Rndr::InputLayoutBuilder& Rndr::InputLayoutBuilder::AddVertexBuffer(const Buffer& buffer, i32 buffer_index, DataRepetition repetition,
                                                                    i32 per_instance_rate)
{
    m_buffers.try_emplace(buffer_index, Opal::Ref(buffer), repetition, per_instance_rate);
    return *this;
}

Rndr::InputLayoutBuilder& Rndr::InputLayoutBuilder::AddShaderStorage(const Rndr::Buffer& buffer, i32 buffer_index)
{
    m_buffers.try_emplace(buffer_index, Opal::Ref(buffer), DataRepetition::PerInstance, 0);
    return *this;
}

Rndr::InputLayoutBuilder& Rndr::InputLayoutBuilder::AppendElement(i32 buffer_index, PixelFormat format)
{
    auto buffer_it = m_buffers.find(buffer_index);
    if (buffer_it == m_buffers.end())
    {
        RNDR_LOG_ERROR("Failed since the buffer index is not present, call AddVertexBuffer first!");
        return *this;
    }
    if (m_elements.GetSize() == GraphicsConstants::k_max_input_layout_entries)
    {
        RNDR_LOG_ERROR("Failed since there are no more slots available!");
        return *this;
    }

    BufferInfo& info = buffer_it->second;
    i32 instance_rate = info.repetition == DataRepetition::PerVertex ? 0 : info.per_instance_rate;
    if (instance_rate < 1)
    {
        instance_rate = 1;
    }
    const InputLayoutElement element{.format = format,
                                     .offset_in_vertex = info.offset,
                                     .binding_index = buffer_index,
                                     .repetition = info.repetition,
                                     .instance_step_rate = instance_rate};

    info.offset += FromPixelFormatToPixelSize(format);
    info.entries_count++;
    m_elements.PushBack(element);

    return *this;
}

Rndr::InputLayoutBuilder& Rndr::InputLayoutBuilder::AddIndexBuffer(const Buffer& buffer)
{
    m_index_buffer = buffer;
    return *this;
}

Rndr::InputLayoutDesc Rndr::InputLayoutBuilder::Build()
{
    Opal::DynamicArray<Opal::Ref<const Buffer>> buffers;
    Opal::DynamicArray<i32> buffer_binding_slots;
    buffers.Reserve(m_buffers.size());
    for (auto const& [binding_index, buffer_info] : m_buffers)
    {
        buffers.PushBack(buffer_info.buffer);
        buffer_binding_slots.PushBack(binding_index);
    }
    return InputLayoutDesc{.index_buffer = m_index_buffer,
                           .vertex_buffers = buffers,
                           .vertex_buffer_binding_slots = buffer_binding_slots,
                           .elements = m_elements};
}
