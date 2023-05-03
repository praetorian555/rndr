#include "Rndr/utility/input-layout-builder.h"
#include "rndr/render-api/render-api.h"

Rndr::InputLayoutBuilder& Rndr::InputLayoutBuilder::AddVertexBuffer(const Ref<Buffer>& buffer,
                                                                    int32_t buffer_index,
                                                                    DataRepetition repetition,
                                                                    int32_t per_instance_rate)
{
    m_buffers.try_emplace(buffer_index, buffer, repetition, per_instance_rate);
    return *this;
}

Rndr::InputLayoutBuilder& Rndr::InputLayoutBuilder::AppendElement(int buffer_index,
                                                                  PixelFormat format)
{
    auto buffer_it = m_buffers.find(buffer_index);
    if (buffer_it == m_buffers.end())
    {
        RNDR_LOG_ERROR("Failed since the buffer index is not present, call AddVertexBuffer first!");
        return *this;
    }
    if (m_elements.size() == GraphicsConstants::kMaxInputLayoutEntries)
    {
        RNDR_LOG_ERROR("Failed since there are no more slots available!");
        return *this;
    }

    BufferInfo& info = buffer_it->second;
    int32_t instance_rate =
        info.repetition == DataRepetition::PerVertex ? 0 : info.per_instance_rate;
    if (instance_rate < 1)
    {
        instance_rate = 1;
    }
    const InputLayoutElement element{.format = format,
                                     .offset_in_vertex = info.offset,
                                     .binding_index = buffer_index,
                                     .repetition = info.repetition,
                                     .instance_step_rate = instance_rate};

    info.offset += GetPixelSize(format);
    info.entries_count++;
    m_elements.push_back(element);

    return *this;
}

Rndr::InputLayoutBuilder& Rndr::InputLayoutBuilder::AddIndexBuffer(const Ref<Buffer>& buffer)
{
    m_index_buffer = &buffer.get();
    return *this;
}

Rndr::InputLayoutDesc Rndr::InputLayoutBuilder::Build()
{
    Array<Ref<Buffer>> buffers;
    Array<int32_t> buffer_binding_slots;
    buffers.reserve(m_buffers.size());
    for (auto const& [binding_index, buffer_info] : m_buffers)
    {
        buffers.push_back(buffer_info.buffer);
        buffer_binding_slots.push_back(binding_index);
    }
    return InputLayoutDesc{.index_buffer = m_index_buffer,
                           .vertex_buffers = buffers,
                           .vertex_buffer_binding_slots = buffer_binding_slots,
                           .elements = m_elements};
}
