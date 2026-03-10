#include "rndr/canvas/display.hpp"

#include "glad/glad_wgl.h"

#include "rndr/exception.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/trace.hpp"

Rndr::Canvas::Display::Display(const Context& context, Opal::Ref<GenericWindow> window, bool vsync_enabled)
    : m_vsync_enabled(vsync_enabled), m_window(std::move(window))
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Display::Display");
    RNDR_UNUSED(context);

    if (m_window == nullptr)
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Window handle is null!");
    }

#if RNDR_WINDOWS
    m_device_context = GetDC(RNDR_TO_HWND(m_window->GetNativeHandle()));
    if (m_device_context == k_invalid_device_context_handle)
    {
        throw GraphicsAPIException(0, "Failed to get device context from window!");
    }

    // Query initial window dimensions.
    RECT client_rect = {};
    GetClientRect(RNDR_TO_HWND(m_window->GetNativeHandle()), &client_rect);
    m_width = static_cast<i32>(client_rect.right - client_rect.left);
    m_height = static_cast<i32>(client_rect.bottom - client_rect.top);

    // Set vsync.
    wglSwapIntervalEXT(m_vsync_enabled ? 1 : 0);
#endif
}

Rndr::Canvas::Display::~Display()
{
    Destroy();
}

Rndr::Canvas::Display::Display(Display&& other) noexcept
    : m_vsync_enabled(other.m_vsync_enabled),
      m_window(std::move(other.m_window)),
      m_device_context(other.m_device_context),
      m_width(other.m_width),
      m_height(other.m_height)
{
    other.m_window = nullptr;
    other.m_device_context = k_invalid_device_context_handle;
    other.m_width = 0;
    other.m_height = 0;
}

Rndr::Canvas::Display& Rndr::Canvas::Display::operator=(Display&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_vsync_enabled = other.m_vsync_enabled;
        m_window = std::move(other.m_window);
        m_device_context = other.m_device_context;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_window = nullptr;
        other.m_device_context = k_invalid_device_context_handle;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

void Rndr::Canvas::Display::Destroy()
{
    if (m_device_context != k_invalid_device_context_handle)
    {
#if RNDR_WINDOWS
        ReleaseDC(RNDR_TO_HWND(m_window->GetNativeHandle()), m_device_context);
#endif
        m_device_context = k_invalid_device_context_handle;
        m_window = nullptr;
        m_width = 0;
        m_height = 0;
    }
}

void Rndr::Canvas::Display::Present()
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Display::Present");

#if RNDR_WINDOWS
    if (m_device_context != k_invalid_device_context_handle)
    {
        SwapBuffers(m_device_context);
    }
#endif
}

void Rndr::Canvas::Display::SetVsync(bool enabled)
{
    m_vsync_enabled = enabled;
#if RNDR_WINDOWS
    wglSwapIntervalEXT(enabled ? 1 : 0);
#endif
}

void Rndr::Canvas::Display::Resize(i32 width, i32 height)
{
    m_width = width;
    m_height = height;
}

Rndr::i32 Rndr::Canvas::Display::GetWidth() const
{
    return m_width;
}

Rndr::i32 Rndr::Canvas::Display::GetHeight() const
{
    return m_height;
}

bool Rndr::Canvas::Display::IsVsyncEnabled() const
{
    return m_vsync_enabled;
}

bool Rndr::Canvas::Display::IsValid() const
{
    return m_device_context != k_invalid_device_context_handle;
}
