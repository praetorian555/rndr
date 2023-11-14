#pragma once

#include "rndr/core/math.h"
#include "rndr/render-api/render-api.h"

namespace Rndr
{

struct RendererBaseDesc
{
    Ref<GraphicsContext> graphics_context;
    Ref<SwapChain> swap_chain;
};

/**
 * Base class for all renderers. This class is used to abstract the renderer from the application.
 */
class RendererBase
{
public:
    RendererBase(const String& name, const RendererBaseDesc& desc);
    virtual ~RendererBase() = default;

    virtual bool Render() = 0;

    [[nodiscard]] const String& GetName() const { return m_name; }

protected:
    String m_name;
    RendererBaseDesc m_desc;
};

/**
 * Renderer that clears the screen.
 */
class ClearRenderer : public RendererBase
{
public:
    ClearRenderer(const String& name, const RendererBaseDesc& desc, const Vector4f& color, float depth = 1.0f, int32_t stencil = 0);

    bool Render() override;

protected:
    Vector4f m_color;
    float m_depth;
    int32_t m_stencil;
};

/**
 * Renderer that presents the swap chain to the screen.
 */
class PresentRenderer : public RendererBase
{
public:
    PresentRenderer(const String& name, const RendererBaseDesc& desc);

    bool Render() override;
};

/**
 * Manages all renderers and their dependency.
 */
class RendererManager
{
public:
    /**
     * Adds a renderer to the manager. The renderer will be added to the end of the list.
     * @param renderer Renderer to add.
     * @return True if the renderer was added successfully.
     */
    bool AddRenderer(RendererBase* renderer);

    /**
     * Adds a renderer to the manager before the renderer with the given name.
     * @param renderer Renderer to add.
     * @param before_name Name of the renderer before which the new renderer should be added.
     * @return True if the renderer was added successfully.
     */
    bool AddRendererBefore(RendererBase* renderer, const String& before_name);

    /**
     * Adds a renderer to the manager after the renderer with the given name.
     * @param renderer Renderer to add.
     * @param after_name Name of the renderer after which the new renderer should be added.
     * @return True if the renderer was added successfully.
     */
    bool AddRendererAfter(RendererBase* renderer, const String& after_name);

    /**
     * Removes a renderer from the manager.
     * @param renderer Renderer to remove.
     * @return True if the renderer was removed successfully.
     */
    bool RemoveRenderer(RendererBase* renderer);

    /**
     * Removes a renderer from the manager.
     * @param name Name of the renderer to remove.
     * @return True if the renderer was removed successfully.
     */
    bool RemoveRenderer(const String& name);

    /**
     * Gets the index of the renderer with the given name.
     * @param name Name of the renderer.
     * @return Index of the renderer or -1 if the renderer was not found.
     */
    [[nodiscard]] int32_t GetRendererIndex(const String& name);

    /**
     * Renders all renderers in the manager.
     * @return True if all renderers were rendered successfully.
     */
    bool Render();

private:
    Array<RendererBase*> m_renderers;
};

}  // namespace Rndr