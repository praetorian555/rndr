#pragma once

#include "opal/exceptions.h"

#include "rndr/types.hpp"

namespace Rndr
{

struct GraphicsAPIException : Opal::Exception
{
    explicit GraphicsAPIException(u32 error_code, const char* message) : Opal::Exception(Opal::StringEx("Graphics API error: ") + static_cast<u64>(error_code) + " - " + message)
    {
    }
};

struct WindowCreationException : Opal::Exception
{
    explicit WindowCreationException(const char* message) : Opal::Exception(Opal::StringEx("Window creation failed: ") + message)
    {
    }
};

/** Thrown by an AudioDevice that cannot open the output stream. The code is the platform's: an HRESULT on Windows. */
struct AudioDeviceException : Opal::Exception
{
    explicit AudioDeviceException(u32 error_code, const char* message)
        : Opal::Exception(Opal::StringEx("Audio device error: ") + HexCode(error_code).text + " - " + message), code(error_code)
    {
    }

    u32 code;

private:
    struct HexCode
    {
        char text[11];
        explicit HexCode(u32 value)
        {
            static constexpr char k_digits[] = "0123456789abcdef";
            text[0] = '0';
            text[1] = 'x';
            for (int i = 0; i < 8; i++)
            {
                text[2 + i] = k_digits[(value >> (28 - 4 * i)) & 0xF];
            }
            text[10] = '\0';
        }
    };
};

}  // namespace Rndr