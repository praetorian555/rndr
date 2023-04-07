#include "rndr/core/base.h"

#include <cstdarg>

#ifdef RNDR_SPDLOG
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#endif  // RNDR_SPDLOG

namespace
{
// TODO(Marko): Add unique identifier to the start so that we can confirm this is indeed this type.
struct DefaultLoggerData
{
    std::shared_ptr<spdlog::logger> logger;
};

rndr::Allocator g_allocator;
rndr::Logger g_logger;
bool g_is_initialized = false;
}  // namespace

bool rndr::Init(const RndrDesc& desc)
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
        g_allocator.allocate = &rndr::DefaultAllocator::Allocate;
        g_allocator.free = &rndr::DefaultAllocator::Free;
        g_allocator.allocator_data = nullptr;
    }
    if (IsValid(desc.user_logger))
    {
        g_logger = desc.user_logger;
    }
    else
    {
        g_logger.init = &rndr::DefaultLogger::Init;
        g_logger.destroy = &rndr::DefaultLogger::Destroy;
        g_logger.log = &rndr::DefaultLogger::Log;
        g_logger.logger_data = nullptr;
        if (!g_logger.init(nullptr, &g_logger.logger_data))
        {
            return false;
        }
    }

    g_is_initialized = true;
    return true;
}

bool rndr::Destroy()
{
    if (!g_is_initialized)
    {
        RNDR_LOG_WARNING("Rndr library is not initialized!");
        return true;
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

rndr::OpaquePtr rndr::DefaultAllocator::Allocate(OpaquePtr allocator_data,
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

void rndr::DefaultAllocator::Free(OpaquePtr ptr, OpaquePtr allocator_data)
{
    RNDR_UNUSED(allocator_data);

#if RNDR_WINDOWS
    _aligned_free(ptr);
#else
#error "Platform missing default allocator implementation!"
#endif
}

bool rndr::IsValid(const rndr::Allocator& allocator)
{
    return allocator.allocate != nullptr && allocator.free != nullptr;
}

bool rndr::IsValid(const rndr::Logger& logger)
{
    return logger.log != nullptr;
}

const rndr::Allocator& rndr::GetAllocator()
{
    assert(g_is_initialized);
    return g_allocator;
}

bool rndr::SetAllocator(const rndr::Allocator& allocator)
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

const rndr::Logger& rndr::GetLogger()
{
    assert(g_is_initialized);
    return g_logger;
}

bool rndr::SetLogger(const rndr::Logger& logger)
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

bool rndr::DefaultLogger::Init(OpaquePtr init_data, OpaquePtr* logger_data)
{
    RNDR_UNUSED(init_data);

#ifdef RNDR_SPDLOG
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("[%H:%M:%S:%e][%P][%t][%^%l%$][%@] %v");

    constexpr int k_max_message_count = 8192;
    constexpr int k_backing_thread_count = 1;
    spdlog::init_thread_pool(k_max_message_count, k_backing_thread_count);

    DefaultLoggerData* default_logger_data =
        RNDR_DEFAULT_NEW(DefaultLoggerData, "Default Logger Data");
    *logger_data = default_logger_data;
    default_logger_data->logger =
        spdlog::create<spdlog::sinks::stdout_color_sink_st>("stdout_logger");
#endif  // RNDR_SPDLOG

    return true;
}

bool rndr::DefaultLogger::Destroy(OpaquePtr logger_data)
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

void rndr::DefaultLogger::Log(OpaquePtr logger_data,
                              const char* file,
                              int line,
                              const char* function,
                              rndr::LogLevel log_level,
                              const char* message)
{
#ifdef RNDR_SPDLOG
    DefaultLoggerData* data = static_cast<DefaultLoggerData*>(logger_data);
    const spdlog::source_loc source_info(file, line, function);

    switch (log_level)
    {
        case rndr::LogLevel::Error:
        {
            data->logger->log(source_info, spdlog::level::level_enum::err, message);
            break;
        }
        case rndr::LogLevel::Warning:
        {
            data->logger->log(source_info, spdlog::level::level_enum::warn, message);
            break;
        }
        case rndr::LogLevel::Debug:
        {
            data->logger->log(source_info, spdlog::level::level_enum::debug, message);
            break;
        }
        case rndr::LogLevel::Info:
        {
            data->logger->log(source_info, spdlog::level::level_enum::info, message);
            break;
        }
        case rndr::LogLevel::Trace:
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

rndr::OpaquePtr rndr::Allocate(uint64_t size, const char* tag)
{
    assert(IsValid(g_allocator));
    return g_allocator.allocate(g_allocator.allocator_data, size, tag);
}

void rndr::Free(rndr::OpaquePtr ptr)
{
    assert(IsValid(g_allocator));
    g_allocator.free(g_allocator.allocator_data, ptr);
}

void rndr::Log(const char* file,
               int line,
               const char* function,
               rndr::LogLevel log_level,
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
