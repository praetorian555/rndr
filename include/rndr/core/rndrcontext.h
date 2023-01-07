#pragma once

#include "rndr/core/allocator.h"
#include "rndr/core/base.h"
#include "rndr/core/delegate.h"
#include "rndr/core/log.h"
#include "rndr/core/window.h"

#include "rndr/render/graphicscontext.h"
#include "rndr/render/swapchain.h"

namespace rndr
{

template <typename T>
class ScopePtr;

class InputSystem;
struct InputContext;

struct RndrContextProperties
{
    Allocator* UserAllocator = nullptr;
    Logger* UserLogger = nullptr;
};

/**
 * Main API of the rndr library.
 */
class RndrContext
{
public:
    explicit RndrContext(const RndrContextProperties& Props = RndrContextProperties{});
    ~RndrContext();

    RndrContext(const RndrContext& Other) = delete;
    RndrContext& operator=(const RndrContext& Other) = delete;

    RndrContext(RndrContext&& Other) = delete;
    RndrContext& operator=(RndrContext&& Other) = delete;

    Allocator* GetAllocator();
    Logger* GetLogger();

    InputSystem* GetInputSystem();
    InputContext* GetInputContext();

    [[nodiscard]] ScopePtr<Window>
    CreateWin(int Width, int Height, const WindowProperties& Props = WindowProperties{}) const;

    [[nodiscard]] ScopePtr<GraphicsContext> CreateGraphicsContext(
        const GraphicsContextProperties& Props = GraphicsContextProperties{}) const;

private:
    RndrContextProperties m_Props;
    bool m_IsInitialized = false;

    Allocator* m_Allocator = nullptr;
    Logger* m_Logger = nullptr;

    class InputSystem* m_InputSystem = nullptr;
};

extern RndrContext* GRndrContext;

}  // namespace rndr
