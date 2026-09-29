#pragma once

#include "opal/logging.h"

namespace Rndr
{

/**
 * The global Opal logger, with the "Rndr" category registered at Verbose on first use unless something registered it
 * first. A program that sets its own level registers the category before it calls into rndr. Not synchronized, like
 * Opal::Logger itself: the first call must not race another.
 */
Opal::Logger& GetLogger();

}  // namespace Rndr

// The format string travels inside __VA_ARGS__ rather than as a named parameter, so there is always at
// least one argument and no trailing comma for the GNU `, ##__VA_ARGS__` extension to swallow.
#define RNDR_LOG_ERROR(...) Rndr::GetLogger().Error("Rndr", __VA_ARGS__)
#define RNDR_LOG_WARNING(...) Rndr::GetLogger().Warning("Rndr", __VA_ARGS__)
#define RNDR_LOG_DEBUG(...) Rndr::GetLogger().Verbose("Rndr", __VA_ARGS__)
#define RNDR_LOG_INFO(...) Rndr::GetLogger().Info("Rndr", __VA_ARGS__)
