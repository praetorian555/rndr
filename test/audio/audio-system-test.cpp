#include <catch2/catch2.hpp>

#include <chrono>
#include <thread>

#include "opal/container/scope-ptr.h"
#include "opal/exceptions.h"

#include "rndr/audio/audio-system.hpp"
#include "rndr/exception.hpp"
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

Opal::ScopePtr<AudioSystem> CreateSystemOrSkip(const AudioSystemDesc& desc = {})
{
    try
    {
        return Opal::MakeScoped<AudioSystem>(nullptr, desc);
    }
    catch (const AudioDeviceException& exception)
    {
        if (AudioTest::IsEnvironmentFlagSet("RNDR_TEST_REQUIRE_AUDIO"))
        {
            FAIL("RNDR_TEST_REQUIRE_AUDIO is set and the audio device could not be opened: " << exception.what());
        }
        SKIP("No usable audio output on this machine: " << exception.what());
    }
    return {};
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
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrSkip();
    REQUIRE(audio.IsValid());
    REQUIRE(audio->GetSampleRate() == k_rate);
    REQUIRE(audio->GetActiveVoiceCount() == 0);
    REQUIRE(audio->GetMasterVolume() == 1.0f);
}

TEST_CASE("Audio system plays a sound to the end", "[audio-device]")
{
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrSkip();
    const AudioClipHandle tone = audio->CreateClip(AudioTest::MakeSineClip(k_rate, 1, 440.0f, k_rate / 10, 0.2f));
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
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrSkip();
    const AudioClipHandle tone = audio->CreateClip(AudioTest::MakeSineClip(k_rate, 2, 330.0f, k_rate / 4, 0.2f));
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
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrSkip();
    const AudioClipHandle tone = audio->CreateClip(AudioTest::MakeSineClip(k_rate, 1, 220.0f, k_rate / 4, 0.2f));
    REQUIRE(audio->Play(tone, {.loop = true}).IsValid());
    SleepMilliseconds(100);

    const Timestamp start = GetTimestamp();
    audio.Reset();
    const f64 shutdown_ms = GetDuration(start, GetTimestamp()) * 1000.0;
    REQUIRE(shutdown_ms < 500.0);
}

TEST_CASE("Audio system destroys a clip out from under a sound", "[audio-device]")
{
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrSkip();
    const AudioClipHandle tone = audio->CreateClip(AudioTest::MakeSineClip(k_rate, 1, 220.0f, k_rate / 4, 0.2f));
    const SoundHandle sound = audio->Play(tone, {.loop = true});
    SleepMilliseconds(50);

    audio->DestroyClip(tone);
    REQUIRE_FALSE(audio->IsClipValid(tone));
    REQUIRE(WaitForSoundToEnd(*audio, sound, 1000));
    // Let the mix epoch move on so the slot is reclaimed, then fill it again.
    SleepMilliseconds(100);
    const AudioClipHandle again = audio->CreateClip(AudioTest::MakeSineClip(k_rate, 1, 220.0f, k_rate / 10, 0.2f));
    REQUIRE(again.IsValid());
}

TEST_CASE("Audio system reports bad input without throwing from Play", "[audio-device]")
{
    Opal::ScopePtr<AudioSystem> audio = CreateSystemOrSkip();
    REQUIRE_THROWS_AS(audio->LoadClip("does-not-exist.wav"), Opal::Exception);
    REQUIRE_FALSE(audio->Play(AudioClipHandle{}).IsValid());
    REQUIRE_FALSE(audio->IsPlaying(SoundHandle{}));
}

TEST_CASE("Audio system rejects an empty description", "[audio]")
{
    REQUIRE_THROWS_AS(AudioSystem({.sample_rate = 0}), Opal::Exception);
}
