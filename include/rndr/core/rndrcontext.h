#pragma once

#include "rndr/core/allocator.h"
#include "rndr/core/base.h"
#include "rndr/core/delegate.h"
#include "rndr/core/singletons.h"
#include "rndr/core/window.h"

#include "rndr/render/graphicscontext.h"
#include "rndr/render/swapchain.h"

namespace rndr
{

class InputSystem;
class InputContext;

using TickDelegate = MultiDelegate<real /* DeltaSeconds */>;

struct RndrContextProperties
{
    int WindowWidth = 1024;
    int WindowHeight = 768;

    bool bCreateWindow = false;
    WindowProperties Window;

    Allocator* UserAllocator = nullptr;
};

/**
 * Main API of the rndr library.
 */
class RndrContext
{
public:
    RndrContext(const RndrContextProperties& Props = RndrContextProperties{});
    ~RndrContext();

    Window* GetWindow();
    GraphicsContext* GetGraphicsContext();
    SwapChain* GetSwapChain();
    InputSystem* GetInputSystem();
    InputContext* GetInputContext();

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
        Ptr->~T();
        m_Allocator->Deallocate(Ptr);
    }

    template <typename T>
    void DestroyArray(T* Ptr, int Count)
    {
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
    Singletons m_Singletons;
    Window* m_Window = nullptr;
    GraphicsContext* m_GraphicsContext = nullptr;
    SwapChain* m_SwapChain = nullptr;
    Allocator* m_Allocator = nullptr;
};

}  // namespace rndr

#define RNDR_NEW(RndrContext, Type, Tag, ...) RndrContext->Create<Type>(Tag, __FILE__, __LINE__, __VA_ARGS__)
#define RNDR_NEW_ARRAY(RndrContext, Type, Count, Tag, ...) RndrContext->CreateArray<Type>(Count, Tag, __FILE__, __LINE__)
#define RNDR_DELETE(RndrContext, Type, Ptr) RndrContext->Destroy<Type>(Ptr)
#define RNDR_DELETE_ARRAY(RndrContext, Type, Ptr, Count) RndrContext->DestroyArray<Type>(Ptr, Count)
