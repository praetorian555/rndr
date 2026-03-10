#include "rndr/canvas/brush.hpp"

#include "glad/glad.h"

#include "rndr/canvas/shader.hpp"
#include "rndr/canvas/texture.hpp"
#include "rndr/exception.hpp"
#include "rndr/trace.hpp"

#include <algorithm>

namespace
{

GLenum ToGLCompareFunc(Rndr::Canvas::CompareFunc func)
{
    switch (func)
    {
        case Rndr::Canvas::CompareFunc::Less:
            return GL_LESS;
        case Rndr::Canvas::CompareFunc::LessEqual:
            return GL_LEQUAL;
        case Rndr::Canvas::CompareFunc::Greater:
            return GL_GREATER;
        case Rndr::Canvas::CompareFunc::GreaterEqual:
            return GL_GEQUAL;
        case Rndr::Canvas::CompareFunc::Equal:
            return GL_EQUAL;
        case Rndr::Canvas::CompareFunc::NotEqual:
            return GL_NOTEQUAL;
        case Rndr::Canvas::CompareFunc::Always:
            return GL_ALWAYS;
        case Rndr::Canvas::CompareFunc::Never:
            return GL_NEVER;
        default:
            return GL_LESS;
    }
}

}  // namespace

Rndr::Canvas::Brush::Brush(const BrushDesc& desc) : m_desc(desc) {}

Rndr::Canvas::Brush::~Brush() = default;

Rndr::Canvas::Brush::Brush(Brush&& other) noexcept
    : m_shader(other.m_shader),
      m_desc(other.m_desc),
      m_uniforms(std::move(other.m_uniforms)),
      m_textures(std::move(other.m_textures)),
      m_buffers(std::move(other.m_buffers)),
      m_uniform_buffer_slots(std::move(other.m_uniform_buffer_slots))
{
    other.m_shader = nullptr;
    other.m_desc = {};
}

Rndr::Canvas::Brush& Rndr::Canvas::Brush::operator=(Brush&& other) noexcept
{
    if (this != &other)
    {
        m_shader = other.m_shader;
        m_desc = other.m_desc;
        m_uniforms = std::move(other.m_uniforms);
        m_textures = std::move(other.m_textures);
        m_buffers = std::move(other.m_buffers);
        m_uniform_buffer_slots = std::move(other.m_uniform_buffer_slots);
        other.m_shader = nullptr;
        other.m_desc = {};
    }
    return *this;
}

Rndr::Canvas::Brush Rndr::Canvas::Brush::Clone() const
{
    Brush clone(m_desc);
    clone.m_shader = m_shader;

    for (u64 i = 0; i < m_uniforms.GetSize(); ++i)
    {
        UniformBinding binding;
        binding.name = m_uniforms[i].name.Clone();
        binding.data.Resize(m_uniforms[i].data.GetSize());
        memcpy(binding.data.GetData(), m_uniforms[i].data.GetData(), m_uniforms[i].data.GetSize());
        clone.m_uniforms.PushBack(std::move(binding));
    }

    for (u64 i = 0; i < m_textures.GetSize(); ++i)
    {
        TextureBinding binding;
        binding.name = m_textures[i].name.Clone();
        binding.texture = m_textures[i].texture;
        clone.m_textures.PushBack(std::move(binding));
    }

    for (u64 i = 0; i < m_buffers.GetSize(); ++i)
    {
        BufferBinding binding;
        binding.name = m_buffers[i].name.Clone();
        binding.buffer = m_buffers[i].buffer;
        clone.m_buffers.PushBack(std::move(binding));
    }

    for (u64 i = 0; i < m_uniform_buffer_slots.GetSize(); ++i)
    {
        UniformBufferSlot slot;
        slot.gpu_buffer = m_uniform_buffer_slots[i].gpu_buffer.Clone();
        slot.cpu_data.Resize(m_uniform_buffer_slots[i].cpu_data.GetSize());
        memcpy(slot.cpu_data.GetData(), m_uniform_buffer_slots[i].cpu_data.GetData(), m_uniform_buffer_slots[i].cpu_data.GetSize());
        slot.binding_index = m_uniform_buffer_slots[i].binding_index;
        slot.binding_space = m_uniform_buffer_slots[i].binding_space;
        slot.dirty = m_uniform_buffer_slots[i].dirty;
        clone.m_uniform_buffer_slots.PushBack(std::move(slot));
    }

    return clone;
}

void Rndr::Canvas::Brush::SetShader(const Shader& shader)
{
    m_shader = &shader;
    CreateUniformBufferSlots();
}

const Rndr::Canvas::Shader* Rndr::Canvas::Brush::GetShader() const
{
    return m_shader;
}

void Rndr::Canvas::Brush::SetBlendMode(BlendMode mode)
{
    m_desc.blend_mode = mode;
}

void Rndr::Canvas::Brush::SetDepthTest(bool enabled)
{
    m_desc.depth_test = enabled;
}

void Rndr::Canvas::Brush::SetDepthWrite(bool enabled)
{
    m_desc.depth_write = enabled;
}

void Rndr::Canvas::Brush::SetDepthCompare(CompareFunc func)
{
    m_desc.depth_compare = func;
}

void Rndr::Canvas::Brush::SetCullMode(CullMode mode)
{
    m_desc.cull_mode = mode;
}

void Rndr::Canvas::Brush::SetFillMode(FillMode mode)
{
    m_desc.fill_mode = mode;
}

void Rndr::Canvas::Brush::SetDepthBias(f32 factor, f32 units)
{
    m_desc.depth_bias_factor = factor;
    m_desc.depth_bias_units = units;
}

const Rndr::Canvas::BrushDesc& Rndr::Canvas::Brush::GetDesc() const
{
    return m_desc;
}

void Rndr::Canvas::Brush::SetTexture(const char* name, const Texture& texture)
{
    for (u64 i = 0; i < m_textures.GetSize(); ++i)
    {
        if (m_textures[i].name == name)
        {
            m_textures[i].texture = &texture;
            return;
        }
    }

    TextureBinding binding;
    binding.name = name;
    binding.texture = &texture;
    m_textures.PushBack(std::move(binding));
}

void Rndr::Canvas::Brush::SetBuffer(const char* name, const Buffer& buffer)
{
    for (u64 i = 0; i < m_buffers.GetSize(); ++i)
    {
        if (m_buffers[i].name == name)
        {
            m_buffers[i].buffer = &buffer;
            return;
        }
    }

    BufferBinding binding;
    binding.name = name;
    binding.buffer = &buffer;
    m_buffers.PushBack(std::move(binding));
}

const Opal::DynamicArray<Rndr::Canvas::UniformBinding>& Rndr::Canvas::Brush::GetUniforms() const
{
    return m_uniforms;
}

const Opal::DynamicArray<Rndr::Canvas::TextureBinding>& Rndr::Canvas::Brush::GetTextures() const
{
    return m_textures;
}

const Opal::DynamicArray<Rndr::Canvas::BufferBinding>& Rndr::Canvas::Brush::GetBuffers() const
{
    return m_buffers;
}

bool Rndr::Canvas::Brush::IsValid() const
{
    return m_shader != nullptr;
}

void Rndr::Canvas::Brush::CreateUniformBufferSlots()
{
    m_uniform_buffer_slots.Clear();

    if (m_shader == nullptr)
    {
        return;
    }

    const Opal::DynamicArray<ShaderParameter>& params = m_shader->GetParameters();

    // Collect unique UBO binding points and compute sizes from uniform fields (size > 0).
    struct UBOInfo
    {
        i32 binding_index;
        i32 binding_space;
        i32 total_size;
    };
    Opal::DynamicArray<UBOInfo> ubo_infos;

    for (u64 i = 0; i < params.GetSize(); ++i)
    {
        const ShaderParameter& p = params[i];
        if (p.category != ParameterCategory::Uniform || p.size <= 0)
        {
            continue;
        }

        const i32 end = p.offset + p.size;
        bool found = false;
        for (u64 j = 0; j < ubo_infos.GetSize(); ++j)
        {
            if (ubo_infos[j].binding_index == p.binding_index && ubo_infos[j].binding_space == p.binding_space)
            {
                ubo_infos[j].total_size = std::max(ubo_infos[j].total_size, end);
                found = true;
                break;
            }
        }

        if (!found)
        {
            UBOInfo info;
            info.binding_index = p.binding_index;
            info.binding_space = p.binding_space;
            info.total_size = end;
            ubo_infos.PushBack(info);
        }
    }

    // Create a GPU buffer + CPU staging area for each UBO.
    for (u64 i = 0; i < ubo_infos.GetSize(); ++i)
    {
        UniformBufferSlot slot;
        slot.binding_index = ubo_infos[i].binding_index;
        slot.binding_space = ubo_infos[i].binding_space;
        slot.cpu_data.Resize(static_cast<u64>(ubo_infos[i].total_size));
        memset(slot.cpu_data.GetData(), 0, slot.cpu_data.GetSize());
        slot.gpu_buffer = Buffer(BufferUsage::Uniform, static_cast<u64>(ubo_infos[i].total_size));
        m_uniform_buffer_slots.PushBack(std::move(slot));
    }
}

void Rndr::Canvas::Brush::SetUniformRaw(const char* name, const void* data, u64 size)
{
    // If a shader is set, try to write into the appropriate UBO slot.
    if (m_shader != nullptr)
    {
        const ShaderParameter* param = m_shader->FindParameter(name);
        if (param != nullptr && param->category == ParameterCategory::Uniform && param->size > 0)
        {
            for (u64 i = 0; i < m_uniform_buffer_slots.GetSize(); ++i)
            {
                UniformBufferSlot& slot = m_uniform_buffer_slots[i];
                if (slot.binding_index == param->binding_index && slot.binding_space == param->binding_space)
                {
                    memcpy(slot.cpu_data.GetData() + param->offset, data, size);
                    slot.dirty = true;
                    return;
                }
            }
        }
    }

    // Fallback: store in m_uniforms for non-shader or non-matched params.
    for (u64 i = 0; i < m_uniforms.GetSize(); ++i)
    {
        if (m_uniforms[i].name == name)
        {
            m_uniforms[i].data.Resize(size);
            memcpy(m_uniforms[i].data.GetData(), data, size);
            return;
        }
    }

    UniformBinding binding;
    binding.name = name;
    binding.data.Resize(size);
    memcpy(binding.data.GetData(), data, size);
    m_uniforms.PushBack(std::move(binding));
}

void Rndr::Canvas::Brush::UploadUniforms()
{
    for (u64 i = 0; i < m_uniform_buffer_slots.GetSize(); ++i)
    {
        UniformBufferSlot& slot = m_uniform_buffer_slots[i];
        if (slot.dirty && slot.gpu_buffer.IsValid())
        {
            slot.gpu_buffer.Update(Opal::ArrayView<const u8>(slot.cpu_data.GetData(), slot.cpu_data.GetSize()));
            slot.dirty = false;
        }
    }
}

const Opal::DynamicArray<Rndr::Canvas::UniformBufferSlot>& Rndr::Canvas::Brush::GetUniformBufferSlots() const
{
    return m_uniform_buffer_slots;
}

void Rndr::Canvas::Brush::Apply()
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Brush::Apply");

    if (m_shader == nullptr)
    {
        return;
    }

    // 1. Bind shader program.
    glUseProgram(m_shader->GetNativeHandle());

    // 2. Depth state.
    if (m_desc.depth_test)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(ToGLCompareFunc(m_desc.depth_compare));
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(m_desc.depth_write ? GL_TRUE : GL_FALSE);

    // 3. Blend state.
    switch (m_desc.blend_mode)
    {
        case BlendMode::None:
            glDisable(GL_BLEND);
            break;
        case BlendMode::Alpha:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BlendMode::Additive:
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            break;
        case BlendMode::Multiply:
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            break;
        default:
            glDisable(GL_BLEND);
            break;
    }

    // 4. Rasterizer state.
    glPolygonMode(GL_FRONT_AND_BACK, m_desc.fill_mode == FillMode::Wireframe ? GL_LINE : GL_FILL);

    if (m_desc.cull_mode != CullMode::None)
    {
        glEnable(GL_CULL_FACE);
        glCullFace(m_desc.cull_mode == CullMode::Front ? GL_FRONT : GL_BACK);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }

    if (m_desc.depth_bias_factor != 0.0f || m_desc.depth_bias_units != 0.0f)
    {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(m_desc.depth_bias_factor, m_desc.depth_bias_units);
    }
    else
    {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
    }

    // 5. Upload dirty uniform buffers to the GPU.
    UploadUniforms();

    // 6. Bind UBOs to their binding points.
    for (u64 i = 0; i < m_uniform_buffer_slots.GetSize(); ++i)
    {
        const UniformBufferSlot& slot = m_uniform_buffer_slots[i];
        if (slot.gpu_buffer.IsValid())
        {
            glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(slot.binding_index), slot.gpu_buffer.GetNativeHandle());
        }
    }

    // 7. Bind textures. Look up the binding index from shader reflection.
    for (u64 i = 0; i < m_textures.GetSize(); ++i)
    {
        const TextureBinding& tb = m_textures[i];
        if (tb.texture == nullptr || !tb.texture->IsValid())
        {
            continue;
        }
        const ShaderParameter* param = m_shader->FindParameter(tb.name);
        if (param != nullptr && param->category == ParameterCategory::Texture)
        {
            glBindTextureUnit(static_cast<GLuint>(param->binding_index), tb.texture->GetNativeHandle());
        }
    }

    // 8. Bind storage buffers. Look up the binding index from shader reflection.
    for (u64 i = 0; i < m_buffers.GetSize(); ++i)
    {
        const BufferBinding& bb = m_buffers[i];
        if (bb.buffer == nullptr || !bb.buffer->IsValid())
        {
            continue;
        }
        const ShaderParameter* param = m_shader->FindParameter(bb.name);
        if (param != nullptr && param->category == ParameterCategory::StorageBuffer)
        {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(param->binding_index), bb.buffer->GetNativeHandle());
        }
    }
}
