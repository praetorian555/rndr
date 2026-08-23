#include "rndr/audio/audio-device.hpp"

#include "opal/allocator.h"

#include "rndr/definitions.hpp"

#if RNDR_WINDOWS
#include "rndr/platform/windows-audio-device.hpp"
#endif

Opal::Expected<Opal::ScopePtr<Rndr::AudioDevice>, Rndr::ErrorCode> Rndr::AudioDevice::Create(const AudioDeviceDesc& desc,
                                                                                             AudioRenderCallback&& callback)
{
    using Result = Opal::Expected<Opal::ScopePtr<AudioDevice>, ErrorCode>;
#if RNDR_WINDOWS
    Opal::ScopePtr<WindowsAudioDevice> device =
        Opal::MakeScoped<WindowsAudioDevice>(Opal::GetDefaultAllocator(), desc, std::move(callback));
    if (!device.IsValid())
    {
        return Result(ErrorCode::OutOfMemory);
    }
    const ErrorCode error = device->Start();
    if (error != ErrorCode::Success)
    {
        return Result(error);
    }
    // Only now that the stream is running does anything outside get to see it.
    return Result(Opal::ScopePtr<AudioDevice>(std::move(device)));
#else
#error "Platform not supported!"
#endif
}
