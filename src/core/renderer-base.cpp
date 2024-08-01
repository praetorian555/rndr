#include "rndr/core/renderer-base.h"

#include "rndr/core/trace.h"

Rndr::RendererBase::RendererBase(const Opal::StringUtf8& name, const RendererBaseDesc& desc) : m_name(name), m_desc(desc) {}

Rndr::ClearRenderer::ClearRenderer(const Opal::StringUtf8& name,
                                   const RendererBaseDesc& desc,
                                   const Vector4f& color,
                                   f32 depth,
                                   i32 stencil)
    : RendererBase(name, desc), m_color(color), m_depth(depth), m_stencil(stencil)
{
}

bool Rndr::ClearRenderer::Render()
{
    RNDR_CPU_EVENT_SCOPED("ClearRenderer::Render");
    return m_desc.graphics_context->ClearAll(m_color, m_depth, m_stencil);
}

Rndr::PresentRenderer::PresentRenderer(const Opal::StringUtf8& name, const Rndr::RendererBaseDesc& desc) : RendererBase(name, desc) {}

bool Rndr::PresentRenderer::Render()
{
    RNDR_CPU_EVENT_SCOPED("PresentRenderer::Render");
    return m_desc.graphics_context->Present(m_desc.swap_chain);
}

bool Rndr::RendererManager::AddRenderer(Rndr::RendererBase* renderer)
{
    m_renderers.PushBack(renderer);
    return true;
}

bool Rndr::RendererManager::AddRendererBefore(Rndr::RendererBase* renderer, const Opal::StringUtf8& before_name)
{
    for (auto it = m_renderers.ConstBegin(); it != m_renderers.ConstEnd(); ++it)
    {
        if ((*it)->GetName() == before_name)
        {
            m_renderers.Insert(it, renderer);
            return true;
        }
    }
    return false;
}

bool Rndr::RendererManager::AddRendererAfter(Rndr::RendererBase* renderer, const Opal::StringUtf8& after_name)
{
    for (auto it = m_renderers.ConstBegin(); it != m_renderers.ConstEnd(); ++it)
    {
        if ((*it)->GetName() == after_name)
        {
            m_renderers.Insert(it + 1, renderer);
            return true;
        }
    }
    return false;
}

bool Rndr::RendererManager::RemoveRenderer(Rndr::RendererBase* renderer)
{
    for (auto it = m_renderers.begin(); it != m_renderers.end(); ++it)
    {
        if (*it == renderer)
        {
            m_renderers.Erase(it);
            return true;
        }
    }
    return false;
}

bool Rndr::RendererManager::RemoveRenderer(const Opal::StringUtf8& name)
{
    for (auto it = m_renderers.begin(); it != m_renderers.end(); ++it)
    {
        if ((*it)->GetName() == name)
        {
            m_renderers.Erase(it);
            return true;
        }
    }
    return false;
}

Rndr::i32 Rndr::RendererManager::GetRendererIndex(const Opal::StringUtf8& name)
{
    for (size_t i = 0; i < m_renderers.GetSize(); ++i)
    {
        if (m_renderers[i]->GetName() == name)
        {
            return static_cast<i32>(i);
        }
    }
    return -1;
}

bool Rndr::RendererManager::Render()
{
    for (auto& renderer : m_renderers)
    {
        if (!renderer->Render())
        {
            return false;
        }
    }
    return true;
}
