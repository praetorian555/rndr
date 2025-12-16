#pragma once

#include "rndr/math.hpp"
#include "rndr/render-api.hpp"

namespace Rndr
{

struct RendererBaseDesc
{
    Opal::Ref<GraphicsContext> graphics_context;
    Opal::Ref<SwapChain> swap_chain;
};

/**
 * Base class for all renderers. This class is used to abstract the renderer from the application.
 */
class RendererBase
{
public:
    RendererBase(const Opal::StringUtf8& name, const RendererBaseDesc& desc);
    virtual ~RendererBase() = default;

    virtual bool Render(f32 delta_seconds, CommandList& command_list) = 0;

    [[nodiscard]] const Opal::StringUtf8& GetName() const { return m_name; }

protected:
    Opal::StringUtf8 m_name;
    RendererBaseDesc m_desc;
};

/**
 * Renderer that clears the screen.
 */
class ClearRenderer : public RendererBase
{
public:
    ClearRenderer(const Opal::StringUtf8& name, const RendererBaseDesc& desc, const Vector4f& color, float depth = 1.0f, i32 stencil = 0);

    bool Render(f32 delta_seconds, CommandList& command_list) override;

protected:
    Vector4f m_color;
    float m_depth;
    i32 m_stencil;
};

/**
 * Renderer that presents the swap chain to the screen.
 */
class PresentRenderer : public RendererBase
{
public:
    PresentRenderer(const Opal::StringUtf8& name, const RendererBaseDesc& desc);

    bool Render(f32 delta_seconds, CommandList& command_list) override;
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
    bool AddRendererBefore(RendererBase* renderer, const Opal::StringUtf8& before_name);

    /**
     * Adds a renderer to the manager after the renderer with the given name.
     * @param renderer Renderer to add.
     * @param after_name Name of the renderer after which the new renderer should be added.
     * @return True if the renderer was added successfully.
     */
    bool AddRendererAfter(RendererBase* renderer, const Opal::StringUtf8& after_name);

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
    bool RemoveRenderer(const Opal::StringUtf8& name);

    /**
     * Gets the index of the renderer with the given name.
     * @param name Name of the renderer.
     * @return Index of the renderer or -1 if the renderer was not found.
     */
    [[nodiscard]] i32 GetRendererIndex(const Opal::StringUtf8& name);

    /**
     * Renders all renderers in the manager.
     * @return True if all renderers were rendered successfully.
     */
    bool Render(f32 delta_seconds, CommandList& command_list);

private:
    Opal::DynamicArray<RendererBase*> m_renderers;
};

}  // namespace Rndr