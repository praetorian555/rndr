#pragma once

#include "rndr/definitions.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/platform/opengl-forward-def.hpp"

namespace Rndr
{

class GraphicsContext;

/**
 * Represents a series of virtual frame buffers used by the graphics card and the OS to render to
 * the screen. Primary use is to present the rendered image to the screen.
 */
class SwapChain
{
public:
    /**
     * Default constructor. Creates an invalid swap chain.
     */
    SwapChain() = default;

    /**
     * Creates a new swap chain.
     * @param graphics_context The graphics context to create the swap chain with.
     * @param desc The description of the swap chain to create.
     */
    SwapChain(const GraphicsContext& graphics_context, const SwapChainDesc& desc);

    ~SwapChain() = default;
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;
    SwapChain(SwapChain&& other) noexcept = default;
    SwapChain& operator=(SwapChain&& other) noexcept = default;

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const SwapChainDesc& GetDesc() const;

    bool SetSize(int32_t width, int32_t height);
    bool SetVerticalSync(bool vertical_sync);

private:
    SwapChainDesc m_desc;
};

}  // namespace Rndr
