#pragma once

#include "rndr/core/graphics-types.h"
#include "rndr/core/hash-map.h"

namespace Rndr
{

class Buffer;

/**
 * Helper class to build input layout description. It uses builder pattern.
 */
struct InputLayoutBuilder
{
    InputLayoutBuilder() = default;
    ~InputLayoutBuilder() = default;

    InputLayoutBuilder(const InputLayoutBuilder& other) = default;
    InputLayoutBuilder& operator=(const InputLayoutBuilder& other) = default;

    InputLayoutBuilder(InputLayoutBuilder&& other) = default;
    InputLayoutBuilder& operator=(InputLayoutBuilder&& other) = default;

    /**
     * Adds a vertex buffer to the input layout.
     * @param buffer_index Slot at which the buffer is bound.
     * @param repetition How often the data is repeated.
     * @param per_instance_rate How often the data is repeated per instance. Default is 0.
     * @return This builder.
     */
    InputLayoutBuilder& AddVertexBuffer(const Buffer& buffer,
                                        int32_t buffer_index,
                                        DataRepetition repetition,
                                        int32_t per_instance_rate = 0);

    /**
     * Adds an index buffer to the input layout. There can be only one. It is ok to not have it.
     * If you call this function twice the second call will override the first one.
     * @param buffer The index buffer.
     * @return This builder.
     */
    InputLayoutBuilder& AddIndexBuffer(const Buffer& buffer);

    /**
     * Appends an element to the input layout.
     * @param buffer_index Buffer index to which the element belongs.
     * @param format Format of the element.
     * @return This builder.
     */
    InputLayoutBuilder& AppendElement(int buffer_index, PixelFormat format);

    /**
     * Builds the input layout description.
     * @return Input layout description.
     */
    InputLayoutDesc Build();

private:
    struct BufferInfo
    {
        Ref<const Buffer> buffer;
        DataRepetition repetition = DataRepetition::PerVertex;
        int per_instance_rate = 0;
        int entries_count = 0;
        int offset = 0;
    };

    Ref<const Buffer> m_index_buffer;
    HashMap<int, BufferInfo> m_buffers;
    HashMap<String, int> m_names;
    Array<InputLayoutElement> m_elements;
};

}  // namespace Rndr
