#include "rndr/core/renderer-base.h"

#include "rndr/utility/cpu-tracer.h"

Rndr::RendererBase::RendererBase(const Rndr::String& name, const Rndr::RendererBaseDesc& desc) : m_name(name), m_desc(desc) {}

Rndr::ClearRenderer::ClearRenderer(const Rndr::String& name,
                                   const Rndr::RendererBaseDesc& desc,
                                   const Vector4f& color,
                                   float depth,
                                   uint32_t stencil)
    : RendererBase(name, desc), m_color(color), m_depth(depth), m_stencil(stencil)
{
}

bool Rndr::ClearRenderer::Render()
{
    RNDR_TRACE_SCOPED(ClearRenderer::Render);
    return m_desc.graphics_context->ClearColorAndDepth(m_color, m_depth);
}

Rndr::PresentRenderer::PresentRenderer(const Rndr::String& name, const Rndr::RendererBaseDesc& desc) : RendererBase(name, desc) {}

bool Rndr::PresentRenderer::Render()
{
    RNDR_TRACE_SCOPED(PresentRenderer::Render);
    return m_desc.graphics_context->Present(m_desc.swap_chain);
}

bool Rndr::RendererManager::AddRenderer(Rndr::RendererBase* renderer)
{
    m_renderers.push_back(renderer);
    return true;
}

bool Rndr::RendererManager::AddRendererBefore(Rndr::RendererBase* renderer, const Rndr::String& before_name)
{
    for (auto it = m_renderers.begin(); it != m_renderers.end(); ++it)
    {
        if ((*it)->GetName() == before_name)
        {
            m_renderers.insert(it, renderer);
            return true;
        }
    }
    return false;
}

bool Rndr::RendererManager::AddRendererAfter(Rndr::RendererBase* renderer, const Rndr::String& after_name)
{
    for (auto it = m_renderers.begin(); it != m_renderers.end(); ++it)
    {
        if ((*it)->GetName() == after_name)
        {
            m_renderers.insert(it + 1, renderer);
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
            m_renderers.erase(it);
            return true;
        }
    }
    return false;
}

bool Rndr::RendererManager::RemoveRenderer(const Rndr::String& name)
{
    for (auto it = m_renderers.begin(); it != m_renderers.end(); ++it)
    {
        if ((*it)->GetName() == name)
        {
            m_renderers.erase(it);
            return true;
        }
    }
    return false;
}

int32_t Rndr::RendererManager::GetRendererIndex(const String& name)
{
    for (size_t i = 0; i < m_renderers.size(); ++i)
    {
        if (m_renderers[i]->GetName() == name)
        {
            return static_cast<int32_t>(i);
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
