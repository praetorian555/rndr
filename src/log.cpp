#include "rndr/log.hpp"

#include <cstdarg>
#include <cstdio>

#include "opal/container/in-place-array.h"
#include "opal/container/string.h"
#include "opal/paths.h"

void Rndr::StdLogger::Log(const Opal::SourceLocation& source_location, LogLevel log_level, const char* message)
{
    static Opal::StringUtf8 s_file_path(300, 0);
    static Opal::StringUtf8 s_file_name(300, 0);
    static const char* s_log_level_strings[] = {"ERROR", "WARNING", "DEBUG", "INFO", "TRACE"};

    // TODO: This will not work if file name or function name is using UTF-8 characters and the console is not set to UTF-8.
    s_file_path.Erase();
    s_file_path.Assign(source_location.file);
    s_file_name.Erase();
    s_file_name.Assign(Opal::Paths::GetFileName(s_file_path).GetValue());

    printf("[%s][%s:%d] %s\n", s_log_level_strings[static_cast<u8>(log_level)], s_file_name.GetData(), source_location.line, message);
}

void Rndr::Log(Logger& logger, const Opal::SourceLocation& source_location, Rndr::LogLevel log_level, const char* format, ...)
{
    constexpr int k_message_size = 16 * 1024;
    Opal::InPlaceArray<char8, k_message_size> message;
    memset(message.GetData(), 0, k_message_size);

    va_list args = nullptr;
    va_start(args, format);
    vsprintf_s(message.GetData(), k_message_size, format, args);
    va_end(args);
    logger.Log(source_location, log_level, message.GetData());
}
