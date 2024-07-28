#include "rndr/core/base.h"

#include <cstdarg>

#include "opal/container/stack-array.h"
#include "opal/paths.h"

#include "rndr/core/input.h"
#include "rndr/core/platform/windows-header.h"

namespace
{
Rndr::RndrDesc g_desc;
bool g_is_initialized = false;
Rndr::StdLogger g_default_logger;
}  // namespace

void Rndr::StdLogger::Log(const Opal::SourceLocation& source_location, LogLevel log_level, const char* message)
{
    static Opal::StringUtf8 s_file_name(300, 0);
    static Opal::StringUtf8 s_function_name(300, 0);
    static const char* s_log_level_strings[] = {"ERROR", "WARNING", "DEBUG", "INFO", "TRACE"};

    // TODO: This will not work if file name or function name is using UTF-8 characters and the console is not set to UTF-8.
    s_file_name = reinterpret_cast<const c8*>(source_location.file);
    s_file_name = Opal::Paths::GetFileName(s_file_name).GetValue();

    printf("[%s][%s:%d] %s\n", s_log_level_strings[static_cast<u8>(log_level)], s_file_name.GetDataAs<c>(), source_location.line, message);
}

bool Rndr::Init(const RndrDesc& desc)
{
    if (g_is_initialized)
    {
        RNDR_LOG_WARNING("Rndr library is already initialized!");
        return true;
    }

    g_desc = desc;
    if (!g_desc.user_logger.IsValid())
    {
        g_desc.user_logger = &g_default_logger;
    }
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

const Rndr::Logger& Rndr::GetLogger()
{
    RNDR_ASSERT(g_is_initialized);
    return *g_desc.user_logger;
}

void Rndr::Log(const Opal::SourceLocation& source_location, Rndr::LogLevel log_level, const char* format, ...)
{
    constexpr int k_message_size = 4096;
    Opal::StackArray<char, k_message_size> message;
    memset(message.data(), 0, k_message_size);

    va_list args = nullptr;
    va_start(args, format);
    vsprintf_s(message.data(), k_message_size, format, args);
    va_end(args);
    g_desc.user_logger->Log(source_location, log_level, message.data());
}

void Rndr::WaitForDebuggerToAttach()
{
    while (IsDebuggerPresent() == 0)
    {
        Sleep(100);
    }
    RNDR_DEBUG_BREAK;
}
