#pragma once

#include "opal/source-location.h"

#include "rndr/types.h"

namespace Rndr
{

enum class LogLevel : u8
{
    Error,
    Warning,
    Debug,
    Info,
    Trace
};

/**
 * Logger interface.
 */
struct Logger
{
    virtual ~Logger() = default;

    virtual void Log(const Opal::SourceLocation& source_location, LogLevel log_level, const char* message) = 0;
};

/**
 * Default logger implementation.
 */
struct StdLogger : public Logger
{
    void Log(const Opal::SourceLocation& source_location, LogLevel log_level, const char* message) override;
};

/**
 * Checks if the logger is valid.
 * @param logger Logger to check.
 * @return True if the logger is valid.
 */
bool IsValid(const Logger& logger);

/**
 * Gets the library's logger.
 * @return Library's logger.
 */
const Logger& GetLogger();

/**
 * Set new global logger.
 * @param logger New logger.
 */
void SetLogger(Logger* logger);

/**
 * Logs a message using the user specified logger. If no logger is provided, the default logger is
 * used.
 * @param source_location Contains information about the source location of the message.
 * @param log_level Log level of the message.
 * @param format Format string for the message.
 * @param ... Arguments for the format string.
 */
void Log(const Opal::SourceLocation& source_location, Rndr::LogLevel log_level, const char* format, ...);

}  // namespace Rndr

/**
 * Helper macros to log messages.
 */
#define RNDR_LOG_ERROR(format, ...) Rndr::Log(Opal::CurrentSourceLocation(), Rndr::LogLevel::Error, format, __VA_ARGS__)
#define RNDR_LOG_WARNING(format, ...) Rndr::Log(Opal::CurrentSourceLocation(), Rndr::LogLevel::Warning, format, __VA_ARGS__)
#define RNDR_LOG_DEBUG(format, ...) Rndr::Log(Opal::CurrentSourceLocation(), Rndr::LogLevel::Debug, format, __VA_ARGS__)
#define RNDR_LOG_INFO(format, ...) Rndr::Log(Opal::CurrentSourceLocation(), Rndr::LogLevel::Info, format, __VA_ARGS__)
#define RNDR_LOG_TRACE(format, ...) Rndr::Log(Opal::CurrentSourceLocation(), Rndr::LogLevel::Trace, format, __VA_ARGS__)
