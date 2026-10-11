#pragma once

#include "opal/threading/atomic.h"
#include "opal/threading/signal.h"
#include "opal/threading/thread.h"

#include "rndr/audio/audio-device.hpp"
#include "rndr/error-codes.hpp"
#include "rndr/types.hpp"

struct IMMDeviceEnumerator;
struct IAudioClient;
struct IAudioRenderClient;
struct IAudioClock;

namespace Rndr
{

/**
 * WASAPI output in shared, event-driven mode.
 *
 * Everything COM happens on the audio thread, so the thread that builds this never needs CoInitializeEx. Start
 * waits for that thread to report whether the stream opened and hands back a code when it did not. The endpoint is
 * asked for stereo f32 at the requested rate and Windows converts to its own mix format, so the callback never has
 * to care what the hardware runs at.
 *
 * When the endpoint goes away - headphones unplugged, default device changed - the stream is torn down and the
 * thread retries against the new default every half second until one opens or it is asked to stop. The callback's
 * state is its own, so playback carries on from where it was.
 */
OPAL_START_DISABLE_WARNINGS
OPAL_DISABLE_MSVC_WARNING(4324)  // Structure was padded due to alignment specifier: the Signal wants a cache line
class WindowsAudioDevice final : public AudioDevice
{
public:
    /** Stores what it was given and nothing else; Start is what opens the stream. */
    WindowsAudioDevice(const AudioDeviceDesc& desc, AudioRenderCallback&& callback);
    ~WindowsAudioDevice() override;

    /**
     * Creates the thread events, starts the audio thread and waits for its first attempt at the stream. Called once,
     * by AudioDevice::Create, before anyone else can see the device - which is why the two steps are not a two-phase
     * initialization anybody has to know about.
     */
    ErrorCode Start();

    [[nodiscard]] u32 GetSampleRate() const override { return m_desc.sample_rate; }

private:
    static void ThreadMain(WindowsAudioDevice* device);
    void Run();

    /** Opens the default render endpoint and starts the stream. Audio thread. */
    i32 InitializeStream();
    void ReleaseStream();
    /** Runs the stream until it fails or the stop event is set; returns false on stop. */
    bool Pump();
    /**
     * Frames the stream holds ahead of what is about to be written, from the stream's clock: what was written less
     * what the endpoint has played by now. @p padding is the answer when the clock has none.
     */
    u32 GetQueuedFrames(u32 padding) const;
    /** Waits out the retry interval; returns false when the stop event is set meanwhile. */
    bool WaitBeforeRetry();

    AudioDeviceDesc m_desc;
    AudioRenderCallback m_callback;

    void* m_stop_event = nullptr;
    void* m_buffer_event = nullptr;
    Opal::ThreadHandle m_thread;
    bool m_is_thread_running = false;

    // Handshake between the constructor and the thread's first attempt at the stream.
    Opal::Signal m_started;
    Opal::Atomic<i32> m_start_result = 0;

    // Audio thread only.
    IMMDeviceEnumerator* m_enumerator = nullptr;
    IAudioClient* m_client = nullptr;
    IAudioRenderClient* m_render_client = nullptr;
    IAudioClock* m_clock = nullptr;
    u32 m_buffer_frame_count = 0;
    /** Frames handed to the stream since it opened, the silence it starts with included. */
    u64 m_frames_written = 0;
    /** Units of IAudioClock::GetPosition in a second. */
    u64 m_clock_frequency = 0;
};
OPAL_END_DISABLE_WARNINGS

}  // namespace Rndr
