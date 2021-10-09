#include "rndr/rndr.h"

#include "log/log.h"

void RNDR::HelloWorld()
{
    Log::Init();

    EXAMPLE_ERROR("Hello there, I am RNDR Library!");
    EXAMPLE_WARNING("Hello there, I am RNDR Library!");
    EXAMPLE_INFO("Hello there, I am RNDR Library!");
    EXAMPLE_DEBUG("Hello there, I am RNDR Library!");
    EXAMPLE_TRACE("Hello there, I am RNDR Library!");

    Log::ShutDown();
}