#include "rndr/utility/default-logger.h"

#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

Rndr::DefaultLogger::DefaultLogger()
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("[%H:%M:%S:%e][%P][%t][%^%l%$][%@] %v");

    constexpr int k_max_message_count = 8192;
    constexpr int k_backing_thread_count = 1;
    spdlog::init_thread_pool(k_max_message_count, k_backing_thread_count);

    m_logger = spdlog::create<spdlog::sinks::stdout_color_sink_st>("stdout_logger");
}

Rndr::DefaultLogger::~DefaultLogger()
{
    spdlog::drop(m_logger->name());
}

void Rndr::DefaultLogger::Log(const std::source_location& source_location, Rndr::LogLevel log_level, const char* message)
{
    const spdlog::source_loc source_info(source_location.file_name(),
                                         static_cast<int32_t>(source_location.line()),
                                         source_location.function_name());

    switch (log_level)
    {
        case Rndr::LogLevel::Error:
        {
            m_logger->log(source_info, spdlog::level::level_enum::err, message);
            break;
        }
        case Rndr::LogLevel::Warning:
        {
            m_logger->log(source_info, spdlog::level::level_enum::warn, message);
            break;
        }
        case Rndr::LogLevel::Debug:
        {
            m_logger->log(source_info, spdlog::level::level_enum::debug, message);
            break;
        }
        case Rndr::LogLevel::Info:
        {
            m_logger->log(source_info, spdlog::level::level_enum::info, message);
            break;
        }
        case Rndr::LogLevel::Trace:
        {
            m_logger->log(source_info, spdlog::level::level_enum::trace, message);
            break;
        }
    }
}
