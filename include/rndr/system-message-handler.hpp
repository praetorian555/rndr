#pragma once

#include "rndr/generic-window.hpp"
#include "rndr/input-primitives.h"
#include "rndr/types.h"

namespace Rndr
{
struct SystemMessageHandler
{
    virtual ~SystemMessageHandler() = default;

    virtual void OnWindowClose(GenericWindow* window) = 0;
    virtual void OnWindowSizeChanged(GenericWindow* window, i32 width, i32 height) = 0;

    virtual bool OnButtonDown(InputPrimitive key_code, bool is_repeated) = 0;
    virtual bool OnButtonUp(InputPrimitive key_code, bool is_repeated) = 0;
    virtual bool OnCharacter(uchar32 character, bool is_repeated) = 0;
};
}  // namespace Rndr