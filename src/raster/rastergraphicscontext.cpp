#include "rndr/render/raster/rastergraphicscontext.h"

#if defined RNDR_RASTER

#include "rndr/core/window.h"

#include "rndr/render/raster/rasterframebuffer.h"
#include "rndr/render/raster/rasterimage.h"
#include "rndr/render/raster/rasterizer.h"

#if defined RNDR_WINDOWS
#include <Windows.h>
#endif  // RNDR_WINDOWS

rndr::GraphicsContext::GraphicsContext(Window* Window, const GraphicsContextProperties& Props) : m_Window(Window), m_Props(Props)
{
    assert(m_Props.WindowWidth > 0 && m_Props.WindowHeight > 0);

    m_WindowFrameBuffer = std::make_unique<FrameBuffer>(this, m_Props.WindowWidth, m_Props.WindowHeight, m_Props.FrameBuffer);
    m_Rasterizer = std::make_unique<Rasterizer>();

    WindowDelegates::OnResize.Add(RNDR_BIND_THREE_PARAM(this, &GraphicsContext::WindowResize));
}

rndr::FrameBuffer* rndr::GraphicsContext::GetWindowFrameBuffer()
{
    return m_WindowFrameBuffer.get();
}

void rndr::GraphicsContext::ClearColor(FrameBuffer* FrameBuffer, const Vector4r& Color, int Index)
{
    FrameBuffer = FrameBuffer == nullptr ? m_WindowFrameBuffer.get() : FrameBuffer;
    Image* Image = FrameBuffer->GetColorBuffer(Index);
    if (Image)
    {
        Image->Clear(Color);
    }
}

void rndr::GraphicsContext::ClearDepth(FrameBuffer* FrameBuffer, real Depth)
{
    FrameBuffer = FrameBuffer == nullptr ? m_WindowFrameBuffer.get() : FrameBuffer;
    Image* Image = FrameBuffer->GetDepthBuffer();
    if (Image)
    {
        Image->Clear(Depth);
    }
}

void rndr::GraphicsContext::ClearStencil(FrameBuffer* FrameBuffer, uint8_t Value)
{
    FrameBuffer = FrameBuffer == nullptr ? m_WindowFrameBuffer.get() : FrameBuffer;
    Image* Image = FrameBuffer->GetStencilBuffer();
    if (Image)
    {
        Image->Clear(Value);
    }
}

void rndr::GraphicsContext::Render(Model* Model)
{
    m_Rasterizer->Draw(Model);
}

void rndr::GraphicsContext::Present(bool bVerticalSync)
{
#if defined RNDR_WINDOWS
    HWND WindowHandle = reinterpret_cast<HWND>(m_Window->GetNativeWindowHandle());

    Image* ColorImage = m_WindowFrameBuffer->GetColorBuffer();

    BITMAPINFO BitmapInfo = {};
    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = m_Props.WindowWidth;
    BitmapInfo.bmiHeader.biHeight = m_Props.WindowHeight;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = ColorImage->GetPixelSize() * 8;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    HDC DC = GetDC(WindowHandle);

    StretchDIBits(DC, 0, 0, m_Props.WindowWidth, m_Props.WindowHeight, 0, 0, m_Props.WindowWidth, m_Props.WindowHeight,
                  ColorImage->GetBuffer(), &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
#else
#error "Platform not supported!"
#endif
}

void rndr::GraphicsContext::WindowResize(Window* Window, int Width, int Height)
{
    if (Window != m_Window)
    {
        return;
    }

    m_Props.WindowWidth = Width;
    m_Props.WindowHeight = Height;
    m_WindowFrameBuffer->SetSize(Width, Height);
}

#endif  // RNDR_RASTER
