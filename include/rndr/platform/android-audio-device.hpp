#pragma once

#include "opal/threading/atomic.h"
#include "opal/threading/signal.h"
#include "opal/threading/thread.h"

#include "rndr/audio/audio-device.hpp"
#include "rndr/error-codes.hpp"
#include "rndr/types.hpp"

struct AAudioStreamStruct;

namespace Rndr
{

/**
 * AAudio output: a shared stream of stereo f32 at the requested rate, whose data callback runs the render callback.
 *
 * AAudio calls the data callback on a thread of its own, in bursts of whatever size the device runs at, and converts
 * the rate when the device runs at another. When the output goes away - headphones unplugged, a Bluetooth headset
 * connecting - AAudio reports the stream disconnected. A stream may not be closed from its own callbacks, so a thread
 * of this device's closes it and opens one on the new default, retrying every half second until one opens or it is
 * asked to stop, the way WindowsAudioDevice does. The callback's state is its own, so playback carries on.
 */
OPAL_START_DISABLE_WARNINGS
OPAL_DISABLE_MSVC_WARNING(4324)  // Structure was padded due to alignment specifier: the Signal wants a cache line
class AndroidAudioDevice final : public AudioDevice
{
public:
    /** Stores what it was given and nothing else; Start is what opens the stream. */
    AndroidAudioDevice(const AudioDeviceDesc& desc, AudioRenderCallback&& callback);
    ~AndroidAudioDevice() override;

    /**
     * Opens and starts the stream, then starts the thread that reopens it. Called once, by AudioDevice::Create,
     * before anyone else can see the device.
     */
    ErrorCode Start();

    [[nodiscard]] u32 GetSampleRate() const override { return m_desc.sample_rate; }

private:
    static i32 DataCallback(AAudioStreamStruct* stream, void* user_data, void* audio_data, i32 frame_count);
    static void ErrorCallback(AAudioStreamStruct* stream, void* user_data, i32 error);
    static void ThreadMain(AndroidAudioDevice* device);
    void Run();

    /**
     * Opens the default output and starts it. Leaves no stream behind when it fails.
     * @return AAUDIO_OK or the AAudio error that stopped it.
     */
    i32 OpenStream();
    void CloseStream();

    /** Data callback. Frames the stream holds ahead of what is about to be written, from its timestamp. */
    [[nodiscard]] u32 GetQueuedFrames(AAudioStreamStruct* stream) const;

    AudioDeviceDesc m_desc;
    AudioRenderCallback m_callback;

    /** The reopening thread's, and the constructor's and destructor's while that thread is not running. */
    AAudioStreamStruct* m_stream = nullptr;
    /** Data callback only: frames handed to the stream since it opened. */
    u64 m_frames_written = 0;

    Opal::ThreadHandle m_thread;
    bool m_is_thread_running = false;
    /** Advanced when the stream disconnects and when the device is asked to stop, to wake the thread. */
    Opal::Signal m_wake;
    Opal::Atomic<bool> m_should_stop = false;
    Opal::Atomic<bool> m_is_disconnected = false;
};
OPAL_END_DISABLE_WARNINGS

}  // namespace Rndr
