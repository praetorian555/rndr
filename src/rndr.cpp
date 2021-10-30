#include "rndr/rndr.h"

#include "log/log.h"

void RNDR::HelloWorld()
{
    Log::Init();

    RNDR_ERROR("Hello there, I am RNDR Library!");
    RNDR_WARNING("Hello there, I am RNDR Library!");
    RNDR_INFO("Hello there, I am RNDR Library!");
    RNDR_DEBUG("Hello there, I am RNDR Library!");
    RNDR_TRACE("Hello there, I am RNDR Library!");

    Log::ShutDown();
}