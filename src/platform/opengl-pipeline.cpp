#include "rndr/platform/opengl-pipeline.hpp"

#include "glad/glad.h"

#include "opal/container/dynamic-array.h"

#include "opengl-helpers.hpp"
#include "rndr/log.hpp"
#include "rndr/platform/opengl-buffer.hpp"
#include "rndr/platform/opengl-shader.hpp"
#include "rndr/trace.hpp"

Rndr::Pipeline::Pipeline(const GraphicsContext& graphics_context, const PipelineDesc& desc) : m_desc(desc)
{
    RNDR_UNUSED(graphics_context);

    RNDR_CPU_EVENT_SCOPED("Create Pipeline");

    // Setup shader program
    m_native_shader_program = glCreateProgram();
    RNDR_ASSERT_OPENGL();

    if (desc.vertex_shader != nullptr)
    {
        glAttachShader(m_native_shader_program, desc.vertex_shader->GetNativeShader());
        RNDR_ASSERT_OPENGL();
    }
    if (desc.pixel_shader != nullptr)
    {
        glAttachShader(m_native_shader_program, desc.pixel_shader->GetNativeShader());
        RNDR_ASSERT_OPENGL();
    }
    if (desc.geometry_shader != nullptr)
    {
        glAttachShader(m_native_shader_program, desc.geometry_shader->GetNativeShader());
        RNDR_ASSERT_OPENGL();
    }
    if (desc.tesselation_control_shader != nullptr)
    {
        glAttachShader(m_native_shader_program, desc.tesselation_control_shader->GetNativeShader());
        RNDR_ASSERT_OPENGL();
    }
    if (desc.tesselation_evaluation_shader != nullptr)
    {
        glAttachShader(m_native_shader_program, desc.tesselation_evaluation_shader->GetNativeShader());
        RNDR_ASSERT_OPENGL();
    }
    if (desc.compute_shader != nullptr)
    {
        glAttachShader(m_native_shader_program, desc.compute_shader->GetNativeShader());
        RNDR_ASSERT_OPENGL();
    }
    glLinkProgram(m_native_shader_program);
    RNDR_ASSERT_OPENGL();
    GLint success = GL_FALSE;
    glGetProgramiv(m_native_shader_program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE)
    {
        constexpr size_t k_error_log_size = 1024;
        GLchar error_log[k_error_log_size] = {0};
        glGetProgramInfoLog(m_native_shader_program, k_error_log_size, nullptr, error_log);
        RNDR_ASSERT_OPENGL();
    }
    glValidateProgram(m_native_shader_program);
    RNDR_ASSERT_OPENGL();
    glGetProgramiv(m_native_shader_program, GL_VALIDATE_STATUS, &success);
    if (success == GL_FALSE)
    {
        constexpr size_t k_error_log_size = 1024;
        GLchar error_log[k_error_log_size] = {0};
        glGetProgramInfoLog(m_native_shader_program, k_error_log_size, nullptr, error_log);
        RNDR_ASSERT_OPENGL();
    }

    // Setup vertex array
    glCreateVertexArrays(1, &m_native_vertex_array);
    RNDR_ASSERT_OPENGL();
    glBindVertexArray(m_native_vertex_array);
    RNDR_ASSERT_OPENGL();
    const InputLayoutDesc& input_layout_desc = m_desc.input_layout;
    for (int i = 0; i < input_layout_desc.vertex_buffers.GetSize(); i++)
    {
        const Buffer& buffer = input_layout_desc.vertex_buffers[i].Get();
        const BufferDesc& buffer_desc = buffer.GetDesc();
        const int32_t binding_index = input_layout_desc.vertex_buffer_binding_slots[i];
        if (buffer_desc.type == BufferType::Vertex)
        {
            RNDR_ASSERT(buffer_desc.stride > static_cast<int64_t>(INT32_MIN) && buffer_desc.stride < static_cast<int64_t>(INT32_MAX),
                "Buffer stride is out of range!");
            glVertexArrayVertexBuffer(m_native_vertex_array, binding_index, buffer.GetNativeBuffer(), buffer_desc.offset,
                                      static_cast<int32_t>(buffer_desc.stride));
            RNDR_ASSERT_OPENGL();
            RNDR_LOG_DEBUG("Added vertex buffer %u to pipeline's vertex array buffer %u, binding index: %d, offset: %d, stride: %d",
                           buffer.GetNativeBuffer(), m_native_vertex_array, binding_index, buffer_desc.offset, buffer_desc.stride);
        }
        else if (buffer_desc.type == BufferType::ShaderStorage)
        {
            glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding_index, buffer.GetNativeBuffer(), 0, buffer_desc.size);
            RNDR_ASSERT_OPENGL();
        }
    }
    for (int i = 0; i < input_layout_desc.elements.GetSize(); i++)
    {
        const InputLayoutElement& element = input_layout_desc.elements[i];
        const int32_t attribute_index = i;
        const GLenum should_normalize_data = FromPixelFormatToShouldNormalizeData(element.format);
        const GLint component_count = FromPixelFormatToComponentCount(element.format);
        const GLenum data_type = FromPixelFormatToDataType(element.format);
        glEnableVertexArrayAttrib(m_native_vertex_array, attribute_index);
        RNDR_ASSERT_OPENGL();
        if (IsPixelFormatInteger(element.format))
        {
            glVertexArrayAttribIFormat(m_native_vertex_array, attribute_index, component_count, data_type, element.offset_in_vertex);
            RNDR_ASSERT_OPENGL();
        }
        else
        {
            glVertexArrayAttribFormat(m_native_vertex_array, attribute_index, component_count, data_type,
                                      static_cast<GLboolean>(should_normalize_data), element.offset_in_vertex);
            RNDR_ASSERT_OPENGL();
        }
        glVertexArrayAttribBinding(m_native_vertex_array, attribute_index, element.binding_index);
        RNDR_ASSERT_OPENGL();
        if (element.repetition == DataRepetition::PerInstance)
        {
            glVertexArrayBindingDivisor(m_native_vertex_array, element.binding_index, element.instance_step_rate);
            RNDR_ASSERT_OPENGL();
        }
        RNDR_LOG_DEBUG(
            "Added attribute at index %d to vertex array buffer %u, binding index: %d, component count: %d, data type: %s, should "
            "normalize data: %s, offset in vertex: %d",
            attribute_index, m_native_vertex_array, element.binding_index, component_count, FromOpenGLDataTypeToString(data_type).GetData(),
            should_normalize_data ? "GL_TRUE" : "GL_FALSE", element.offset_in_vertex);
    }
    if (input_layout_desc.index_buffer.IsValid())
    {
        const Buffer& buffer = input_layout_desc.index_buffer.Get();
        RNDR_ASSERT(buffer.GetDesc().type == BufferType::Index, "Buffer is not an index buffer!");
        glVertexArrayElementBuffer(m_native_vertex_array, buffer.GetNativeBuffer());
        RNDR_ASSERT_OPENGL();
        RNDR_LOG_DEBUG("Added index buffer %u to vertex array buffer %u", buffer.GetNativeBuffer(), m_native_vertex_array);
    }
}

Rndr::Pipeline::~Pipeline()
{
    Destroy();
}

Rndr::Pipeline::Pipeline(Pipeline&& other) noexcept
    : m_desc(std::move(other.m_desc)),
      m_native_shader_program(other.m_native_shader_program),
      m_native_vertex_array(other.m_native_vertex_array)
{
    other.m_native_shader_program = k_invalid_opengl_object;
    other.m_native_vertex_array = k_invalid_opengl_object;
}

Rndr::Pipeline& Rndr::Pipeline::operator=(Pipeline&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_desc = other.m_desc;
        m_native_shader_program = other.m_native_shader_program;
        m_native_vertex_array = other.m_native_vertex_array;
        other.m_native_shader_program = k_invalid_opengl_object;
        other.m_native_vertex_array = k_invalid_opengl_object;
    }
    return *this;
}

void Rndr::Pipeline::Destroy()
{
    if (m_native_shader_program != k_invalid_opengl_object)
    {
        glDeleteProgram(m_native_shader_program);
        RNDR_ASSERT_OPENGL();
        m_native_shader_program = k_invalid_opengl_object;
    }
    if (m_native_vertex_array != k_invalid_opengl_object)
    {
        glDeleteVertexArrays(1, &m_native_vertex_array);
        RNDR_ASSERT_OPENGL();
        m_native_vertex_array = k_invalid_opengl_object;
    }
}

bool Rndr::Pipeline::IsValid() const
{
    return m_native_shader_program != k_invalid_opengl_object && m_native_vertex_array != k_invalid_opengl_object;
}

const Rndr::PipelineDesc& Rndr::Pipeline::GetDesc() const
{
    return m_desc;
}

GLuint Rndr::Pipeline::GetNativeShaderProgram() const
{
    return m_native_shader_program;
}

GLuint Rndr::Pipeline::GetNativeVertexArray() const
{
    return m_native_vertex_array;
}

bool Rndr::Pipeline::IsIndexBufferBound() const
{
    return m_desc.input_layout.index_buffer.IsValid();
}

int64_t Rndr::Pipeline::GetIndexBufferElementSize() const
{
    RNDR_ASSERT(IsIndexBufferBound(), "Index buffer is not bound!");
    const Buffer& index_buffer = *m_desc.input_layout.index_buffer;
    const BufferDesc& index_buffer_desc = index_buffer.GetDesc();
    return index_buffer_desc.stride;
}
