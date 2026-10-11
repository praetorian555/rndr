#include "rndr/platform/android-audio-device.hpp"

#include <aaudio/AAudio.h>
#include <cstdint>
#include <time.h>

#include "opal/time.h"

#include "rndr/log.hpp"

namespace
{

/** How long to wait between attempts to reopen a lost output. */
constexpr Rndr::u64 k_retry_interval_ms = 500;

Rndr::i64 GetMonotonicNanoseconds()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<Rndr::i64>(now.tv_sec) * 1'000'000'000 + now.tv_nsec;
}

}  // namespace

Rndr::AndroidAudioDevice::AndroidAudioDevice(const AudioDeviceDesc& desc, AudioRenderCallback&& callback)
    : m_desc(desc), m_callback(std::move(callback))
{
}

Rndr::ErrorCode Rndr::AndroidAudioDevice::Start()
{
    if (m_desc.sample_rate == 0 || m_desc.buffer_frames == 0)
    {
        RNDR_LOG_ERROR("AndroidAudioDevice: sample rate and buffer size must be greater than 0");
        return ErrorCode::InvalidArgument;
    }

    const aaudio_result_t result = OpenStream();
    if (result != AAUDIO_OK)
    {
        RNDR_LOG_ERROR("AndroidAudioDevice: could not open the output stream ({})", AAudio_convertResultToText(result));
        return result == AAUDIO_ERROR_NO_SERVICE || result == AAUDIO_ERROR_UNAVAILABLE ? ErrorCode::NoAudioDevice
                                                                                        : ErrorCode::PlatformError;
    }

    Opal::Expected<Opal::ThreadHandle, Opal::ErrorCode> thread = Opal::CreateThread(&AndroidAudioDevice::ThreadMain, this);
    if (!thread.HasValue())
    {
        RNDR_LOG_ERROR("AndroidAudioDevice: could not start the thread that reopens the stream");
        CloseStream();
        return ErrorCode::PlatformError;
    }
    m_thread = thread.GetValue();
    m_is_thread_running = true;
    return ErrorCode::Success;
}

Rndr::AndroidAudioDevice::~AndroidAudioDevice()
{
    // Start may have failed anywhere along the way, so nothing here is assumed to exist.
    if (m_is_thread_running)
    {
        m_should_stop.Store(true);
        m_wake.NotifyAll();
        Opal::JoinThread(m_thread);
    }
    CloseStream();
}

void Rndr::AndroidAudioDevice::ThreadMain(AndroidAudioDevice* device)
{
    device->Run();
}

void Rndr::AndroidAudioDevice::Run()
{
    while (true)
    {
        // The state is read before the flags, so a notification that lands between the two is not lost.
        const u32 state = m_wake.GetState();
        if (m_should_stop.Load())
        {
            return;
        }
        if (!m_is_disconnected.Load())
        {
            m_wake.Wait(state);
            continue;
        }

        RNDR_LOG_WARNING("AndroidAudioDevice: lost the audio output, reopening the stream");
        CloseStream();
        m_is_disconnected.Store(false);
        while (true)
        {
            const aaudio_result_t result = OpenStream();
            if (result == AAUDIO_OK)
            {
                RNDR_LOG_INFO("AndroidAudioDevice: audio output restored");
                break;
            }
            RNDR_LOG_DEBUG("AndroidAudioDevice: the output did not reopen ({})", AAudio_convertResultToText(result));
            const u32 retry_state = m_wake.GetState();
            if (m_should_stop.Load())
            {
                return;
            }
            m_wake.WaitFor(retry_state, k_retry_interval_ms);
            if (m_should_stop.Load())
            {
                return;
            }
        }
    }
}

Rndr::i32 Rndr::AndroidAudioDevice::OpenStream()
{
    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK)
    {
        return result;
    }
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setSampleRate(builder, static_cast<i32>(m_desc.sample_rate));
    AAudioStreamBuilder_setDataCallback(builder, &AndroidAudioDevice::DataCallback, this);
    AAudioStreamBuilder_setErrorCallback(builder, &AndroidAudioDevice::ErrorCallback, this);

    AAudioStream* stream = nullptr;
    result = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);
    if (result != AAUDIO_OK)
    {
        return result;
    }
    // The callback is written for exactly this: anything else would be read as the wrong samples.
    if (AAudioStream_getFormat(stream) != AAUDIO_FORMAT_PCM_FLOAT || AAudioStream_getChannelCount(stream) != 2 ||
        AAudioStream_getSampleRate(stream) != static_cast<i32>(m_desc.sample_rate))
    {
        RNDR_LOG_ERROR("AndroidAudioDevice: the stream opened as format {}, {} channels at {} Hz rather than stereo f32 at {} Hz",
                       AAudioStream_getFormat(stream), AAudioStream_getChannelCount(stream), AAudioStream_getSampleRate(stream),
                       m_desc.sample_rate);
        AAudioStream_close(stream);
        return AAUDIO_ERROR_INVALID_FORMAT;
    }
    // A request; the stream rounds it to its bursts and caps it at its capacity.
    AAudioStream_setBufferSizeInFrames(stream, static_cast<i32>(m_desc.buffer_frames));

    m_frames_written = 0;
    result = AAudioStream_requestStart(stream);
    if (result != AAUDIO_OK)
    {
        AAudioStream_close(stream);
        return result;
    }
    m_stream = stream;
    return AAUDIO_OK;
}

void Rndr::AndroidAudioDevice::CloseStream()
{
    if (m_stream == nullptr)
    {
        return;
    }
    // Close waits for a callback in flight, and none comes after it.
    AAudioStream_requestStop(m_stream);
    AAudioStream_close(m_stream);
    m_stream = nullptr;
}

Rndr::i32 Rndr::AndroidAudioDevice::DataCallback(AAudioStream* stream, void* user_data, void* audio_data, i32 frame_count)
{
    AndroidAudioDevice* device = static_cast<AndroidAudioDevice*>(user_data);
    const AudioRenderTiming timing{.queued_frames = device->GetQueuedFrames(stream), .time_seconds = Opal::GetSeconds()};
    device->m_callback(Opal::ArrayView<f32>(static_cast<f32*>(audio_data), static_cast<u64>(frame_count) * 2), timing);
    device->m_frames_written += static_cast<u64>(frame_count);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void Rndr::AndroidAudioDevice::ErrorCallback(AAudioStream* stream, void* user_data, aaudio_result_t error)
{
    (void)stream;
    // Disconnection is the one error that ends a stream; the thread does the reopening, which this callback may not.
    if (error == AAUDIO_ERROR_DISCONNECTED)
    {
        AndroidAudioDevice* device = static_cast<AndroidAudioDevice*>(user_data);
        device->m_is_disconnected.Store(true);
        device->m_wake.NotifyAll();
    }
}

Rndr::u32 Rndr::AndroidAudioDevice::GetQueuedFrames(AAudioStream* stream) const
{
    // The timestamp says which frame reached the speaker when; it has played on since at the stream's rate. Until the
    // stream has played anything there is none, and what has been written less what the stream has read stands in.
    int64_t frame_position = 0;
    int64_t frame_time_ns = 0;
    f64 played = 0.0;
    if (AAudioStream_getTimestamp(stream, CLOCK_MONOTONIC, &frame_position, &frame_time_ns) == AAUDIO_OK)
    {
        const i64 elapsed_ns = GetMonotonicNanoseconds() - frame_time_ns;
        played = static_cast<f64>(frame_position) + static_cast<f64>(elapsed_ns > 0 ? elapsed_ns : 0) * m_desc.sample_rate / 1e9;
    }
    else
    {
        played = static_cast<f64>(AAudioStream_getFramesRead(stream));
    }
    const f64 queued = static_cast<f64>(m_frames_written) - played;
    return queued > 0.0 ? static_cast<u32>(queued) : 0;
}
