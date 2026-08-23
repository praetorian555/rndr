#pragma once

#include "opal/container/array-view.h"
#include "opal/container/expected.h"
#include "opal/container/function.h"
#include "opal/container/scope-ptr.h"

#include "rndr/error-codes.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

struct AudioDeviceDesc
{
    /** Rate the render callback is asked to fill at. The device converts to whatever the endpoint runs at. */
    u32 sample_rate = 48000;
    /** Frames the device keeps queued ahead of the endpoint. More is safer against glitches, less is lower latency. */
    u32 buffer_frames = 1024;
};

/**
 * Fills a buffer of interleaved stereo f32 frames. Runs on the device's own thread, so it must not block, allocate
 * or touch anything the main thread is mutating. The buffer's size is twice the frame count and is never zero.
 */
using AudioRenderCallback = Opal::Function<void(Opal::ArrayView<f32> /*interleaved_stereo*/)>;

/**
 * The platform half of audio output: owns a thread that pulls stereo f32 from a render callback and pushes it to the
 * default output endpoint. Knows nothing about clips or voices - that is AudioMixer, which is what the callback
 * usually runs.
 *
 * Construction opens the stream and starts the thread; destruction stops the thread before returning, so whatever
 * the callback reads can be torn down right after.
 */
class AudioDevice
{
public:
    /**
     * Picks the implementation for this platform and opens its stream.
     * @return The running device, ErrorCode::NoAudioDevice on a machine with no output endpoint, or
     *         ErrorCode::PlatformError when the endpoint is there and would not open. The platform's own code is
     *         logged at error level.
     */
    static Opal::Expected<Opal::ScopePtr<AudioDevice>, ErrorCode> Create(const AudioDeviceDesc& desc, AudioRenderCallback&& callback);

    virtual ~AudioDevice() = default;

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;
    AudioDevice(AudioDevice&&) = delete;
    AudioDevice& operator=(AudioDevice&&) = delete;

    [[nodiscard]] virtual u32 GetSampleRate() const = 0;

protected:
    AudioDevice() = default;
};

}  // namespace Rndr
