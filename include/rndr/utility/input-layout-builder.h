#pragma once

#include "opal/container/hash-map.h"
#include "opal/container/string-hash.h"

#include "rndr/core/graphics-types.h"

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
    InputLayoutBuilder& AddVertexBuffer(const Buffer& buffer, i32 buffer_index, DataRepetition repetition,
                                        i32 per_instance_rate = 0);

    /**
     * Adds a shader storage buffer to the input layout. How these buffers are used is up to the shader.
     * @param buffer The buffer.
     * @param buffer_index Slot at which the buffer is bound.
     * @return This builder.
     */
    InputLayoutBuilder& AddShaderStorage(const Buffer& buffer, i32 buffer_index);

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
    InputLayoutBuilder& AppendElement(i32 buffer_index, PixelFormat format);

    /**
     * Builds the input layout description.
     * @return Input layout description.
     */
    InputLayoutDesc Build();

private:
    struct BufferInfo
    {
        Opal::Ref<const Buffer> buffer;
        DataRepetition repetition = DataRepetition::PerVertex;
        i32 per_instance_rate = 0;
        i32 entries_count = 0;
        i32 offset = 0;
    };

    Opal::Ref<const Buffer> m_index_buffer;
    Opal::HashMap<i32, BufferInfo> m_buffers;
    Opal::HashMap<Opal::StringUtf8, i32, Opal::Hash<Opal::StringUtf8>> m_names;
    Opal::Array<InputLayoutElement> m_elements;
};

}  // namespace Rndr
