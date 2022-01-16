#include "rndr/core/singletons.h"

#include "rndr/core/threading.h"
#include "rndr/core/log.h"

rndr::Singletons::Singletons()
{
    StdAsyncLogger::Get()->Init();
    Scheduler::Get()->Init();
}

rndr::Singletons::~Singletons()
{
    Scheduler::Get()->ShutDown();
    StdAsyncLogger::Get()->ShutDown();
}
