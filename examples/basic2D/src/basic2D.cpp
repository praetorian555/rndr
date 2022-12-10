#include "rndr/rndr.h"

class App
{
public:
    App(int WindowWidth, int WindowHeight) : m_WindowWidth(WindowWidth), m_WindowHeight(WindowHeight)
    {
        m_RndrCtx = std::make_unique<rndr::RndrContext>();
        assert(m_RndrCtx.get());
        m_GraphicsCtx = m_RndrCtx->CreateGraphicsContext();
        assert(m_GraphicsCtx.IsValid());
        m_Window = m_RndrCtx->CreateWin(m_WindowWidth, m_WindowHeight);
        assert(m_Window.IsValid());

        rndr::NativeWindowHandle Handle = m_Window->GetNativeWindowHandle();
        m_SwapChain = m_GraphicsCtx->CreateSwapChain(Handle, m_WindowWidth, m_WindowHeight);
        assert(m_SwapChain.IsValid());
    }

    ~App() = default;

    void RunLoop()
    {
        while (!m_Window->IsClosed())
        {
            m_Window->ProcessEvents();

            math::Vector4 ClearColor{0.2f, 0.5f, 0.4f, 1.0f};
            rndr::FrameBuffer* DefaultFB = m_SwapChain->FrameBuffer;
            m_GraphicsCtx->ClearColor(DefaultFB->ColorBuffers[0], ClearColor);

            m_GraphicsCtx->Present(m_SwapChain.Get(), true);
        }
    }

private:
    std::unique_ptr<rndr::RndrContext> m_RndrCtx;

    rndr::ScopePtr<rndr::GraphicsContext> m_GraphicsCtx;
    rndr::ScopePtr<rndr::Window> m_Window;
    rndr::ScopePtr<rndr::SwapChain> m_SwapChain;

    int m_WindowWidth = 800;
    int m_WindowHeight = 600;
};

int main()
{
    App MainApp(800, 600);
    MainApp.RunLoop();
    return 0;
}