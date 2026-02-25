#include <catch2/catch2.hpp>

#include "rndr/log.hpp"

class CustomLogger : public Rndr::Logger
{
    void Log(const Opal::SourceLocation& source_location,
             Rndr::LogLevel log_level,
             const char* message)
    {
        RNDR_UNUSED(source_location);
        RNDR_UNUSED(log_level);
        RNDR_UNUSED(message);
    }
};

TEST_CASE("Init", "[init]")
{
    Opal::MallocAllocator allocator;
    Opal::PushDefaultAllocator(&allocator);
    SECTION("Default create and destroy")
    {
        auto app = Rndr::Application::Create();
        REQUIRE(app != nullptr);
    }
    SECTION("Create with custom logger")
    {
        CustomLogger custom_logger;
        Rndr::Logger* ptr = &custom_logger;
        const Rndr::ApplicationDesc desc = { .user_logger = ptr };
        auto app = Rndr::Application::Create(desc);
        REQUIRE(app != nullptr);
        REQUIRE(&app->GetLoggerChecked() == ptr);
    }
}
