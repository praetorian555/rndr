#include "rndr/rndr.h"

#include "renderer.h"

class App
{
public:
    constexpr static int MaxInstancesCount = 1000;

public:
    App(int WindowWidth, int WindowHeight)
        : m_WindowWidth(WindowWidth), m_WindowHeight(WindowHeight)
    {
        m_RndrCtx = std::make_unique<Rndr::RndrContext>();
        assert(m_RndrCtx.get());
        m_GraphicsCtx = m_RndrCtx->CreateGraphicsContext();
        assert(m_GraphicsCtx.IsValid());
        m_Window = m_RndrCtx->CreateWin(m_WindowWidth, m_WindowHeight);
        assert(m_Window.IsValid());

        Rndr::NativeWindowHandle Handle = m_Window->GetNativeWindowHandle();
        m_SwapChain = m_GraphicsCtx->CreateSwapChain(Handle, m_WindowWidth, m_WindowHeight);
        assert(m_SwapChain.IsValid());

        m_Renderer =
            std::make_unique<Renderer>(m_GraphicsCtx.Get(), MaxInstancesCount, GetScreenSize());
        assert(m_Renderer.get());

        m_Renderer->AddFont("Liberation Sans", BASIC2D_ASSET_DIR "/LiberationSans-Regular.ttf");
        m_Renderer->AddFont("Yu Mincho", BASIC2D_ASSET_DIR "/yumin.ttf");

#ifdef RNDR_IMGUi
        m_ImGui = rndr::ImGuiWrapper::Create(m_Window.GetRef(), m_GraphicsCtx.GetRef());
        assert(m_ImGui.IsValid());
#endif
    }

    ~App() = default;

    void RunLoop()
    {
        while (!m_Window->IsClosed())
        {
            m_Window->ProcessEvents();

            math::Vector4 ClearColor{55 / 255.0f, 67 / 255.0f, 90 / 255.0f, 1.0f};
            Rndr::FrameBuffer* DefaultFB = m_SwapChain->FrameBuffer.Get();
            m_GraphicsCtx->ClearColor(DefaultFB->ColorBuffers[0].Get(), ClearColor);

            TextProperties Props;
            Props.Color = Rndr::Colors::kYellow;
            m_Renderer->RenderText("Liberation Sans with shadow, 16px!", "Liberation Sans", 16,
                                   {10, 300}, Props);

            Props.Scale = 0.5f;
            m_Renderer->RenderText("Liberation Sans with shadow, 32px, 0.5 scale!",
                                   "Liberation Sans", 32, {10, 280}, Props);
            Props.Scale = 1.0f;

            Props.bShadow = false;
            m_Renderer->RenderText("Liberation Sans no shadow, 16px!", "Liberation Sans", 16,
                                   {10, 260}, Props);
            Props.bShadow = true;

            Props.Color = Rndr::Colors::kGreen;
            m_Renderer->RenderText("Liberation Sans with shadow, 24px!", "Liberation Sans", 24,
                                   {10, 200}, Props);

            Props.Color = Rndr::Colors::kWhite;
            m_Renderer->RenderText("Liberation Sans with shadow, 32px!", "Liberation Sans", 32,
                                   {10, 170}, Props);

            Props.bShadow = true;
            Props.Threshold = 0.68f;
            Props.Color = Rndr::Colors::kYellow;
            m_Renderer->RenderText("Yu Mincho with shadow, 16px!", "Yu Mincho", 16, {10, 550},
                                   Props);

            Props.bShadow = false;
            Props.Color = Rndr::Colors::kYellow;
            m_Renderer->RenderText("Yu Mincho no shadow, 24px!", "Yu Mincho", 24, {10, 500}, Props);
            Props.bShadow = true;

            Props.Color = Rndr::Colors::kYellow;
            m_Renderer->RenderText("Yu Mincho with shadow, 32px!", "Yu Mincho", 32, {10, 450},
                                   Props);

            m_Renderer->Present(m_SwapChain->FrameBuffer.Get());

#ifdef RNDR_IMGUI
            m_ImGui->StartFrame();
            m_ImGui->EndFrame();
#endif

            m_GraphicsCtx->Present(m_SwapChain.Get(), true);
        }
    }

    Rndr::GraphicsContext* GetGraphicsContext() { return m_GraphicsCtx.Get(); }
    math::Vector2 GetScreenSize() const
    {
        return math::Vector2{static_cast<float>(m_WindowWidth), static_cast<float>(m_WindowHeight)};
    }

private:
    std::unique_ptr<Rndr::RndrContext> m_RndrCtx;

    Rndr::ScopePtr<Rndr::GraphicsContext> m_GraphicsCtx;
    Rndr::ScopePtr<Rndr::Window> m_Window;
    Rndr::ScopePtr<Rndr::SwapChain> m_SwapChain;
#ifdef RNDR_IMGUI
    Rndr::ScopePtr<Rndr::ImGuiWrapper> m_ImGui;
#endif

    std::unique_ptr<Renderer> m_Renderer;

    int m_WindowWidth = 800;
    int m_WindowHeight = 600;
};

int main()
{
    AtlasPacker::Test();

    App MainApp(800, 600);
    MainApp.RunLoop();
    return 0;
}