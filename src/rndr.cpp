#include "rndr/rndr.h"

#include "opal/paths.h"

#include "rndr/input.h"
#include "rndr/platform/windows-header.h"

namespace
{
Rndr::RndrDesc g_desc;
bool g_is_initialized = false;
}  // namespace

bool Rndr::Init(const RndrDesc& desc)
{
    if (g_is_initialized)
    {
        RNDR_LOG_WARNING("Rndr library is already initialized!");
        return true;
    }

    g_desc = desc;
    Rndr::SetLogger(g_desc.user_logger.GetPtr());
    if (g_desc.enable_input_system && !InputSystem::Init())
    {
        RNDR_LOG_ERROR("Failed to initialize the input system!");
        return false;
    }

#if RNDR_TRACER
    if (g_desc.enable_cpu_tracer && !CpuTracer::Init())
    {
        RNDR_LOG_ERROR("Failed to initialize the CPU tracer!");
        return false;
    }
#endif  // RNDR_TRACER

    g_is_initialized = true;
    return true;
}

bool Rndr::Destroy()
{
    if (!g_is_initialized)
    {
        RNDR_LOG_WARNING("Rndr library is not initialized!");
        return true;
    }
#if RNDR_TRACER
    if (g_desc.enable_cpu_tracer && !CpuTracer::Destroy())
    {
        RNDR_LOG_ERROR("Failed to destroy the CPU tracer!");
        return false;
    }
#endif  // RNDR_TRACER
    if (g_desc.enable_input_system && !InputSystem::Destroy())
    {
        RNDR_LOG_ERROR("Failed to destroy the input system!");
        return false;
    }
    g_is_initialized = false;
    return true;
}

void Rndr::WaitForDebuggerToAttach()
{
    while (IsDebuggerPresent() == 0)
    {
        Sleep(100);
    }
    RNDR_DEBUG_BREAK;
}
