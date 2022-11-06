#pragma once

#include "rndr/core/allocator.h"
#include "rndr/core/base.h"
#include "rndr/core/delegate.h"
#include "rndr/core/singletons.h"
#include "rndr/core/window.h"
#include "rndr/core/log.h"

#include "rndr/render/graphicscontext.h"
#include "rndr/render/swapchain.h"

namespace rndr
{

class InputSystem;
class InputContext;

using TickDelegate = MultiDelegate<real /* DeltaSeconds */>;

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
    RndrContext(const RndrContextProperties& Props = RndrContextProperties{});
    ~RndrContext();

    Allocator* GetAllocator();
    Logger* GetLogger();

    InputSystem* GetInputSystem();
    InputContext* GetInputContext();

    Window* CreateWin(int Width, int Height, const WindowProperties& Props = WindowProperties{});
    GraphicsContext* CreateGraphicsContext(const GraphicsContextProperties& Props = GraphicsContextProperties{});

    // Run render loop.
    void Run();

    template <typename T, typename... Args>
    T* Create(const char* Tag, const char* File, int Line, Args&&... Arguments)
    {
        void* Memory = m_Allocator->Allocate(sizeof(T), Tag, File, Line);
        if (!Memory)
        {
            return nullptr;
        }
        return new (Memory) T{std::forward<Args>(Arguments)...};
    }

    template <typename T>
    T* CreateArray(int Count, const char* Tag, const char* File, int Line)
    {
        if (Count <= 0)
        {
            return nullptr;
        }
        void* Memory = m_Allocator->Allocate(Count * sizeof(T), Tag, File, Line);
        if (!Memory)
        {
            return nullptr;
        }
        return new (Memory) T[Count]{};
    }

    template <typename T>
    void Destroy(T* Ptr)
    {
        if (!Ptr)
        {
            return;
        }
        Ptr->~T();
        m_Allocator->Deallocate(Ptr);
    }

    template <typename T>
    void DestroyArray(T* Ptr, int Count)
    {
        if (!Ptr)
        {
            return;
        }
        if (Count > 0)
        {
            T* It = Ptr;
            for (int i = 0; i < Count; i++)
            {
                It->~T();
                It++;
            }
        }
        m_Allocator->Deallocate(Ptr);
    }

public:
    TickDelegate OnTickDelegate;

private:
    RndrContextProperties m_Props;
    bool bInitialized = false;

    Singletons m_Singletons;
    Allocator* m_Allocator = nullptr;
    Logger* m_Logger = nullptr;
};

extern RndrContext* GRndrContext;

}  // namespace rndr
