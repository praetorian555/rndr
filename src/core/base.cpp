#include "rndr/core/base.h"

#include <cstdarg>

#ifdef RNDR_SPDLOG
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#endif  // RNDR_SPDLOG

#include "rndr/core/input.h"

namespace
{
// TODO(Marko): Add unique identifier to the start so that we can confirm this is indeed this type.
struct DefaultLoggerData
{
    std::shared_ptr<spdlog::logger> logger;
};

Rndr::Allocator g_allocator;
Rndr::Logger g_logger;
bool g_is_initialized = false;
}  // namespace

bool Rndr::Init(const RndrDesc& desc)
{
    if (g_is_initialized)
    {
        RNDR_LOG_WARNING("Rndr library is already initialized!");
        return true;
    }
    if (IsValid(desc.user_allocator))
    {
        g_allocator = desc.user_allocator;
    }
    else
    {
        g_allocator.init = nullptr;
        g_allocator.destroy = nullptr;
        g_allocator.allocate = &Rndr::DefaultAllocator::Allocate;
        g_allocator.free = &Rndr::DefaultAllocator::Free;
        g_allocator.allocator_data = nullptr;
    }
    if (IsValid(desc.user_logger))
    {
        g_logger = desc.user_logger;
    }
    else
    {
        g_logger.init = &Rndr::DefaultLogger::Init;
        g_logger.destroy = &Rndr::DefaultLogger::Destroy;
        g_logger.log = &Rndr::DefaultLogger::Log;
        g_logger.logger_data = nullptr;
        if (!g_logger.init(nullptr, &g_logger.logger_data))
        {
            return false;
        }
    }

    if (!InputSystem::Init())
    {
        RNDR_LOG_ERROR("Failed to initialize the input system!");
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
    if (!InputSystem::Destroy())
    {
        RNDR_LOG_ERROR("Failed to destroy the input system!");
        return false;
    }
    if (g_allocator.destroy != nullptr)
    {
        const bool result = g_allocator.destroy(g_allocator.allocator_data);
        if (!result)
        {
            RNDR_LOG_ERROR("Failed to destroy the allocator!");
            return false;
        }
    }
    if (g_logger.destroy != nullptr)
    {
        const bool result = g_logger.destroy(g_logger.logger_data);
        if (!result)
        {
            RNDR_LOG_ERROR("Failed to destroy the logger!");
            return false;
        }
    }
    g_allocator = {};
    g_logger = {};
    g_is_initialized = false;
    return true;
}

Rndr::OpaquePtr Rndr::DefaultAllocator::Allocate(OpaquePtr allocator_data,
                                                 uint64_t size,
                                                 const char* tag)
{
    RNDR_UNUSED(tag);
    RNDR_UNUSED(allocator_data);

#if RNDR_WINDOWS
    constexpr int k_k_alignment_in_bytes = 16;
    return _aligned_malloc(size, k_k_alignment_in_bytes);
#else
#error "Platform missing default allocator implementation!"
    return nullptr;
#endif
}

void Rndr::DefaultAllocator::Free(OpaquePtr ptr, OpaquePtr allocator_data)
{
    RNDR_UNUSED(allocator_data);

#if RNDR_WINDOWS
    _aligned_free(ptr);
#else
#error "Platform missing default allocator implementation!"
#endif
}

bool Rndr::IsValid(const Rndr::Allocator& allocator)
{
    return allocator.allocate != nullptr && allocator.free != nullptr;
}

bool Rndr::IsValid(const Rndr::Logger& logger)
{
    return logger.log != nullptr;
}

const Rndr::Allocator& Rndr::GetAllocator()
{
    assert(g_is_initialized);
    return g_allocator;
}

bool Rndr::SetAllocator(const Rndr::Allocator& allocator)
{
    assert(g_is_initialized);
    if (IsValid(allocator))
    {
        RNDR_LOG_ERROR("Allocator is not valid!");
        return false;
    }
    g_allocator = allocator;
    return true;
}

const Rndr::Logger& Rndr::GetLogger()
{
    assert(g_is_initialized);
    return g_logger;
}

bool Rndr::SetLogger(const Rndr::Logger& logger)
{
    assert(g_is_initialized);
    if (IsValid(logger))
    {
        RNDR_LOG_ERROR("Logger is invalid!");
        return false;
    }
    g_logger = logger;
    return true;
}

bool Rndr::DefaultLogger::Init(OpaquePtr init_data, OpaquePtr* logger_data)
{
    RNDR_UNUSED(init_data);

#ifdef RNDR_SPDLOG
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("[%H:%M:%S:%e][%P][%t][%^%l%$][%@] %v");

    constexpr int k_max_message_count = 8192;
    constexpr int k_backing_thread_count = 1;
    spdlog::init_thread_pool(k_max_message_count, k_backing_thread_count);

    DefaultLoggerData* default_logger_data = RNDR_NEW(DefaultLoggerData);
    *logger_data = default_logger_data;
    default_logger_data->logger =
        spdlog::create<spdlog::sinks::stdout_color_sink_st>("stdout_logger");
#endif  // RNDR_SPDLOG

    return true;
}

bool Rndr::DefaultLogger::Destroy(OpaquePtr logger_data)
{
    DefaultLoggerData* data = static_cast<DefaultLoggerData*>(logger_data);
#ifdef RNDR_SPDLOG
    if (data != nullptr)
    {
        spdlog::drop(data->logger->name());
        RNDR_DELETE(DefaultLoggerData, data);
    }
#endif  // RNDR_SPDLOG
    return true;
}

void Rndr::DefaultLogger::Log(OpaquePtr logger_data,
                              const char* file,
                              int line,
                              const char* function,
                              Rndr::LogLevel log_level,
                              const char* message)
{
#ifdef RNDR_SPDLOG
    DefaultLoggerData* data = static_cast<DefaultLoggerData*>(logger_data);
    const spdlog::source_loc source_info(file, line, function);

    switch (log_level)
    {
        case Rndr::LogLevel::Error:
        {
            data->logger->log(source_info, spdlog::level::level_enum::err, message);
            break;
        }
        case Rndr::LogLevel::Warning:
        {
            data->logger->log(source_info, spdlog::level::level_enum::warn, message);
            break;
        }
        case Rndr::LogLevel::Debug:
        {
            data->logger->log(source_info, spdlog::level::level_enum::debug, message);
            break;
        }
        case Rndr::LogLevel::Info:
        {
            data->logger->log(source_info, spdlog::level::level_enum::info, message);
            break;
        }
        case Rndr::LogLevel::Trace:
        {
            data->logger->log(source_info, spdlog::level::level_enum::trace, message);
            break;
        }
    }
#else
    RNDR_UNUSED(File);
    RNDR_UNUSED(Line);
    RNDR_UNUSED(Function);
    RNDR_UNUSED(LogLevel);
    RNDR_UNUSED(Message);
#endif  // RNDR_SPDLOG
}

Rndr::OpaquePtr Rndr::Allocate(uint64_t size, const char* tag)
{
    assert(IsValid(g_allocator));
    return g_allocator.allocate(g_allocator.allocator_data, size, tag);
}

void Rndr::Free(Rndr::OpaquePtr ptr)
{
    assert(IsValid(g_allocator));
    g_allocator.free(g_allocator.allocator_data, ptr);
}

void Rndr::Log(const char* file,
               int line,
               const char* function,
               Rndr::LogLevel log_level,
               const char* format,
               ...)
{
    assert(IsValid(g_logger));

    constexpr int k_message_size = 4096;
    std::array<char, k_message_size> message;
    memset(message.data(), 0, k_message_size);

    va_list args = nullptr;
    va_start(args, format);
    vsprintf_s(message.data(), k_message_size, format, args);
    va_end(args);
    g_logger.log(g_logger.logger_data, file, line, function, log_level, message.data());
}
