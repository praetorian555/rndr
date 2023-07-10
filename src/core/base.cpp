#include "rndr/core/base.h"

#include <cstdarg>

#include "rndr/core/input.h"
#include "rndr/utility/cpu-tracer.h"
#include "rndr/utility/default-logger.h"

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

#if RNDR_DEFAULT_LOGGER
    if (g_desc.user_logger == nullptr)
    {
        g_desc.user_logger = RNDR_NEW(DefaultLogger);
    }
#endif // RNDR_DEFAULT_LOGGER

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
    if (g_desc.user_logger != nullptr)
    {
        RNDR_DELETE(Allocator, g_desc.user_logger);
        g_desc.user_logger = nullptr;
    }
    g_is_initialized = false;
    return true;
}

const Rndr::Logger& Rndr::GetLogger()
{
    assert(g_is_initialized);
    return *g_desc.user_logger;
}

void Rndr::Log(const std::source_location& source_location, Rndr::LogLevel log_level, const char* format, ...)
{
    if (g_desc.user_logger == nullptr)
    {
        return;
    }

    constexpr int k_message_size = 4096;
    std::array<char, k_message_size> message;
    memset(message.data(), 0, k_message_size);

    va_list args = nullptr;
    va_start(args, format);
    vsprintf_s(message.data(), k_message_size, format, args);
    va_end(args);
    g_desc.user_logger->Log(source_location, log_level, message.data());
}
