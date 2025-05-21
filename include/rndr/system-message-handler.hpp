#pragma once

#include "generic-window.hpp"
#include "rndr/types.h"

namespace Rndr
{
struct SystemMessageHandler
{
    virtual ~SystemMessageHandler() = default;

    virtual void OnWindowClose(GenericWindow* window) = 0;
    virtual void OnWindowSizeChanged(GenericWindow* window, i32 width, i32 height) = 0;
};
}  // namespace Rndr