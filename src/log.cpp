#include "rndr/log.h"

#include <cstdarg>
#include <cstdio>

#include "opal/container/stack-array.h"
#include "opal/container/string.h"
#include "opal/paths.h"

#include "rndr/definitions.h"

namespace
{
Rndr::StdLogger g_default_logger;
Rndr::Logger* g_current_logger = &g_default_logger;
}  // namespace

void Rndr::StdLogger::Log(const Opal::SourceLocation& source_location, LogLevel log_level, const char* message)
{
    static Opal::StringUtf8 s_file_path(300, 0);
    static Opal::StringUtf8 s_file_name(300, 0);
    static const char* s_log_level_strings[] = {"ERROR", "WARNING", "DEBUG", "INFO", "TRACE"};

    // TODO: This will not work if file name or function name is using UTF-8 characters and the console is not set to UTF-8.
    s_file_path.Erase();
    s_file_path.Assign(reinterpret_cast<const c8*>(source_location.file));
    s_file_name.Erase();
    s_file_name.Assign(Opal::Paths::GetFileName(s_file_path).GetValue());

    printf("[%s][%s:%d] %s\n", s_log_level_strings[static_cast<u8>(log_level)], s_file_name.GetDataAs<c>(), source_location.line, message);
}

const Rndr::Logger& Rndr::GetLogger()
{
    return *g_current_logger;
}

void Rndr::SetLogger(Rndr::Logger* logger)
{
    if (logger == nullptr)
    {
        g_current_logger = &g_default_logger;
        return;
    }
    g_current_logger = logger;
}

void Rndr::Log(const Opal::SourceLocation& source_location, Rndr::LogLevel log_level, const char* format, ...)
{
    constexpr int k_message_size = 16 * 1024;
    Opal::StackArray<char, k_message_size> message;
    memset(message.data(), 0, k_message_size);

    va_list args = nullptr;
    va_start(args, format);
    vsprintf_s(message.data(), k_message_size, format, args);
    va_end(args);
    g_current_logger->Log(source_location, log_level, message.data());
}
