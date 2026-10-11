#include <catch2/catch2.hpp>

#include <atomic>
#include <chrono>
#include <thread>

#include "opal/container/expected.h"
#include "opal/container/scope-ptr.h"

#include "rndr/audio/audio-system.hpp"
#include "rndr/error-codes.hpp"
#include "rndr/time.hpp"

#include "audio-test-common.hpp"

using namespace Rndr;

/**
 * These cases open the real output device and play through it, so they need a machine with one. Without one they
 * skip, which looks the same as passing from the outside; RNDR_TEST_REQUIRE_AUDIO=1 turns that skip into a failure
 * for a machine that is supposed to have audio.
 */
namespace
{

constexpr u32 k_rate = 48000;

/** Answers an empty pointer on the one machine a run is allowed to skip over, and fails on every other. */
Opal::ScopePtr<AudioSystem> CreateSystemOrEmpty(const AudioSystemDesc& desc = {})
{
    Opal::Expected<Opal::ScopePtr<AudioSystem>, ErrorCode> result = AudioSystem::Create(desc);
    if (result.HasValue())
    {
        return std::move(result.GetValue());
    }
    // A machine with no output endpoint is the one case a run is allowed to skip over; anything else means the
    // endpoint was there and something about opening it went wrong, which is a failure wherever it happens.
    if (result.GetError() != ErrorCode::NoAudioDevice)
    {
        FAIL("the audio device could not be opened, error code " << static_cast<u32>(result.GetError()));
    }
    if (AudioTest::IsEnvironmentFlagSet("RNDR_TEST_REQUIRE_AUDIO"))
    {
        FAIL("RNDR_TEST_REQUIRE_AUDIO is set and this machine has no audio output");
    }
    return {};
}

/** Unwraps a clip that is meant to load, so the cases below stay about the system rather than about Expected. */
AudioClipHandle CreateClipOrFail(AudioSystem& audio, AudioClip&& clip)
{
    Opal::Expected<AudioClipHandle, ErrorCode> result = audio.CreateClip(std::move(clip));
    if (!result.HasValue())
    {
        FAIL("expected a clip handle, got error code " << static_cast<u32>(result.GetError()));
    }
    return result.GetValue();
}

void SleepMilliseconds(u32 milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

/** Polls IsPlaying until it goes false or the timeout passes; returns whether it went false. */
bool WaitForSoundToEnd(const AudioSystem& audio, SoundHandle sound, u32 timeout_ms)
{
    const Timestamp start = GetTimestamp();
    while (audio.IsPlaying(sound))
    {
        if (GetDuration(start, GetTimestamp()) * 1000.0 > timeout_ms)
        {
            return false;
        }
        SleepMilliseconds(5);
    }
    return true;
}

}  // namespace

TEST_CASE("Audio system opens and closes the device", "[audio-device]")
{
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrEmpty();
    if (!audio)
    {
        SKIP("No audio output on this machine.");
    }
    REQUIRE(audio.IsValid());
    REQUIRE(audio->GetSampleRate() == k_rate);
    REQUIRE(audio->GetActiveVoiceCount() == 0);
    REQUIRE(audio->GetMasterVolume() == 1.0f);
}

TEST_CASE("Audio system plays a sound to the end", "[audio-device]")
{
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrEmpty();
    if (!audio)
    {
        SKIP("No audio output on this machine.");
    }
    const AudioClipHandle tone = CreateClipOrFail(*audio, AudioTest::MakeSineClip(k_rate, 1, 440.0f, k_rate / 10, 0.2f));
    REQUIRE(audio->IsClipValid(tone));

    const SoundHandle sound = audio->Play(tone);
    REQUIRE(sound.IsValid());
    REQUIRE(audio->IsPlaying(sound));

    // 100 ms of sound; give the device its own startup latency on top.
    REQUIRE(WaitForSoundToEnd(*audio, sound, 1000));
    SleepMilliseconds(50);
    REQUIRE(audio->GetActiveVoiceCount() == 0);
    REQUIRE(audio->GetDroppedCommandCount() == 0);
}

TEST_CASE("Audio system takes control calls while a sound plays", "[audio-device]")
{
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrEmpty();
    if (!audio)
    {
        SKIP("No audio output on this machine.");
    }
    const AudioClipHandle tone = CreateClipOrFail(*audio, AudioTest::MakeSineClip(k_rate, 2, 330.0f, k_rate / 4, 0.2f));
    const SoundHandle sound = audio->Play(tone, {.pan = -0.5f, .loop = true});
    REQUIRE(sound.IsValid());

    SleepMilliseconds(50);
    audio->SetVolume(sound, 0.5f);
    audio->SetPan(sound, 0.5f);
    audio->SetPitch(sound, 1.5f);
    audio->SetMasterVolume(0.8f);
    audio->SetBusVolume(0, 0.9f);
    audio->Pause(sound);
    SleepMilliseconds(50);
    REQUIRE(audio->IsPlaying(sound));
    audio->Resume(sound);
    SleepMilliseconds(50);
    REQUIRE(audio->IsPlaying(sound));
    REQUIRE(audio->GetActiveVoiceCount() == 1);

    audio->Stop(sound);
    REQUIRE(WaitForSoundToEnd(*audio, sound, 1000));
    REQUIRE(audio->GetDroppedCommandCount() == 0);
}

TEST_CASE("Audio system shuts down promptly with a sound still playing", "[audio-device]")
{
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrEmpty();
    if (!audio)
    {
        SKIP("No audio output on this machine.");
    }
    const AudioClipHandle tone = CreateClipOrFail(*audio, AudioTest::MakeSineClip(k_rate, 1, 220.0f, k_rate / 4, 0.2f));
    REQUIRE(audio->Play(tone, {.loop = true}).IsValid());
    SleepMilliseconds(100);

    const Timestamp start = GetTimestamp();
    audio.Reset();
    const f64 shutdown_ms = GetDuration(start, GetTimestamp()) * 1000.0;
    REQUIRE(shutdown_ms < 500.0);
}

TEST_CASE("Audio system destroys a clip out from under a sound", "[audio-device]")
{
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrEmpty();
    if (!audio)
    {
        SKIP("No audio output on this machine.");
    }
    const AudioClipHandle tone = CreateClipOrFail(*audio, AudioTest::MakeSineClip(k_rate, 1, 220.0f, k_rate / 4, 0.2f));
    const SoundHandle sound = audio->Play(tone, {.loop = true});
    SleepMilliseconds(50);

    audio->DestroyClip(tone);
    REQUIRE_FALSE(audio->IsClipValid(tone));
    REQUIRE(WaitForSoundToEnd(*audio, sound, 1000));
    // Let the mix epoch move on so the slot is reclaimed, then fill it again.
    SleepMilliseconds(100);
    const AudioClipHandle again = CreateClipOrFail(*audio, AudioTest::MakeSineClip(k_rate, 1, 220.0f, k_rate / 10, 0.2f));
    REQUIRE(again.IsValid());
}

TEST_CASE("Audio system reports bad input without throwing from Play", "[audio-device]")
{
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrEmpty();
    if (!audio)
    {
        SKIP("No audio output on this machine.");
    }
    const Opal::Expected<AudioClipHandle, ErrorCode> missing = audio->LoadClip("does-not-exist.wav");
    REQUIRE_FALSE(missing.HasValue());
    REQUIRE(missing.GetError() == ErrorCode::FileNotFound);
    REQUIRE_FALSE(audio->Play(AudioClipHandle{}).IsValid());
    REQUIRE_FALSE(audio->IsPlaying(SoundHandle{}));
}

TEST_CASE("Audio system plays a stream another thread writes", "[audio-device]")
{
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrEmpty();
    if (!audio)
    {
        SKIP("No audio output on this machine.");
    }
    Opal::Expected<AudioStreamHandle, ErrorCode> stream_result = audio->CreateStream({.channel_count = 1, .capacity_frames = k_rate / 4});
    REQUIRE(stream_result.HasValue());
    const AudioStreamHandle stream = stream_result.GetValue();

    // 600 ms of a quiet tone through a ring that holds 250 ms, so the writer has to keep up with playback.
    constexpr u64 k_total_frames = k_rate * 6 / 10;
    const AudioClip tone = AudioTest::MakeSineClip(k_rate, 1, 440.0f, k_total_frames, 0.1f);
    const Opal::ArrayView<const f32> samples = tone.GetData();
    // Whatever fits each time round: a sleep can take a whole scheduler tick, 15 ms on Windows.
    std::atomic<bool> is_written = false;
    std::thread writer(
        [&audio, &is_written, stream, samples]
        {
            u64 written = 0;
            while (written < k_total_frames)
            {
                written += audio->WriteStream(stream, {samples.GetData() + written, k_total_frames - written});
                SleepMilliseconds(2);
            }
            is_written.store(true);
        });

    const SoundHandle sound = audio->PlayStream(stream, {.start_paused = true});
    REQUIRE(sound.IsValid());
    const Timestamp fill_start = GetTimestamp();
    while (audio->GetStreamQueuedFrames(stream) < k_rate / 10 && GetDuration(fill_start, GetTimestamp()) < 1.0)
    {
        SleepMilliseconds(1);
    }
    REQUIRE(audio->GetStreamPosition(stream) == 0);
    audio->Resume(sound);

    // The position never goes back, and moves at the rate the device plays.
    const Timestamp play_start = GetTimestamp();
    u64 last_position = 0;
    u64 first_moving_position = 0;
    Timestamp first_moving_time = 0;
    u64 lag_frames = 0;
    bool is_lag_measured = false;
    while (last_position < k_total_frames && GetDuration(play_start, GetTimestamp()) < 3.0)
    {
        const u64 queued = audio->GetStreamQueuedFrames(stream);
        const u64 position = audio->GetStreamPosition(stream);
        REQUIRE(position >= last_position);
        REQUIRE(position <= k_total_frames);
        if (position > 0 && first_moving_time == 0)
        {
            first_moving_position = position;
            first_moving_time = GetTimestamp();
        }
        // Once everything is written, what has been mixed is known, and the speaker is behind it by the device's lag.
        if (!is_lag_measured && queued > 0 && position > 0 && is_written.load())
        {
            const u64 mixed = k_total_frames - queued;
            REQUIRE(mixed >= position);
            lag_frames = mixed - position;
            is_lag_measured = true;
        }
        last_position = position;
        SleepMilliseconds(5);
    }
    const f64 play_seconds = GetDuration(first_moving_time, GetTimestamp());
    writer.join();

    REQUIRE(last_position == k_total_frames);
    const f64 rate = static_cast<f64>(last_position - first_moving_position) / play_seconds;
    INFO("position moved at " << rate << " frames a second, " << lag_frames << " frames behind the mix");
    REQUIRE(rate > k_rate * 0.8);
    REQUIRE(rate < k_rate * 1.2);
    REQUIRE(is_lag_measured);
    REQUIRE(lag_frames < k_rate / 5);
    // The one dry spell allowed is the end, where the sound plays on after the last frame.
    REQUIRE(audio->GetStreamUnderrunCount(stream) <= 1);

    audio->Stop(sound);
    REQUIRE(WaitForSoundToEnd(*audio, sound, 1000));
    audio->DestroyStream(stream);
    REQUIRE_FALSE(audio->IsStreamValid(stream));
}
