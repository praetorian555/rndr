#include "rndr/core/setup.h"

#include "rndr/core/rndrapp.h"

rndr::RndrApp* rndr::Init()
{
    assert(!GRndrApp);
    GRndrApp = new RndrApp{};
    return GRndrApp;
}

void rndr::ShutDown()
{
    delete GRndrApp;
    GRndrApp = nullptr;
}
