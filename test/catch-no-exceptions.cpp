#include "catch-no-exceptions.hpp"

#include <cstdio>
#include <cstdlib>

#if defined(CATCH_CONFIG_DISABLE_EXCEPTIONS)

namespace Catch
{

/**
 * What Catch2 calls when it has to throw and cannot. Its own version terminates, which on Windows is an
 * abort dialog that a CI runner waits on until the job's ceiling; this reports the same thing and leaves.
 * The exit skips destructors on purpose - the stack it is called from is mid-assertion and there is nothing
 * left to run correctly.
 *
 * The macros in catch-no-exceptions.hpp keep the ordinary failures away from here. What is left is Catch2
 * aborting a run on its own account: an internal error, a generator that ran dry, --abort, or one of the
 * assertion macros this suite does not use.
 */
[[noreturn]] void throw_exception(std::exception const& e)
{
    Catch::cerr() << "The run is ending here: this binary is built without exceptions, and Catch2 needed to throw.\n"
                  << "The message was: " << e.what() << '\n';
    Catch::cerr().flush();
    std::fflush(nullptr);
    std::_Exit(EXIT_FAILURE);
}

}  // namespace Catch

#endif
