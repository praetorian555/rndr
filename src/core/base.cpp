#include "rndr/core/base.h"

#include <cstdarg>

#ifdef RNDR_SPDLOG
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#endif  // RNDR_SPDLOG

#include "rndr/core/input.h"
#include "rndr/utility/cpu-tracer.h"

namespace
{
struct DefaultLogger : public Rndr::Logger
{
    DefaultLogger()
    {
#ifdef RNDR_SPDLOG
        spdlog::set_level(spdlog::level::debug);
        spdlog::set_level(spdlog::level::trace);
        spdlog::set_pattern("[%H:%M:%S:%e][%P][%t][%^%l%$][%@] %v");

        constexpr int k_max_message_count = 8192;
        constexpr int k_backing_thread_count = 1;
        spdlog::init_thread_pool(k_max_message_count, k_backing_thread_count);

        logger = spdlog::create<spdlog::sinks::stdout_color_sink_st>("stdout_logger");
#endif  // RNDR_SPDLOG
    }

    ~DefaultLogger() override
    {
#ifdef RNDR_SPDLOG
        spdlog::drop(logger->name());
#endif  // RNDR_SPDLOG
    }

    void Log(const std::source_location& source_location,
             Rndr::LogLevel log_level,
             const char* message)
    {
#ifdef RNDR_SPDLOG
        const spdlog::source_loc source_info(source_location.file_name(),
                                             static_cast<int32_t>(source_location.line()),
                                             source_location.function_name());

        switch (log_level)
        {
            case Rndr::LogLevel::Error:
            {
                logger->log(source_info, spdlog::level::level_enum::err, message);
                break;
            }
            case Rndr::LogLevel::Warning:
            {
                logger->log(source_info, spdlog::level::level_enum::warn, message);
                break;
            }
            case Rndr::LogLevel::Debug:
            {
                logger->log(source_info, spdlog::level::level_enum::debug, message);
                break;
            }
            case Rndr::LogLevel::Info:
            {
                logger->log(source_info, spdlog::level::level_enum::info, message);
                break;
            }
            case Rndr::LogLevel::Trace:
            {
                logger->log(source_info, spdlog::level::level_enum::trace, message);
                break;
            }
        }
#else
        RNDR_UNUSED(source_location);
        RNDR_UNUSED(log_level);
        RNDR_UNUSED(message);
#endif  // RNDR_SPDLOG
    }

    std::shared_ptr<spdlog::logger> logger;
};

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

    if (g_desc.user_logger == nullptr)
    {
        g_desc.user_logger = RNDR_NEW(DefaultLogger);
    }

    if (g_desc.enable_input_system && !InputSystem::Init())
    {
        RNDR_LOG_ERROR("Failed to initialize the input system!");
        return false;
    }

    if (g_desc.enable_cpu_tracer && !CpuTracer::Init())
    {
        RNDR_LOG_ERROR("Failed to initialize the CPU tracer!");
        return false;
    }

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
    if (g_desc.enable_cpu_tracer && !CpuTracer::Destroy())
    {
        RNDR_LOG_ERROR("Failed to destroy the CPU tracer!");
        return false;
    }
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

void Rndr::Log(const std::source_location& source_location,
               Rndr::LogLevel log_level,
               const char* format,
               ...)
{
    assert(g_desc.user_logger);

    constexpr int k_message_size = 4096;
    std::array<char, k_message_size> message;
    memset(message.data(), 0, k_message_size);

    va_list args = nullptr;
    va_start(args, format);
    vsprintf_s(message.data(), k_message_size, format, args);
    va_end(args);
    g_desc.user_logger->Log(source_location, log_level, message.data());
}
