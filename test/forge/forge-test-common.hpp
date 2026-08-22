#pragma once

#include <cstdlib>

#include "opal/container/string.h"
#include "opal/logging.h"

#include "rndr/forge/debug.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/types.hpp"

/**
 * What the two Forge test files share: the validation report, the assertion built on it, and the
 * environment flag that turns a skipped run into a failing one. Everything else about a fixture differs
 * between them - the headless file builds a device and nothing more, the windowed one an application, a
 * window and a surface first - so the fixtures themselves stay where they are used.
 */
namespace ForgeTest
{

/**
 * Registers the log category that RNDR_LOG_* writes through, so the validation layer's report reaches the
 * console of a test run.
 *
 * Nothing else in a test binary does it. `Rndr::Application` registers the category and is the only thing
 * that does, so the messages were printed only by the runs that happened to build one first - a full run,
 * or one of the windowed suites - and a run of `[forge]` on its own collected the same messages and printed
 * none of them. The assertions never depended on this, since the fixture reads what the debug callback
 * collected rather than what was logged, but a person reading a failure did.
 *
 * At `Warning`, not the `Verbose` an application asks for: what a test run wants is what the layer objects
 * to, and the rest is the loader naming every ICD and layer file it opens, once per fixture. This runs
 * before main and `Rndr::Application` leaves a category that is already registered alone, so the level here
 * is the one a full run gets too.
 */
/**
 * What every context in the two test files is built with: the messages collected so the assertions can read
 * them, and only the ones about this code written to the log.
 *
 * The loader reports a layer manifest that some other application left on the machine at error severity,
 * once for every instance, and a suite that builds one per case turns that into hundreds of lines around
 * the two that matter. Those are General messages, so naming the other two types leaves them out of the
 * log. Nothing is left out of the counts or the storage, which is where the assertions look.
 */
inline Rndr::Forge::GraphicsContextDesc TestContextDesc()
{
    return {.collect_debug_messages = true,
            .logged_message_types =
                Rndr::Forge::DebugMessageTypeBits::Validation | Rndr::Forge::DebugMessageTypeBits::Performance};
}

inline bool RegisterLogCategory()
{
    Opal::Logger& logger = Opal::GetLogger();
    if (!logger.IsCategoryRegistered("Rndr"))
    {
        logger.RegisterCategory("Rndr", Opal::LogLevel::Warning);
    }
    return true;
}

inline const bool g_log_category_registered = RegisterLogCategory();

/**
 * What the validation layer reported, as text, so a failure names the problem instead of only counting it.
 * Validation messages only: the loader reports a layer manifest that some other application left behind at
 * error severity, which says nothing about this code. Always empty in a build without RNDR_FORGE_VALIDATION,
 * where there is no layer to report anything.
 */
inline Opal::StringUtf8 CollectValidationErrors(const Rndr::Forge::GraphicsContext& context)
{
    using namespace Rndr;
    Opal::StringUtf8 report;
    for (const Forge::DebugMessage& message : context.GetDebugMessages())
    {
        if (message.severity == Forge::DebugMessageSeverity::Error && !!(message.types & Forge::DebugMessageTypeBits::Validation))
        {
            report += message.text;
            if (!message.objects.IsEmpty())
            {
                report += Opal::StringUtf8(" [objects: ");
                report += message.objects;
                report += Opal::StringUtf8("]");
            }
            report += Opal::StringUtf8("\n");
        }
    }
    return report;
}

inline Rndr::u32 CountValidationErrors(const Rndr::Forge::GraphicsContext& context)
{
    using namespace Rndr;
    return context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation);
}

/**
 * Whether an environment variable is set to anything but "0". _dupenv_s rather than getenv, which MSVC
 * deprecates and this build turns into an error.
 */
inline bool IsEnvironmentFlagSet(const char* name)
{
    bool is_set = false;
#if defined(_MSC_VER)
    char* value = nullptr;
    size_t size = 0;
    if (_dupenv_s(&value, &size, name) == 0 && value != nullptr)
    {
        is_set = value[0] != '0';
        free(value);
    }
#else
    const char* value = std::getenv(name);
    is_set = value != nullptr && value[0] != '0';
#endif
    return is_set;
}

}  // namespace ForgeTest

/** Fails the test with the text of the messages when the validation layer reported an error. */
#define REQUIRE_NO_VALIDATION_ERROR(fixture)                                        \
    do                                                                              \
    {                                                                               \
        const Opal::StringUtf8 validation_errors = (fixture).GetValidationErrors(); \
        INFO(*validation_errors);                                                   \
        REQUIRE((fixture).GetValidationErrorCount() == 0);                          \
    } while (false)

/**
 * The same check, with the device released first, so that an object nobody destroyed is named rather than
 * outliving the last assertion. Everything the case built on the device has to be gone before this.
 */
#define REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture) \
    do                                                   \
    {                                                    \
        (fixture).DestroyDevice();                       \
        REQUIRE_NO_VALIDATION_ERROR(fixture);            \
    } while (false)
