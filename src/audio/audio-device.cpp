#include "rndr/audio/audio-device.hpp"

#include "opal/allocator.h"

#include "rndr/definitions.hpp"

#if RNDR_WINDOWS
#include "rndr/platform/windows-audio-device.hpp"
#endif

Opal::ScopePtr<Rndr::AudioDevice> Rndr::AudioDevice::Create(const AudioDeviceDesc& desc, AudioRenderCallback&& callback)
{
#if RNDR_WINDOWS
    return Opal::MakeScoped<AudioDevice, WindowsAudioDevice>(Opal::GetDefaultAllocator(), desc, std::move(callback));
#else
#error "Platform not supported!"
#endif
}
