#include "rndr/core/singletons.h"

#include "rndr/core/threading.h"

rndr::Singletons::Singletons()
{
    Scheduler::Get()->Init();
}

rndr::Singletons::~Singletons()
{
    Scheduler::Get()->ShutDown();
}
