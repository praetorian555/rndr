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
    return {.sample_rate = k_rate, .max_voices = 4, .max_clips = 2, .max_streams = 2, .command_queue_capacity = 64};
}

AudioStreamHandle CreateStreamOrFail(AudioMixer& mixer, const AudioStreamDesc& desc)
{
    Opal::Expected<AudioStreamHandle, ErrorCode> result = mixer.CreateStream(desc);
    if (!result.HasValue())
    {
        FAIL("expected a stream handle, got error code " << static_cast<u32>(result.GetError()));
    }
    return result.GetValue();
}

ErrorCode CreateStreamError(AudioMixer& mixer, const AudioStreamDesc& desc)
{
    Opal::Expected<AudioStreamHandle, ErrorCode> result = mixer.CreateStream(desc);
    if (result.HasValue())
    {
        FAIL("expected a failure, got a stream handle");
    }
    return result.GetError();
}

f64 Seconds(u64 frames)
{
    return static_cast<f64>(frames) / static_cast<f64>(k_rate);
}

/** Mixes @p frame_count frames as a device would that holds @p queued_frames ahead of them at @p time_frames. */
Opal::DynamicArray<f32> Mix(AudioMixer& mixer, u64 frame_count, u32 queued_frames = 0, u64 time_frames = 0)
{
    Opal::DynamicArray<f32> out(frame_count * 2, 123.0f);  // a value Mix has to overwrite
    mixer.Mix({out.GetData(), out.GetSize()}, {.queued_frames = queued_frames, .time_seconds = Seconds(time_frames)});
    return out;
}

/** Mono frames whose value is (first + i) / 100000, so the output says which frame of the stream it came from. */
Opal::DynamicArray<f32> CountingFrames(u64 first, u64 frame_count)
{
    Opal::DynamicArray<f32> samples(frame_count);
    for (u64 i = 0; i < frame_count; i++)
    {
        samples[i] = static_cast<f32>(first + i) / 100000.0f;
    }
    return samples;
}

u64 Write(AudioMixer& mixer, AudioStreamHandle stream, const Opal::DynamicArray<f32>& samples)
{
    return mixer.WriteStream(stream, {samples.GetData(), samples.GetSize()});
}

/** Asserts that frames [first, last) of the buffer carry stream frames from @p stream_frame on, centred. */
void RequireCounting(const Opal::DynamicArray<f32>& out, u64 first, u64 last, u64 stream_frame)
{
    for (u64 frame = first; frame < last; frame++)
    {
        INFO("frame " << frame);
        const f32 expected = static_cast<f32>(stream_frame + frame - first) / 100000.0f * k_centre_gain;
        REQUIRE(out[frame * 2] == Catch::Approx(expected).margin(1e-6f));
        REQUIRE(out[frame * 2 + 1] == Catch::Approx(expected).margin(1e-6f));
    }
}

void RequireSilence(const Opal::DynamicArray<f32>& out, u64 first, u64 last)
{
    for (u64 frame = first; frame < last; frame++)
    {
        INFO("frame " << frame);
        REQUIRE(out[frame * 2] == 0.0f);
        REQUIRE(out[frame * 2 + 1] == 0.0f);
    }
}

}  // namespace

TEST_CASE("Mixer streams", "[audio]")
{
    AudioMixer mixer(SmallDesc());

    SECTION("A stream has one or two channels")
    {
        REQUIRE(CreateStreamError(mixer, {.channel_count = 0}) == ErrorCode::InvalidArgument);
        REQUIRE(CreateStreamError(mixer, {.channel_count = 3}) == ErrorCode::InvalidArgument);
        REQUIRE(mixer.IsStreamValid(CreateStreamOrFail(mixer, {.channel_count = 1})));
        REQUIRE(mixer.IsStreamValid(CreateStreamOrFail(mixer, {.channel_count = 2})));
    }

    SECTION("Slots run out and come back once the audio thread is done")
    {
        const AudioStreamHandle a = CreateStreamOrFail(mixer, {});
        CreateStreamOrFail(mixer, {});
        REQUIRE(CreateStreamError(mixer, {}) == ErrorCode::OutOfResources);

        mixer.DestroyStream(a);
        REQUIRE_FALSE(mixer.IsStreamValid(a));
        REQUIRE(CreateStreamError(mixer, {}) == ErrorCode::OutOfResources);  // not before two mixes
        Mix(mixer, 16);
        Mix(mixer, 16);
        const AudioStreamHandle c = CreateStreamOrFail(mixer, {});
        REQUIRE(c.index == a.index);
        REQUIRE(c != a);
    }

    SECTION("A zero capacity takes half a second")
    {
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 2});
        REQUIRE(mixer.GetStreamFreeFrames(stream) == k_rate / 2);
    }

    SECTION("A stale handle reaches nothing")
    {
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 1});
        mixer.DestroyStream(stream);
        const Opal::DynamicArray<f32> samples = CountingFrames(0, 10);
        REQUIRE(Write(mixer, stream, samples) == 0);
        REQUIRE(mixer.GetStreamFreeFrames(stream) == 0);
        REQUIRE(mixer.GetStreamQueuedFrames(stream) == 0);
        REQUIRE_FALSE(mixer.PlayStream(stream).IsValid());
        REQUIRE_FALSE(mixer.PlayStream(AudioStreamHandle{}).IsValid());
        REQUIRE_FALSE(mixer.IsStreamValid(AudioStreamHandle{.index = 7, .generation = 1}));
    }
}

TEST_CASE("Mixer stream playback", "[audio]")
{
    AudioMixer mixer(SmallDesc());

    SECTION("Written frames come out in order, across the end of the ring")
    {
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 1, .capacity_frames = 250});
        const SoundHandle sound = mixer.PlayStream(stream);
        REQUIRE(sound.IsValid());

        // 160 frames at a time through a ring of 250, so the writes and the reads wrap at different places. Each mix
        // is longer than the gain ramp, which therefore lands inside the first one.
        u64 played = 0;
        for (u32 round = 0; round < 10; round++)
        {
            REQUIRE(Write(mixer, stream, CountingFrames(played, 160)) == 160);
            REQUIRE(mixer.GetStreamQueuedFrames(stream) == 160);
            const Opal::DynamicArray<f32> out = Mix(mixer, 160);
            const u64 first = round == 0 ? k_ramp_frames : 0;
            RequireCounting(out, first, 160, played + first);
            played += 160;
            REQUIRE(mixer.GetStreamQueuedFrames(stream) == 0);
        }
        REQUIRE(mixer.GetStreamUnderrunCount(stream) == 0);
        REQUIRE(mixer.IsPlaying(sound));
    }

    SECTION("A full ring takes only what fits")
    {
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 1, .capacity_frames = 100});
        REQUIRE(Write(mixer, stream, CountingFrames(0, 70)) == 70);
        REQUIRE(mixer.GetStreamFreeFrames(stream) == 30);
        REQUIRE(Write(mixer, stream, CountingFrames(70, 70)) == 30);
        REQUIRE(mixer.GetStreamFreeFrames(stream) == 0);
        REQUIRE(Write(mixer, stream, CountingFrames(100, 1)) == 0);

        mixer.PlayStream(stream);
        Mix(mixer, 60);
        REQUIRE(mixer.GetStreamFreeFrames(stream) == 60);
        REQUIRE(mixer.GetStreamQueuedFrames(stream) == 40);
    }

    SECTION("A stereo stream keeps its channels apart")
    {
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 2, .capacity_frames = 1000});
        Opal::DynamicArray<f32> samples(600);
        for (u64 frame = 0; frame < 300; frame++)
        {
            samples[frame * 2] = 0.25f;
            samples[frame * 2 + 1] = -0.5f;
        }
        REQUIRE(mixer.WriteStream(stream, {samples.GetData(), samples.GetSize()}) == 300);
        mixer.PlayStream(stream);
        const Opal::DynamicArray<f32> out = Mix(mixer, 300);
        for (u64 frame = k_ramp_frames; frame < 300; frame++)
        {
            INFO("frame " << frame);
            REQUIRE(out[frame * 2] == Catch::Approx(0.25f * k_centre_gain));
            REQUIRE(out[frame * 2 + 1] == Catch::Approx(-0.5f * k_centre_gain));
        }
    }

    SECTION("Running dry plays silence and counts once per dry spell")
    {
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 1, .capacity_frames = 1000});
        mixer.PlayStream(stream);
        Write(mixer, stream, CountingFrames(0, 300));

        Opal::DynamicArray<f32> out = Mix(mixer, 400);
        RequireCounting(out, k_ramp_frames, 300, k_ramp_frames);
        RequireSilence(out, 300, 400);
        REQUIRE(mixer.GetStreamUnderrunCount(stream) == 1);

        Mix(mixer, 400);
        Mix(mixer, 400);
        REQUIRE(mixer.GetStreamUnderrunCount(stream) == 1);

        // Back to playing where it left off, then dry again.
        Write(mixer, stream, CountingFrames(300, 500));
        out = Mix(mixer, 400);
        RequireCounting(out, 0, 400, 300);
        REQUIRE(mixer.GetStreamUnderrunCount(stream) == 1);
        Mix(mixer, 400);
        REQUIRE(mixer.GetStreamUnderrunCount(stream) == 2);
    }

    SECTION("Pausing holds the read position")
    {
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 1, .capacity_frames = 1000});
        const SoundHandle sound = mixer.PlayStream(stream, {.start_paused = true});
        Write(mixer, stream, CountingFrames(0, 500));

        RequireSilence(Mix(mixer, 200), 0, 200);
        REQUIRE(mixer.GetStreamQueuedFrames(stream) == 500);
        REQUIRE(mixer.GetStreamUnderrunCount(stream) == 0);

        mixer.Resume(sound);
        Mix(mixer, 200);
        REQUIRE(mixer.GetStreamQueuedFrames(stream) == 300);
        mixer.Pause(sound);
        Mix(mixer, 200);
        REQUIRE(mixer.GetStreamQueuedFrames(stream) == 300);
        mixer.Resume(sound);
        RequireCounting(Mix(mixer, 100), 0, 100, 200);
    }

    SECTION("A flush drops what was written and plays what comes after it")
    {
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 1, .capacity_frames = 1000});
        mixer.PlayStream(stream);
        Write(mixer, stream, CountingFrames(0, 800));
        Mix(mixer, 200);

        mixer.FlushStream(stream);
        REQUIRE(mixer.GetStreamQueuedFrames(stream) == 0);
        // The audio thread has not dropped them yet, so they still take room.
        REQUIRE(mixer.GetStreamFreeFrames(stream) == 400);
        REQUIRE(Write(mixer, stream, CountingFrames(50000, 300)) == 300);
        REQUIRE(mixer.GetStreamQueuedFrames(stream) == 300);

        RequireCounting(Mix(mixer, 300), 0, 300, 50000);
        REQUIRE(mixer.GetStreamFreeFrames(stream) == 1000);
        REQUIRE(mixer.GetStreamUnderrunCount(stream) == 0);
    }

    SECTION("A stream plays one sound at a time")
    {
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 1, .capacity_frames = 1000});
        Write(mixer, stream, CountingFrames(0, 1000));
        const SoundHandle first = mixer.PlayStream(stream);
        REQUIRE(first.IsValid());
        REQUIRE_FALSE(mixer.PlayStream(stream).IsValid());

        mixer.Stop(first);
        REQUIRE_FALSE(mixer.PlayStream(stream).IsValid());  // still ramping out
        Mix(mixer, 200);
        REQUIRE_FALSE(mixer.IsPlaying(first));
        const u64 queued = mixer.GetStreamQueuedFrames(stream);
        REQUIRE(queued >= 1000 - 200);

        // The next sound carries on from where the ramp left the stream.
        const SoundHandle second = mixer.PlayStream(stream);
        REQUIRE(second.IsValid());
        Mix(mixer, 200);
        REQUIRE(mixer.GetStreamQueuedFrames(stream) == queued - 200);
    }

    SECTION("Pitch and loop do not apply to a stream")
    {
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 1, .capacity_frames = 1000});
        Write(mixer, stream, CountingFrames(0, 300));
        mixer.PlayStream(stream, {.pitch = 2.0f, .loop = true});
        const Opal::DynamicArray<f32> out = Mix(mixer, 400);
        RequireCounting(out, k_ramp_frames, 300, k_ramp_frames);
        RequireSilence(out, 300, 400);
    }

    SECTION("Destroying a stream stops its sound")
    {
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 1, .capacity_frames = 1000});
        Write(mixer, stream, CountingFrames(0, 1000));
        const SoundHandle sound = mixer.PlayStream(stream);
        Mix(mixer, 100);
        mixer.DestroyStream(stream);
        RequireSilence(Mix(mixer, 100), 0, 100);
        REQUIRE_FALSE(mixer.IsPlaying(sound));
        REQUIRE(mixer.GetActiveVoiceCount() == 0);
    }

    SECTION("A stream and a clip mix together")
    {
        const AudioClipHandle clip = mixer.CreateClip(AudioTest::MakeConstantClip(k_rate, 1, 1000, 0.25f)).GetValue();
        const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 1, .capacity_frames = 1000});
        Write(mixer, stream, CountingFrames(0, 500));
        mixer.Play(clip);
        mixer.PlayStream(stream);
        const Opal::DynamicArray<f32> out = Mix(mixer, 400);
        for (u64 frame = k_ramp_frames; frame < 400; frame++)
        {
            INFO("frame " << frame);
            const f32 expected = (0.25f + static_cast<f32>(frame) / 100000.0f) * k_centre_gain;
            REQUIRE(out[frame * 2] == Catch::Approx(expected).margin(1e-6f));
        }
        REQUIRE(mixer.GetActiveVoiceCount() == 2);
    }
}

TEST_CASE("Mixer stream position", "[audio]")
{
    AudioMixer mixer(SmallDesc());
    const AudioStreamHandle stream = CreateStreamOrFail(mixer, {.channel_count = 1, .capacity_frames = 10000});

    SECTION("Before anything plays the position is the start")
    {
        Mix(mixer, 480, 1000, 0);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(5000)) == 0);
    }

    SECTION("The speaker is behind the mix by what the device holds")
    {
        Write(mixer, stream, CountingFrames(0, 10000));
        mixer.PlayStream(stream);

        // The device holds 1000 frames ahead of the first buffer, so the stream starts 1000 frames after the mix.
        Mix(mixer, 480, 1000, 0);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(0)) == 0);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(999)) == 0);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(1100)) == 100);
        // Never past what has been mixed.
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(5000)) == 480);

        // A steady device: every mix finds it holding the same amount.
        for (u64 mix = 1; mix < 10; mix++)
        {
            Mix(mixer, 480, 1000, mix * 480);
            REQUIRE(mixer.GetStreamPosition(stream, Seconds(mix * 480)) == mix * 480 - std::min<u64>(mix * 480, 1000));
            REQUIRE(mixer.GetStreamPosition(stream, Seconds(mix * 480 + 200)) == mix * 480 + 200 - std::min<u64>(mix * 480 + 200, 1000));
        }
    }

    SECTION("Without a device delay the position follows the mix")
    {
        Write(mixer, stream, CountingFrames(0, 10000));
        mixer.PlayStream(stream);
        Mix(mixer, 480, 0, 0);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(0)) == 0);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(240)) == 240);
        Mix(mixer, 480, 0, 480);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(480)) == 480);
    }

    SECTION("A pause plays out what the device holds and then holds")
    {
        Write(mixer, stream, CountingFrames(0, 10000));
        const SoundHandle sound = mixer.PlayStream(stream);
        for (u64 mix = 0; mix < 5; mix++)
        {
            Mix(mixer, 480, 1000, mix * 480);
        }
        // 2400 frames mixed; the speaker is at 1400 when the pause reaches the audio thread.
        mixer.Pause(sound);
        Mix(mixer, 480, 1000, 2400);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(2400)) == 1400);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(2900)) == 1900);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(3400)) == 2400);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(9000)) == 2400);
        for (u64 mix = 6; mix < 12; mix++)
        {
            Mix(mixer, 480, 1000, mix * 480);
            INFO("mix " << mix);
            REQUIRE(mixer.GetStreamPosition(stream, Seconds(mix * 480)) == std::min<u64>(mix * 480 - 1000, 2400));
            REQUIRE(mixer.GetStreamPosition(stream, Seconds(mix * 480 + 300)) == std::min<u64>(mix * 480 + 300 - 1000, 2400));
        }

        // Resumed at output frame 5760: heard 1000 frames later, from stream frame 2400 on.
        mixer.Resume(sound);
        Mix(mixer, 480, 1000, 5760);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(5760)) == 2400);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(6760)) == 2400);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(6860)) == 2500);
    }

    SECTION("A dry spell holds the position until the stream plays again")
    {
        const SoundHandle sound = mixer.PlayStream(stream);
        Write(mixer, stream, CountingFrames(0, 600));
        Mix(mixer, 480, 0, 0);
        Mix(mixer, 480, 0, 480);  // 120 frames in, dry after them
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(480)) == 480);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(1000)) == 600);
        Mix(mixer, 480, 0, 960);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(1200)) == 600);

        Write(mixer, stream, CountingFrames(600, 1000));
        Mix(mixer, 480, 0, 1440);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(1440)) == 600);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(1640)) == 800);
        REQUIRE(mixer.IsPlaying(sound));
    }

    SECTION("A flush moves the position on by what it dropped")
    {
        mixer.PlayStream(stream);
        Write(mixer, stream, CountingFrames(0, 5000));
        Mix(mixer, 480, 0, 0);
        mixer.FlushStream(stream);
        Write(mixer, stream, CountingFrames(5000, 1000));
        Mix(mixer, 480, 0, 480);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(480)) == 5000);
        REQUIRE(mixer.GetStreamPosition(stream, Seconds(700)) == 5220);
    }
}
