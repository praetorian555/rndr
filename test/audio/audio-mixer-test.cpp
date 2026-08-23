#include <catch2/catch2.hpp>

#include <cmath>

#include "rndr/audio/audio-mixer.hpp"
#include "rndr/error-codes.hpp"

#include "audio-test-common.hpp"

using namespace Rndr;

namespace
{

constexpr u32 k_rate = 48000;
/** The mixer ramps every gain change over 2 ms; cases that read a steady gain skip this many frames first. */
constexpr u64 k_ramp_frames = k_rate / 500;
/** What a centred mono sound contributes to each channel: constant-power pan at the middle. */
const f32 k_centre_gain = std::cos(AudioTest::k_pi / 4.0f);

AudioMixerDesc SmallDesc()
{
    return {.sample_rate = k_rate, .max_voices = 4, .max_clips = 4, .command_queue_capacity = 64};
}

/** Unwraps a CreateClip that is meant to succeed, so the cases below read as they did before it returned a code. */
AudioClipHandle CreateClipOrFail(AudioMixer& mixer, AudioClip&& clip)
{
    Opal::Expected<AudioClipHandle, ErrorCode> result = mixer.CreateClip(std::move(clip));
    if (!result.HasValue())
    {
        FAIL("expected a clip handle, got error code " << static_cast<u32>(result.GetError()));
    }
    return result.GetValue();
}

/** The code a CreateClip was expected to fail with. */
ErrorCode CreateClipError(AudioMixer& mixer, AudioClip&& clip)
{
    Opal::Expected<AudioClipHandle, ErrorCode> result = mixer.CreateClip(std::move(clip));
    if (result.HasValue())
    {
        FAIL("expected a failure, got a clip handle");
    }
    return result.GetError();
}

Opal::DynamicArray<f32> Mix(AudioMixer& mixer, u64 frame_count)
{
    Opal::DynamicArray<f32> out(frame_count * 2, 123.0f);  // a value Mix has to overwrite
    mixer.Mix({out.GetData(), out.GetSize()});
    return out;
}

/** Asserts that every frame in [first, last) of the buffer holds the two given values. */
void RequireFrames(const Opal::DynamicArray<f32>& out, u64 first, u64 last, f32 left, f32 right, f32 margin = 1e-5f)
{
    for (u64 frame = first; frame < last; frame++)
    {
        INFO("frame " << frame);
        REQUIRE(out[frame * 2] == Catch::Approx(left).margin(margin));
        REQUIRE(out[frame * 2 + 1] == Catch::Approx(right).margin(margin));
    }
}

/** A mono clip whose sample at frame f is f / frame_count, for cases that need to see where playback is. */
AudioClip MakeRampClip(u32 sample_rate, u64 frame_count)
{
    Opal::DynamicArray<f32> samples(frame_count);
    for (u64 i = 0; i < frame_count; i++)
    {
        samples[i] = static_cast<f32>(i) / static_cast<f32>(frame_count);
    }
    return AudioClip::Create(sample_rate, 1, {samples.GetData(), samples.GetSize()}).GetValue();
}

}  // namespace

TEST_CASE("Mixer construction", "[audio]")
{
    // A desc with a zero field is a bug in the calling code, so it asserts in a debug build rather than reporting
    // anything. What can be tested here is what a release build is left with: something that works.
#if !RNDR_DEBUG
    SECTION("An empty capacity is clamped rather than honoured")
    {
        const AudioMixer clamped({.sample_rate = 0, .max_voices = 0, .max_clips = 0, .command_queue_capacity = 0});
        REQUIRE(clamped.GetSampleRate() >= 8000);
        REQUIRE(clamped.GetMaxVoices() >= 1);
    }
#endif

    SECTION("Starts silent and idle")
    {
        AudioMixer mixer(SmallDesc());
        REQUIRE(mixer.GetSampleRate() == k_rate);
        REQUIRE(mixer.GetMasterVolume() == 1.0f);
        REQUIRE(mixer.GetBusVolume(0) == 1.0f);
        REQUIRE(mixer.GetBusVolume(k_audio_bus_count - 1) == 1.0f);
        REQUIRE(mixer.GetActiveVoiceCount() == 0);
        REQUIRE(mixer.GetMixEpoch() == 0);

        const Opal::DynamicArray<f32> out = Mix(mixer, 64);
        RequireFrames(out, 0, 64, 0.0f, 0.0f);
        REQUIRE(mixer.GetMixEpoch() == 1);
    }
}

TEST_CASE("Mixer clips", "[audio]")
{
    AudioMixer mixer(SmallDesc());

    SECTION("An empty clip is refused")
    {
        REQUIRE(CreateClipError(mixer, AudioClip()) == ErrorCode::InvalidArgument);
    }

    SECTION("Handles are valid until destroyed")
    {
        const AudioClipHandle clip = CreateClipOrFail(mixer, AudioTest::MakeConstantClip(k_rate, 1, 100, 0.5f));
        REQUIRE(clip.IsValid());
        REQUIRE(mixer.IsClipValid(clip));
        REQUIRE_FALSE(mixer.IsClipValid(AudioClipHandle{}));
        REQUIRE_FALSE(mixer.IsClipValid(AudioClipHandle{.index = 99, .generation = 1}));

        mixer.DestroyClip(clip);
        REQUIRE_FALSE(mixer.IsClipValid(clip));
        REQUIRE_FALSE(mixer.Play(clip).IsValid());
        mixer.DestroyClip(clip);  // stale: no-op
    }

    SECTION("Slot capacity is honoured")
    {
        for (u32 i = 0; i < 4; i++)
        {
            REQUIRE(CreateClipOrFail(mixer, AudioTest::MakeConstantClip(k_rate, 1, 10, 0.1f)).IsValid());
        }
        REQUIRE(CreateClipError(mixer, AudioTest::MakeConstantClip(k_rate, 1, 10, 0.1f)) == ErrorCode::OutOfResources);
    }

    SECTION("A destroyed slot is reused only after two more mixes")
    {
        AudioMixer single({.sample_rate = k_rate, .max_voices = 1, .max_clips = 1});
        const AudioClipHandle first = CreateClipOrFail(single, AudioTest::MakeConstantClip(k_rate, 1, 10, 0.1f));
        single.DestroyClip(first);

        REQUIRE(CreateClipError(single, AudioTest::MakeConstantClip(k_rate, 1, 10, 0.1f)) == ErrorCode::OutOfResources);
        Mix(single, 8);
        REQUIRE(CreateClipError(single, AudioTest::MakeConstantClip(k_rate, 1, 10, 0.1f)) == ErrorCode::OutOfResources);
        Mix(single, 8);
        const AudioClipHandle second = CreateClipOrFail(single, AudioTest::MakeConstantClip(k_rate, 1, 10, 0.1f));
        REQUIRE(second.IsValid());
        REQUIRE(second.index == first.index);
        REQUIRE(second.generation != first.generation);
        REQUIRE_FALSE(single.IsClipValid(first));
    }

    SECTION("Destroying a clip finishes the voices on it")
    {
        const AudioClipHandle clip = CreateClipOrFail(mixer, AudioTest::MakeConstantClip(k_rate, 1, 100, 0.5f));
        const SoundHandle sound = mixer.Play(clip, {.loop = true});
        Mix(mixer, 200);
        REQUIRE(mixer.IsPlaying(sound));
        REQUIRE(mixer.GetActiveVoiceCount() == 1);

        mixer.DestroyClip(clip);
        REQUIRE(mixer.IsPlaying(sound));  // until the audio thread sees the command
        const Opal::DynamicArray<f32> out = Mix(mixer, 64);
        REQUIRE_FALSE(mixer.IsPlaying(sound));
        REQUIRE(mixer.GetActiveVoiceCount() == 0);
        RequireFrames(out, 0, 64, 0.0f, 0.0f);
    }
}

TEST_CASE("Mixer playback", "[audio]")
{
    AudioMixer mixer(SmallDesc());
    const AudioClipHandle constant = CreateClipOrFail(mixer, AudioTest::MakeConstantClip(k_rate, 1, 1000, 0.5f));

    SECTION("A mono clip reaches both channels at the centre gain and ends on time")
    {
        const SoundHandle sound = mixer.Play(constant);
        REQUIRE(sound.IsValid());
        REQUIRE(mixer.IsPlaying(sound));

        const Opal::DynamicArray<f32> out = Mix(mixer, 1100);
        REQUIRE(out[0] == Catch::Approx(0.0f).margin(0.01f));  // fades in rather than stepping
        RequireFrames(out, k_ramp_frames, 1000, 0.5f * k_centre_gain, 0.5f * k_centre_gain);
        RequireFrames(out, 1000, 1100, 0.0f, 0.0f);
        REQUIRE_FALSE(mixer.IsPlaying(sound));
        REQUIRE(mixer.GetActiveVoiceCount() == 0);
    }

    SECTION("Hard pan silences the other side")
    {
        const SoundHandle left = mixer.Play(constant, {.pan = -1.0f});
        const Opal::DynamicArray<f32> out_left = Mix(mixer, 500);
        RequireFrames(out_left, k_ramp_frames, 500, 0.5f, 0.0f);
        mixer.Stop(left);
        Mix(mixer, 500);

        mixer.Play(constant, {.pan = 1.0f});
        const Opal::DynamicArray<f32> out_right = Mix(mixer, 500);
        RequireFrames(out_right, k_ramp_frames, 500, 0.0f, 0.5f);
    }

    SECTION("A stereo clip keeps its channels apart")
    {
        Opal::DynamicArray<f32> samples(200);
        for (u64 frame = 0; frame < 100; frame++)
        {
            samples[frame * 2] = 0.25f;
            samples[frame * 2 + 1] = -0.25f;
        }
        const AudioClipHandle stereo =
            CreateClipOrFail(mixer, AudioClip::Create(k_rate, 2, {samples.GetData(), samples.GetSize()}).GetValue());
        mixer.Play(stereo, {.loop = true});
        const Opal::DynamicArray<f32> out = Mix(mixer, 500);
        RequireFrames(out, k_ramp_frames, 500, 0.25f * k_centre_gain, -0.25f * k_centre_gain);
    }

    SECTION("Volume, bus and master multiply")
    {
        mixer.SetMasterVolume(0.5f);
        mixer.SetBusVolume(3, 0.5f);
        mixer.Play(constant, {.volume = 0.5f, .bus = 3});
        const Opal::DynamicArray<f32> out = Mix(mixer, 500);
        RequireFrames(out, k_ramp_frames, 500, 0.125f * 0.5f * k_centre_gain, 0.125f * 0.5f * k_centre_gain);
    }

    SECTION("Bus out of range plays through bus 0")
    {
        mixer.SetBusVolume(0, 0.5f);
        const SoundHandle sound = mixer.Play(constant, {.bus = k_audio_bus_count});
        REQUIRE(sound.IsValid());
        const Opal::DynamicArray<f32> out = Mix(mixer, 500);
        RequireFrames(out, k_ramp_frames, 500, 0.25f * k_centre_gain, 0.25f * k_centre_gain);
    }

    SECTION("Pitch two finishes in half the frames")
    {
        const SoundHandle fast = mixer.Play(constant, {.pitch = 2.0f});
        Mix(mixer, 600);
        REQUIRE_FALSE(mixer.IsPlaying(fast));

        const SoundHandle normal = mixer.Play(constant);
        Mix(mixer, 600);
        REQUIRE(mixer.IsPlaying(normal));
    }

    SECTION("A clip at another rate is resampled to the output rate")
    {
        const AudioClipHandle slow = CreateClipOrFail(mixer, AudioTest::MakeConstantClip(44100, 1, 4410, 0.5f));
        const SoundHandle sound = mixer.Play(slow);
        Mix(mixer, 4700);
        REQUIRE(mixer.IsPlaying(sound));
        const Opal::DynamicArray<f32> out = Mix(mixer, 200);
        REQUIRE_FALSE(mixer.IsPlaying(sound));
        // 4410 frames at 44100 are 4800 at 48000, so the sound ran out 100 frames into this buffer.
        RequireFrames(out, 0, 90, 0.5f * k_centre_gain, 0.5f * k_centre_gain);
        RequireFrames(out, 110, 200, 0.0f, 0.0f);
    }

    SECTION("Resampling interpolates rather than steps")
    {
        const AudioClipHandle ramp = CreateClipOrFail(mixer, MakeRampClip(24000, 240));
        mixer.Play(ramp, {.pan = -1.0f});
        const Opal::DynamicArray<f32> out = Mix(mixer, 400);
        // At half the output rate, every output frame advances half a clip frame, so the left channel follows
        // clip[f / 2] with the in-between frames landing in the middle.
        for (u64 frame = k_ramp_frames; frame < 400; frame++)
        {
            INFO("frame " << frame);
            REQUIRE(out[frame * 2] == Catch::Approx(static_cast<f32>(frame) / 2.0f / 240.0f).margin(1e-5f));
        }
    }

    SECTION("Looping goes on past the end")
    {
        const AudioClipHandle ramp = CreateClipOrFail(mixer, MakeRampClip(k_rate, 100));
        const SoundHandle sound = mixer.Play(ramp, {.pan = -1.0f, .loop = true});
        const Opal::DynamicArray<f32> out = Mix(mixer, 1000);
        REQUIRE(mixer.IsPlaying(sound));
        for (u64 frame = k_ramp_frames; frame < 1000; frame++)
        {
            INFO("frame " << frame);
            REQUIRE(out[frame * 2] == Catch::Approx(static_cast<f32>(frame % 100) / 100.0f).margin(1e-5f));
        }
        mixer.SetLooping(sound, false);
        Mix(mixer, 100);
        REQUIRE_FALSE(mixer.IsPlaying(sound));
    }

    SECTION("Output is clamped")
    {
        const AudioClipHandle loud = CreateClipOrFail(mixer, AudioTest::MakeConstantClip(k_rate, 1, 1000, 0.8f));
        mixer.Play(loud, {.pan = -1.0f});
        mixer.Play(loud, {.pan = -1.0f});
        const Opal::DynamicArray<f32> out = Mix(mixer, 500);
        RequireFrames(out, k_ramp_frames, 500, 1.0f, 0.0f);
    }

    SECTION("Pan law is constant power")
    {
        mixer.Play(constant, {.pan = 0.5f});
        const Opal::DynamicArray<f32> out = Mix(mixer, 300);
        const f32 left = out[299 * 2] / 0.5f;
        const f32 right = out[299 * 2 + 1] / 0.5f;
        REQUIRE(left * left + right * right == Catch::Approx(1.0f).margin(1e-4f));
        REQUIRE(right > left);
    }
}

TEST_CASE("Mixer voice control", "[audio]")
{
    AudioMixer mixer(SmallDesc());
    const AudioClipHandle constant = CreateClipOrFail(mixer, AudioTest::MakeConstantClip(k_rate, 1, 100000, 0.5f));

    SECTION("Stop ramps to silence and retires the voice")
    {
        const SoundHandle sound = mixer.Play(constant, {.pan = -1.0f});
        Mix(mixer, 300);
        mixer.Stop(sound);
        REQUIRE(mixer.IsPlaying(sound));

        const Opal::DynamicArray<f32> out = Mix(mixer, 300);
        REQUIRE_FALSE(mixer.IsPlaying(sound));
        // Monotone descent, no step bigger than one ramp increment, silence after the ramp.
        const f32 max_step = 0.5f / static_cast<f32>(k_ramp_frames) + 1e-5f;
        for (u64 frame = 1; frame < 300; frame++)
        {
            INFO("frame " << frame);
            REQUIRE(out[frame * 2] <= out[(frame - 1) * 2] + 1e-6f);
            REQUIRE(out[(frame - 1) * 2] - out[frame * 2] <= max_step);
        }
        RequireFrames(out, k_ramp_frames + 1, 300, 0.0f, 0.0f);

        mixer.Stop(sound);  // stale: no-op
        mixer.SetVolume(sound, 0.0f);
        mixer.SetPan(sound, 0.0f);
        mixer.SetPitch(sound, 0.0f);
        mixer.SetLooping(sound, true);
        mixer.Pause(sound);
        mixer.Resume(sound);
        REQUIRE(mixer.GetDroppedCommandCount() == 0);
    }

    SECTION("StopAll stops everything")
    {
        const SoundHandle a = mixer.Play(constant);
        const SoundHandle b = mixer.Play(constant);
        Mix(mixer, 300);
        mixer.StopAll();
        Mix(mixer, 300);
        REQUIRE_FALSE(mixer.IsPlaying(a));
        REQUIRE_FALSE(mixer.IsPlaying(b));
        REQUIRE(mixer.GetActiveVoiceCount() == 0);
    }

    SECTION("Pause holds the position")
    {
        const AudioClipHandle ramp = CreateClipOrFail(mixer, MakeRampClip(k_rate, 1000));
        const SoundHandle sound = mixer.Play(ramp, {.pan = -1.0f});
        Mix(mixer, 200);

        mixer.Pause(sound);
        const Opal::DynamicArray<f32> paused = Mix(mixer, 100);
        REQUIRE(mixer.IsPlaying(sound));
        REQUIRE(mixer.GetActiveVoiceCount() == 1);
        RequireFrames(paused, 0, 100, 0.0f, 0.0f);

        mixer.Resume(sound);
        const Opal::DynamicArray<f32> resumed = Mix(mixer, 100);
        // Picks up at frame 200 of the clip, at full gain since the ramp finished before the pause.
        REQUIRE(resumed[0] == Catch::Approx(200.0f / 1000.0f).margin(1e-5f));
        REQUIRE(resumed[99 * 2] == Catch::Approx(299.0f / 1000.0f).margin(1e-5f));
    }

    SECTION("Pausing a stopping voice does not strand it")
    {
        const SoundHandle sound = mixer.Play(constant);
        Mix(mixer, 300);
        mixer.Stop(sound);
        mixer.Pause(sound);
        Mix(mixer, 300);
        REQUIRE_FALSE(mixer.IsPlaying(sound));
    }

    SECTION("Stopping a paused voice ends it at once")
    {
        const SoundHandle sound = mixer.Play(constant, {.start_paused = true});
        const Opal::DynamicArray<f32> out = Mix(mixer, 100);
        RequireFrames(out, 0, 100, 0.0f, 0.0f);
        REQUIRE(mixer.IsPlaying(sound));
        mixer.Stop(sound);
        Mix(mixer, 1);
        REQUIRE_FALSE(mixer.IsPlaying(sound));
    }

    SECTION("PauseAll and ResumeAll")
    {
        const SoundHandle a = mixer.Play(constant);
        const SoundHandle b = mixer.Play(constant);
        Mix(mixer, 300);
        mixer.PauseAll();
        const Opal::DynamicArray<f32> paused = Mix(mixer, 100);
        RequireFrames(paused, 0, 100, 0.0f, 0.0f);
        REQUIRE(mixer.IsPlaying(a));
        REQUIRE(mixer.IsPlaying(b));
        mixer.ResumeAll();
        const Opal::DynamicArray<f32> resumed = Mix(mixer, 100);
        RequireFrames(resumed, 0, 100, 2.0f * 0.5f * k_centre_gain, 2.0f * 0.5f * k_centre_gain);
    }

    SECTION("Volume and pan changes are ramped")
    {
        const SoundHandle sound = mixer.Play(constant, {.pan = -1.0f});
        Mix(mixer, 300);
        mixer.SetVolume(sound, 0.5f);
        const Opal::DynamicArray<f32> out = Mix(mixer, 300);
        REQUIRE(out[0] > 0.4f);  // still near the old gain
        RequireFrames(out, k_ramp_frames + 1, 300, 0.25f, 0.0f);

        mixer.SetPan(sound, 1.0f);
        const Opal::DynamicArray<f32> panned = Mix(mixer, 300);
        REQUIRE(panned[0] > 0.2f);
        RequireFrames(panned, k_ramp_frames + 1, 300, 0.0f, 0.25f);
    }

    SECTION("Pitch change takes effect")
    {
        const AudioClipHandle ramp = CreateClipOrFail(mixer, MakeRampClip(k_rate, 1000));
        const SoundHandle sound = mixer.Play(ramp, {.pan = -1.0f});
        Mix(mixer, 200);
        mixer.SetPitch(sound, 2.0f);
        const Opal::DynamicArray<f32> out = Mix(mixer, 100);
        REQUIRE(out[99 * 2] == Catch::Approx((200.0f + 99.0f * 2.0f) / 1000.0f).margin(1e-5f));
    }
}

TEST_CASE("Mixer voice slots", "[audio]")
{
    AudioMixer mixer({.sample_rate = k_rate, .max_voices = 2, .max_clips = 2, .command_queue_capacity = 64});
    const AudioClipHandle constant = CreateClipOrFail(mixer, AudioTest::MakeConstantClip(k_rate, 1, 1000, 0.5f));

    SECTION("No voice a sound may take means no sound")
    {
        const SoundHandle a = mixer.Play(constant, {.priority = 255});
        const SoundHandle b = mixer.Play(constant, {.priority = 255});
        REQUIRE(a.IsValid());
        REQUIRE(b.IsValid());
        REQUIRE_FALSE(mixer.Play(constant).IsValid());
        REQUIRE(mixer.GetStolenVoiceCount() == 0);

        Mix(mixer, 1100);  // both finish
        REQUIRE(mixer.Play(constant).IsValid());
    }

    SECTION("A stale handle does not reach the sound now in the slot")
    {
        const SoundHandle first = mixer.Play(constant);
        Mix(mixer, 1100);
        REQUIRE_FALSE(mixer.IsPlaying(first));

        // Fill both slots so one of them is the slot `first` used.
        const SoundHandle second = mixer.Play(constant, {.loop = true});
        const SoundHandle third = mixer.Play(constant, {.loop = true});
        const bool reused = second.index == first.index || third.index == first.index;
        REQUIRE(reused);
        REQUIRE(second != first);
        REQUIRE(third != first);

        mixer.Stop(first);
        Mix(mixer, 300);
        REQUIRE(mixer.IsPlaying(second));
        REQUIRE(mixer.IsPlaying(third));
    }

    SECTION("Invalid clip handle plays nothing")
    {
        REQUIRE_FALSE(mixer.Play(AudioClipHandle{}).IsValid());
        REQUIRE_FALSE(mixer.Play(AudioClipHandle{.index = 1, .generation = 1}).IsValid());
        REQUIRE_FALSE(mixer.IsPlaying(SoundHandle{}));
        REQUIRE_FALSE(mixer.IsPlaying(SoundHandle{.index = 50, .generation = 1}));
    }
}

TEST_CASE("Mixer voice stealing", "[audio]")
{
    AudioMixer mixer({.sample_rate = k_rate, .max_voices = 2, .max_clips = 2, .command_queue_capacity = 64});
    // Long enough that nothing ends on its own during a case.
    const AudioClipHandle constant = CreateClipOrFail(mixer, AudioTest::MakeConstantClip(k_rate, 1, 100000, 0.5f));

    SECTION("Equal priority loses the oldest")
    {
        const SoundHandle a = mixer.Play(constant);
        const SoundHandle b = mixer.Play(constant);
        const SoundHandle c = mixer.Play(constant);
        REQUIRE(c.IsValid());
        REQUIRE(c.index == a.index);
        REQUIRE_FALSE(mixer.IsPlaying(a));  // stale the moment the slot is taken, before any mix
        REQUIRE(mixer.IsPlaying(b));
        REQUIRE(mixer.IsPlaying(c));
        REQUIRE(mixer.GetStolenVoiceCount() == 1);
    }

    SECTION("Priority beats age")
    {
        const SoundHandle a = mixer.Play(constant, {.priority = 200});
        const SoundHandle b = mixer.Play(constant, {.priority = 50});
        const SoundHandle c = mixer.Play(constant, {.priority = 100});
        REQUIRE(c.index == b.index);
        REQUIRE(mixer.IsPlaying(a));
        REQUIRE_FALSE(mixer.IsPlaying(b));
    }

    SECTION("The quietest of equals goes")
    {
        const SoundHandle a = mixer.Play(constant, {.volume = 1.0f});
        const SoundHandle b = mixer.Play(constant, {.volume = 0.2f});
        const SoundHandle c = mixer.Play(constant);
        REQUIRE(c.index == b.index);
        REQUIRE(mixer.IsPlaying(a));
    }

    SECTION("Bus volume counts towards quietest")
    {
        mixer.SetBusVolume(1, 0.1f);
        const SoundHandle a = mixer.Play(constant, {.bus = 0});
        const SoundHandle b = mixer.Play(constant, {.bus = 1});
        const SoundHandle c = mixer.Play(constant);
        REQUIRE(c.index == b.index);
        REQUIRE(mixer.IsPlaying(a));
    }

    SECTION("A volume change moves who is quietest")
    {
        const SoundHandle a = mixer.Play(constant, {.volume = 0.2f});
        const SoundHandle b = mixer.Play(constant, {.volume = 1.0f});
        mixer.SetVolume(a, 1.0f);
        mixer.SetVolume(b, 0.1f);
        const SoundHandle c = mixer.Play(constant);
        REQUIRE(c.index == b.index);
        REQUIRE(mixer.IsPlaying(a));
    }

    SECTION("A higher priority is never taken")
    {
        const SoundHandle a = mixer.Play(constant, {.priority = 255});
        const SoundHandle b = mixer.Play(constant, {.priority = 255});
        REQUIRE_FALSE(mixer.Play(constant, {.priority = 100}).IsValid());
        REQUIRE(mixer.GetStolenVoiceCount() == 0);
        REQUIRE(mixer.IsPlaying(a));
        REQUIRE(mixer.IsPlaying(b));
        // Equal priority is fair game.
        REQUIRE(mixer.Play(constant, {.priority = 255}).IsValid());
    }

    SECTION("A sound on its way out goes first whatever its priority")
    {
        const SoundHandle a = mixer.Play(constant, {.priority = 255});
        const SoundHandle b = mixer.Play(constant, {.priority = 255});
        mixer.Stop(a);
        const SoundHandle c = mixer.Play(constant, {.priority = 10});
        REQUIRE(c.IsValid());
        REQUIRE(c.index == a.index);
        REQUIRE(mixer.IsPlaying(b));
    }

    SECTION("A paused sound is never taken")
    {
        const SoundHandle a = mixer.Play(constant);
        const SoundHandle b = mixer.Play(constant);
        mixer.Pause(a);
        const SoundHandle c = mixer.Play(constant);
        REQUIRE(c.index == b.index);
        REQUIRE(mixer.IsPlaying(a));

        mixer.Pause(c);
        REQUIRE_FALSE(mixer.Play(constant).IsValid());

        // Resuming puts it back in reach.
        mixer.Resume(a);
        REQUIRE(mixer.Play(constant).IsValid());
    }

    SECTION("A sound started paused is not taken either")
    {
        const SoundHandle a = mixer.Play(constant, {.start_paused = true});
        const SoundHandle b = mixer.Play(constant);
        const SoundHandle c = mixer.Play(constant);
        REQUIRE(c.index == b.index);
        REQUIRE(mixer.IsPlaying(a));
    }

    SECTION("The stolen handle reaches nothing")
    {
        const SoundHandle a = mixer.Play(constant, {.pan = -1.0f});
        const SoundHandle b = mixer.Play(constant, {.pan = -1.0f});
        const SoundHandle c = mixer.Play(constant, {.pan = -1.0f});
        REQUIRE(c.index == a.index);

        mixer.Stop(a);
        mixer.SetVolume(a, 0.0f);
        mixer.SetPitch(a, 4.0f);
        mixer.Pause(a);
        Mix(mixer, 500);
        REQUIRE(mixer.IsPlaying(b));
        REQUIRE(mixer.IsPlaying(c));
        REQUIRE(mixer.GetActiveVoiceCount() == 2);
        const Opal::DynamicArray<f32> out = Mix(mixer, 500);
        RequireFrames(out, 0, 500, 1.0f, 0.0f);  // two unity voices on the left, clamped
    }

    SECTION("The swap is a crossfade, not a click")
    {
        AudioMixer single({.sample_rate = k_rate, .max_voices = 1, .max_clips = 1});
        const AudioClipHandle clip = CreateClipOrFail(single, AudioTest::MakeConstantClip(k_rate, 1, 100000, 0.5f));
        single.Play(clip, {.pan = -1.0f});
        Mix(single, 500);  // the first sound is at full gain by now

        single.Play(clip, {.pan = -1.0f});
        const Opal::DynamicArray<f32> out = Mix(single, 500);
        // The sound leaving and the sound arriving ramp at the same rate over the same distance, so what comes out
        // never dips: the level holds through the swap and there is no step anywhere in it.
        RequireFrames(out, 0, 500, 0.5f, 0.0f, 2e-3f);
        REQUIRE(single.GetActiveVoiceCount() == 1);  // the ghost is nobody's voice
    }

    SECTION("A slot taken twice inside one ramp still ends up with the last sound")
    {
        AudioMixer single({.sample_rate = k_rate, .max_voices = 1, .max_clips = 1});
        const AudioClipHandle clip = CreateClipOrFail(single, AudioTest::MakeConstantClip(k_rate, 1, 100000, 0.5f));
        const SoundHandle a = single.Play(clip);
        const SoundHandle b = single.Play(clip);
        const SoundHandle c = single.Play(clip);
        REQUIRE(single.GetStolenVoiceCount() == 2);
        REQUIRE_FALSE(single.IsPlaying(a));
        REQUIRE_FALSE(single.IsPlaying(b));

        Mix(single, 500);
        REQUIRE(single.IsPlaying(c));
        REQUIRE(single.GetActiveVoiceCount() == 1);
    }

    SECTION("Destroying a clip takes the fading sounds with it")
    {
        AudioMixer single({.sample_rate = k_rate, .max_voices = 1, .max_clips = 2});
        const AudioClipHandle first = CreateClipOrFail(single, AudioTest::MakeConstantClip(k_rate, 1, 100000, 0.5f));
        single.Play(first);
        Mix(single, 500);
        single.Play(first);  // the first sound becomes a ghost on the next mix
        single.DestroyClip(first);
        // Two mixes of a handful of frames: without the ghost being finished too, the clip would be freed under it.
        Mix(single, 8);
        Mix(single, 8);
        const AudioClipHandle second = CreateClipOrFail(single, AudioTest::MakeConstantClip(k_rate, 1, 100, 0.5f));
        REQUIRE(second.IsValid());
        const Opal::DynamicArray<f32> out = Mix(single, 64);
        RequireFrames(out, 0, 64, 0.0f, 0.0f);
    }

    SECTION("A steal whose command is dropped leaves the sound alone")
    {
        AudioMixer single({.sample_rate = k_rate, .max_voices = 1, .max_clips = 1, .command_queue_capacity = 1});
        const AudioClipHandle clip = CreateClipOrFail(single, AudioTest::MakeConstantClip(k_rate, 1, 100000, 0.5f));
        const SoundHandle a = single.Play(clip, {.pan = -1.0f});
        REQUIRE(a.IsValid());

        const SoundHandle b = single.Play(clip);  // queue is full: the steal has to be undone
        REQUIRE_FALSE(b.IsValid());
        REQUIRE(single.GetStolenVoiceCount() == 0);
        REQUIRE(single.IsPlaying(a));

        const Opal::DynamicArray<f32> out = Mix(single, 500);
        RequireFrames(out, k_ramp_frames, 500, 0.5f, 0.0f);
        REQUIRE(single.IsPlaying(a));
    }
}

TEST_CASE("Mixer command queue", "[audio]")
{
    AudioMixer mixer({.sample_rate = k_rate, .max_voices = 4, .max_clips = 2, .command_queue_capacity = 1});
    const AudioClipHandle constant = CreateClipOrFail(mixer, AudioTest::MakeConstantClip(k_rate, 1, 1000, 0.5f));

    SECTION("A full queue drops the command and hands the voice back")
    {
        const SoundHandle first = mixer.Play(constant);
        REQUIRE(first.IsValid());
        const SoundHandle second = mixer.Play(constant);
        REQUIRE_FALSE(second.IsValid());
        REQUIRE(mixer.GetDroppedCommandCount() == 1);

        Mix(mixer, 100);
        REQUIRE(mixer.GetActiveVoiceCount() == 1);

        // The slot the dropped Play claimed is free again, and the one that went through is not.
        const SoundHandle third = mixer.Play(constant);
        REQUIRE(third.IsValid());
        REQUIRE(third.index != first.index);
        Mix(mixer, 100);
        REQUIRE(mixer.GetActiveVoiceCount() == 2);
    }

    SECTION("A full queue leaves a clip live")
    {
        mixer.Play(constant);
        mixer.DestroyClip(constant);
        REQUIRE(mixer.IsClipValid(constant));
        Mix(mixer, 10);
        mixer.DestroyClip(constant);
        REQUIRE_FALSE(mixer.IsClipValid(constant));
    }
}
