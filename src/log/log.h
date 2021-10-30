#pragma once

namespace Log
{

enum class Level
{
    Error,
    Warning,
    Debug,
    Info,
    Trace
};

struct Config
{
    bool EnableDebug = true;
    bool EnableTrace = true;
};

void Init(const Config& config = Config{});
void ShutDown();

void Log(Level logLevel, const char* format, ...);

}  // namespace Log

#define RNDR_ERROR(format, ...) Log::Log(Log::Level::Error, format, __VA_ARGS__)
#define RNDR_WARNING(format, ...) Log::Log(Log::Level::Warning, format, __VA_ARGS__)
#define RNDR_DEBUG(format, ...) Log::Log(Log::Level::Debug, format, __VA_ARGS__)
#define RNDR_INFO(format, ...) Log::Log(Log::Level::Info, format, __VA_ARGS__)
#define RNDR_TRACE(format, ...) Log::Log(Log::Level::Trace, format, __VA_ARGS__)