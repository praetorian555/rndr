#include "rndr/rndr.h"

#include <chrono>

#include "imgui.h"

#include "renderer3d.h"

class App
{
public:
    constexpr static int kMaxVertexCount = 10'000;
    constexpr static int kMaxFaceCount = 2'000;
    constexpr static int kMaxInstanceCount = 100;

public:
    App(int WindowWidth, int WindowHeight)
        : m_WindowWidth(WindowWidth), m_WindowHeight(WindowHeight)
    {
        m_RndrCtx = std::make_unique<rndr::RndrContext>();
        assert(m_RndrCtx.get());
        m_GraphicsCtx = m_RndrCtx->CreateGraphicsContext();
        assert(m_GraphicsCtx.IsValid());
        m_Window = m_RndrCtx->CreateWin(m_WindowWidth, m_WindowHeight);
        assert(m_Window.IsValid());

        rndr::InputContext* DefaultInputContext = m_RndrCtx->GetInputContext();
        DefaultInputContext->CreateMapping(
            "InfCursor",
            [this](rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real Value)
            {
                RNDR_UNUSED(Primitive);
                RNDR_UNUSED(Value);
                static int s_ModeCounter = 0;
                if (rndr::InputTrigger::ButtonUp == Trigger)
                {
                    s_ModeCounter++;
                    s_ModeCounter %= 3;
                    m_CursorMode = static_cast<rndr::CursorMode>(s_ModeCounter);
                    m_Window->SetCursorMode(m_CursorMode);
                }
            });
        DefaultInputContext->AddBinding("InfCursor", rndr::InputPrimitive::Keyboard_Q,
                                        rndr::InputTrigger::ButtonUp);

        rndr::NativeWindowHandle Handle = m_Window->GetNativeWindowHandle();
        const rndr::SwapChainProperties SwapChainProps{.UseDepthStencil = true};
        m_SwapChain =
            m_GraphicsCtx->CreateSwapChain(Handle, m_WindowWidth, m_WindowHeight, SwapChainProps);
        assert(m_SwapChain.IsValid());

        // TODO(Marko): Handle window resizing

        m_FlyCamera = rndr::CreateScoped<rndr::FlyCamera>("FlyCamera", DefaultInputContext,
                                                          m_WindowWidth, m_WindowHeight);

        m_Renderer = std::make_unique<Renderer>(m_GraphicsCtx.Get(), kMaxVertexCount, kMaxFaceCount,
                                                kMaxInstanceCount, m_WindowWidth, m_WindowHeight);
        m_Renderer->SetRenderTarget(m_SwapChain->FrameBuffer.GetRef());
        m_Renderer->SetProjectionCamera(m_FlyCamera.Get());

#ifdef RNDR_IMGUI
        m_ImGui = rndr::ImGuiWrapper::Create(m_Window.GetRef(), m_GraphicsCtx.GetRef(),
                                             {.DisplayDemoWindow = false});
        assert(m_ImGui.IsValid());
#endif

#ifdef RNDR_ASSIMP
        rndr::ModelLoader Loader;
        m_SphereModel = Loader.Load(SCENE3D_ASSET_DIR "/sphere.dae");
#endif
    }

    ~App() = default;

    // TODO(Marko): Move to a utility/time.h
    static int64_t GetTimestamp()
    {
        const auto Timestamp = std::chrono::high_resolution_clock::now();
        const int64_t ResultInt =
            std::chrono::duration_cast<std::chrono::microseconds>(Timestamp.time_since_epoch())
                .count();

        return ResultInt;
    }

    void RunLoop()
    {
        int64_t StartSeconds = 0;
        int64_t EndSeconds = 0;
        float FrameDuration = 0.0f;
        while (!m_Window->IsClosed())
        {
            StartSeconds = GetTimestamp();

            m_Window->ProcessEvents();
            m_RndrCtx->GetInputSystem()->Update(FrameDuration);
            m_FlyCamera->Update(FrameDuration);

            math::Vector4 ClearColor{55 / 255.0f, 67 / 255.0f, 90 / 255.0f, 1.0f};
            rndr::FrameBuffer* DefaultFB = m_SwapChain->FrameBuffer.Get();
            m_GraphicsCtx->ClearColor(DefaultFB->ColorBuffers[0].Get(), ClearColor);
            m_GraphicsCtx->ClearDepth(DefaultFB->DepthStencilBuffer.Get(), 1.0f);

#ifdef RNDR_ASSIMP
            math::Transform ObjectToWorld =
                math::Translate({0.0f, 0.0f, m_ObjectDepth}) * math::RotateX(m_Orientation);
            m_Renderer->RenderModel(m_SphereModel.GetRef(),
                                    rndr::Span<math::Transform>{&ObjectToWorld});
#endif

#ifdef RNDR_IMGUI
            m_ImGui->StartFrame();

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(300, 200));

            // Increase transparency of the next imgui window.
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.7f);

            ImGui::Begin("Menu");
            ImGui::Text("FPS: %.6f", 1.0f / FrameDuration);
            ImGui::Text("Cycle through cursor modes with Q");
            switch (m_CursorMode)
            {
                case rndr::CursorMode::Normal:
                {
                    ImGui::Text("CursorMode: Normal");
                    break;
                }
                case rndr::CursorMode::Hidden:
                {
                    ImGui::Text("CursorMode: Hidden");
                    break;
                }
                case rndr::CursorMode::Infinite:
                {
                    ImGui::Text("CursorMode: Infinite");
                    break;
                }
            }
            ImGui::SliderFloat("Depth", &m_ObjectDepth, 2.0f, 100.0f);
            ImGui::SliderFloat("Orientation", &m_Orientation, -90.0f, 90.0f);
            ImGui::SliderFloat("Shininess", &m_Shininess, 2, 256);

            ImGui::PopStyleVar();
            ImGui::End();

            m_ImGui->EndFrame();

            m_Renderer->SetShininess(m_Shininess);
#endif

            m_GraphicsCtx->Present(m_SwapChain.Get(), false);

            EndSeconds = GetTimestamp();
            FrameDuration = static_cast<float>(EndSeconds - StartSeconds) / 1000000.0f;
            assert(FrameDuration > 0.0f);
        }
    }

    rndr::GraphicsContext* GetGraphicsContext() { return m_GraphicsCtx.Get(); }
    [[nodiscard]] math::Vector2 GetScreenSize() const
    {
        return math::Vector2{static_cast<float>(m_WindowWidth), static_cast<float>(m_WindowHeight)};
    }

private:
    std::unique_ptr<rndr::RndrContext> m_RndrCtx;

    rndr::ScopePtr<rndr::GraphicsContext> m_GraphicsCtx;
    rndr::ScopePtr<rndr::Window> m_Window;
    rndr::ScopePtr<rndr::SwapChain> m_SwapChain;

    std::unique_ptr<Renderer> m_Renderer;

#ifdef RNDR_IMGUI
    rndr::ScopePtr<rndr::ImGuiWrapper> m_ImGui;
#endif

#ifdef RNDR_ASSIMP
    rndr::ScopePtr<rndr::Model> m_SphereModel;
#endif

    rndr::ScopePtr<rndr::FlyCamera> m_FlyCamera;

    int m_WindowWidth = 800;
    int m_WindowHeight = 600;

    float m_ObjectDepth = 5.0f;
    float m_Orientation = 0.0f;
    float m_Shininess = 8.0f;
    rndr::CursorMode m_CursorMode = rndr::CursorMode::Normal;
};

int main()
{
    App MainApp(800, 600);
    MainApp.RunLoop();
    return 0;
}