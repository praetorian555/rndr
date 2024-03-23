#include "rndr/core/platform/opengl-swap-chain.h"

#include <glad/glad_wgl.h>

Rndr::SwapChain::SwapChain(const Rndr::GraphicsContext& graphics_context, const Rndr::SwapChainDesc& desc) : m_desc(desc)
{
    RNDR_UNUSED(graphics_context);
    wglSwapIntervalEXT(desc.enable_vsync ? 1 : 0);
}

const Rndr::SwapChainDesc& Rndr::SwapChain::GetDesc() const
{
    return m_desc;
}

bool Rndr::SwapChain::IsValid() const
{
    return true;
}

bool Rndr::SwapChain::SetSize(int32_t width, int32_t height)
{
    if (width <= 0 || height <= 0)
    {
        RNDR_LOG_ERROR("Invalid swap chain size!");
        return false;
    }
    m_desc.width = width;
    m_desc.height = height;
    return true;
}

bool Rndr::SwapChain::SetVerticalSync(bool vertical_sync)
{
    m_desc.enable_vsync = vertical_sync;
    wglSwapIntervalEXT(vertical_sync ? 1 : 0);
    return true;
}
