#include "rndr/core/singletons.h"

#include "rndr/core/threading.h"
#include "rndr/core/log.h"

#include "rndr/profiling/cputracer.h"

rndr::Singletons::Singletons()
{
    StdAsyncLogger::Get()->Init();
    Scheduler::Get()->Init();
    CpuTracer::Get()->Init();
}

rndr::Singletons::~Singletons()
{
    CpuTracer::Get()->ShutDown();
    Scheduler::Get()->ShutDown();
    StdAsyncLogger::Get()->ShutDown();
}
