#include <catch2/catch2.hpp>

#include <algorithm>
#include <cmath>

#include "opal/paths.h"

#include "rndr/audio/audio-clip.hpp"
#include "rndr/error-codes.hpp"
#include "rndr/file.hpp"

#include "audio-test-common.hpp"

using namespace Rndr;

namespace
{

/** A ramp through the whole range, so every bit depth gets exercised at both ends and around zero. */
Opal::DynamicArray<f32> MakeRamp(u64 sample_count)
{
    Opal::DynamicArray<f32> samples(sample_count);
    for (u64 i = 0; i < sample_count; i++)
    {
        samples[i] = -1.0f + 2.0f * static_cast<f32>(i) / static_cast<f32>(sample_count - 1);
    }
    // Full-scale negative is representable; full-scale positive is one step short of it in integer formats.
    samples[sample_count - 1] = 0.999f;
    return samples;
}

f32 Rms(Opal::ArrayView<const f32> samples)
{
    f64 sum = 0.0;
    for (const f32 sample : samples)
    {
        sum += static_cast<f64>(sample) * sample;
    }
    return static_cast<f32>(std::sqrt(sum / static_cast<f64>(samples.GetSize())));
}

/** Unwraps a decode that is meant to succeed, and fails the case with the code when it did not. */
AudioClip RequireClip(Opal::Expected<AudioClip, ErrorCode> result)
{
    if (!result.HasValue())
    {
        FAIL("expected a clip, got error code " << static_cast<u32>(result.GetError()));
    }
    return std::move(result.GetValue());
}

/** The code a decode was expected to fail with. */
ErrorCode ErrorOf(Opal::Expected<AudioClip, ErrorCode> result)
{
    if (result.HasValue())
    {
        FAIL("expected a failure, got a clip");
    }
    return result.GetError();
}

void RequireClose(Opal::ArrayView<const f32> actual, Opal::ArrayView<const f32> expected, f32 tolerance)
{
    REQUIRE(actual.GetSize() == expected.GetSize());
    for (u64 i = 0; i < actual.GetSize(); i++)
    {
        INFO("sample " << i);
        REQUIRE(actual[i] == Catch::Approx(expected[i]).margin(tolerance));
    }
}

}  // namespace

TEST_CASE("Audio clip", "[audio]")
{
    SECTION("Default is empty")
    {
        const AudioClip clip;
        REQUIRE_FALSE(clip.IsValid());
        REQUIRE(clip.GetFrameCount() == 0);
        REQUIRE(clip.GetDurationSeconds() == 0.0);
    }

    SECTION("Stores what it was given")
    {
        const f32 samples[] = {0.1f, 0.2f, 0.3f, 0.4f};
        const AudioClip clip = RequireClip(AudioClip::Create(1000, 2, samples));
        REQUIRE(clip.IsValid());
        REQUIRE(clip.GetSampleRate() == 1000);
        REQUIRE(clip.GetChannelCount() == 2);
        REQUIRE(clip.GetFrameCount() == 2);
        REQUIRE(clip.GetDurationSeconds() == Catch::Approx(0.002));
        REQUIRE(clip.GetData().GetSize() == 4);
        REQUIRE(clip.GetData()[3] == 0.4f);
    }

    SECTION("Rejects bad arguments, saying which kind")
    {
        const f32 samples[] = {0.1f, 0.2f, 0.3f};
        REQUIRE(ErrorOf(AudioClip::Create(0, 1, samples)) == ErrorCode::InvalidArgument);
        REQUIRE(ErrorOf(AudioClip::Create(1000, 1, Opal::ArrayView<const f32>{})) == ErrorCode::InvalidArgument);
        // Three samples is not a whole number of stereo frames.
        REQUIRE(ErrorOf(AudioClip::Create(1000, 2, samples)) == ErrorCode::InvalidArgument);
        // A channel count nobody here handles is a property of the data rather than of the call.
        REQUIRE(ErrorOf(AudioClip::Create(1000, 0, samples)) == ErrorCode::UnsupportedFormat);
        REQUIRE(ErrorOf(AudioClip::Create(1000, 3, samples)) == ErrorCode::UnsupportedFormat);
    }

    SECTION("Move empties the source")
    {
        const f32 samples[] = {0.1f, 0.2f};
        AudioClip source = RequireClip(AudioClip::Create(1000, 1, samples));
        const AudioClip moved(std::move(source));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetFrameCount() == 2);
    }
}

TEST_CASE("WAV decoding", "[audio]")
{
    const Opal::DynamicArray<f32> ramp = MakeRamp(64);
    const Opal::ArrayView<const f32> ramp_view{ramp.GetData(), ramp.GetSize()};

    SECTION("PCM 16-bit mono")
    {
        const AudioTest::WavSpec spec{.channel_count = 1, .sample_rate = 44100, .bits_per_sample = 16};
        const Opal::DynamicArray<u8> file = AudioTest::BuildWav(spec, ramp_view);
        const AudioClip clip = RequireClip(File::DecodeAudioClip({file.GetData(), file.GetSize()}, AudioFileFormat::Wav));
        REQUIRE(clip.GetSampleRate() == 44100);
        REQUIRE(clip.GetChannelCount() == 1);
        REQUIRE(clip.GetFrameCount() == 64);
        RequireClose(clip.GetData(), ramp_view, 2.0f / 32768.0f);
    }

    SECTION("PCM 16-bit stereo")
    {
        const AudioTest::WavSpec spec{.channel_count = 2, .sample_rate = 48000, .bits_per_sample = 16};
        const Opal::DynamicArray<u8> file = AudioTest::BuildWav(spec, ramp_view);
        const AudioClip clip = RequireClip(File::DecodeAudioClip({file.GetData(), file.GetSize()}, AudioFileFormat::Wav));
        REQUIRE(clip.GetSampleRate() == 48000);
        REQUIRE(clip.GetChannelCount() == 2);
        REQUIRE(clip.GetFrameCount() == 32);
        RequireClose(clip.GetData(), ramp_view, 2.0f / 32768.0f);
    }

    SECTION("PCM 8-bit is unsigned")
    {
        const AudioTest::WavSpec spec{.bits_per_sample = 8};
        const Opal::DynamicArray<u8> file = AudioTest::BuildWav(spec, ramp_view);
        const AudioClip clip = RequireClip(File::DecodeAudioClip({file.GetData(), file.GetSize()}, AudioFileFormat::Wav));
        RequireClose(clip.GetData(), ramp_view, 2.0f / 128.0f);
    }

    SECTION("PCM 24-bit")
    {
        const AudioTest::WavSpec spec{.bits_per_sample = 24};
        const Opal::DynamicArray<u8> file = AudioTest::BuildWav(spec, ramp_view);
        const AudioClip clip = RequireClip(File::DecodeAudioClip({file.GetData(), file.GetSize()}, AudioFileFormat::Wav));
        RequireClose(clip.GetData(), ramp_view, 2.0f / 8388608.0f);
    }

    SECTION("PCM 32-bit")
    {
        const AudioTest::WavSpec spec{.bits_per_sample = 32};
        const Opal::DynamicArray<u8> file = AudioTest::BuildWav(spec, ramp_view);
        const AudioClip clip = RequireClip(File::DecodeAudioClip({file.GetData(), file.GetSize()}, AudioFileFormat::Wav));
        RequireClose(clip.GetData(), ramp_view, 2.0f / 8388608.0f);
    }

    SECTION("IEEE float is passed through")
    {
        const AudioTest::WavSpec spec{.format_tag = AudioTest::k_wav_float, .bits_per_sample = 32};
        const Opal::DynamicArray<u8> file = AudioTest::BuildWav(spec, ramp_view);
        const AudioClip clip = RequireClip(File::DecodeAudioClip({file.GetData(), file.GetSize()}, AudioFileFormat::Wav));
        RequireClose(clip.GetData(), ramp_view, 0.0f);
    }

    SECTION("Extensible header wrapping PCM and float")
    {
        const AudioTest::WavSpec pcm_spec{.channel_count = 2, .bits_per_sample = 24, .extensible = true};
        const Opal::DynamicArray<u8> pcm_file = AudioTest::BuildWav(pcm_spec, ramp_view);
        const AudioClip pcm_clip = RequireClip(File::DecodeAudioClip({pcm_file.GetData(), pcm_file.GetSize()}, AudioFileFormat::Wav));
        REQUIRE(pcm_clip.GetChannelCount() == 2);
        RequireClose(pcm_clip.GetData(), ramp_view, 2.0f / 8388608.0f);

        const AudioTest::WavSpec float_spec{.format_tag = AudioTest::k_wav_float, .bits_per_sample = 32, .extensible = true};
        const Opal::DynamicArray<u8> float_file = AudioTest::BuildWav(float_spec, ramp_view);
        const AudioClip float_clip = RequireClip(File::DecodeAudioClip({float_file.GetData(), float_file.GetSize()}, AudioFileFormat::Wav));
        RequireClose(float_clip.GetData(), ramp_view, 0.0f);
    }

    SECTION("Unknown chunks between fmt and data are skipped")
    {
        const AudioTest::WavSpec spec{.list_chunk_before_data = true};
        const Opal::DynamicArray<u8> file = AudioTest::BuildWav(spec, ramp_view);
        const AudioClip clip = RequireClip(File::DecodeAudioClip({file.GetData(), file.GetSize()}, AudioFileFormat::Wav));
        REQUIRE(clip.GetFrameCount() == 64);
    }

    SECTION("Odd-sized data chunk keeps its pad byte out of the samples")
    {
        const AudioTest::WavSpec spec{.bits_per_sample = 8};
        const f32 samples[] = {0.0f, 0.5f, -0.5f};
        const Opal::DynamicArray<u8> file = AudioTest::BuildWav(spec, samples);
        REQUIRE(file.GetSize() % 2 == 0);
        const AudioClip clip = RequireClip(File::DecodeAudioClip({file.GetData(), file.GetSize()}, AudioFileFormat::Wav));
        REQUIRE(clip.GetFrameCount() == 3);
    }

    SECTION("Truncated data chunk throws")
    {
        const AudioTest::WavSpec spec{};
        const Opal::DynamicArray<u8> file = AudioTest::BuildWav(spec, ramp_view);
        const Opal::ArrayView<const u8> truncated{file.GetData(), file.GetSize() - 10};
        REQUIRE(ErrorOf(File::DecodeAudioClip(truncated, AudioFileFormat::Wav)) == ErrorCode::CorruptData);
    }

    SECTION("Not a RIFF file throws")
    {
        const u8 garbage[] = {'O', 'g', 'g', 'S', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        REQUIRE(ErrorOf(File::DecodeAudioClip(garbage, AudioFileFormat::Wav)) == ErrorCode::CorruptData);
        REQUIRE(ErrorOf(File::DecodeAudioClip(Opal::ArrayView<const u8>{}, AudioFileFormat::Wav)) == ErrorCode::CorruptData);
    }

    SECTION("A layout no decoder here handles is unsupported rather than corrupt")
    {
        const AudioTest::WavSpec four_channels{.channel_count = 4};
        const Opal::DynamicArray<u8> four_channel_file = AudioTest::BuildWav(four_channels, ramp_view);
        const Opal::ArrayView<const u8> four_channel_view{four_channel_file.GetData(), four_channel_file.GetSize()};
        REQUIRE(ErrorOf(File::DecodeAudioClip(four_channel_view, AudioFileFormat::Wav)) == ErrorCode::UnsupportedFormat);

        const AudioTest::WavSpec mp3_in_wav{.format_tag = 0x0055};
        const Opal::DynamicArray<u8> mp3_file = AudioTest::BuildWav(mp3_in_wav, ramp_view);
        const Opal::ArrayView<const u8> mp3_view{mp3_file.GetData(), mp3_file.GetSize()};
        REQUIRE(ErrorOf(File::DecodeAudioClip(mp3_view, AudioFileFormat::Wav)) == ErrorCode::UnsupportedFormat);

        // The builder writes nothing for 12-bit samples, so hand it none and let the header alone fail.
        const AudioTest::WavSpec twelve_bit{.bits_per_sample = 12};
        const Opal::DynamicArray<u8> twelve_bit_file = AudioTest::BuildWav(twelve_bit, Opal::ArrayView<const f32>{});
        const Opal::ArrayView<const u8> twelve_bit_view{twelve_bit_file.GetData(), twelve_bit_file.GetSize()};
        REQUIRE(ErrorOf(File::DecodeAudioClip(twelve_bit_view, AudioFileFormat::Wav)) == ErrorCode::UnsupportedFormat);
    }
}

TEST_CASE("OGG decoding", "[audio]")
{
    SECTION("Decodes the fixture tone")
    {
        // assets/audio/test-tone.ogg: 1 second of a 0.5 amplitude 440 Hz sine, mono, 22050 Hz.
        const Opal::StringUtf8 path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "audio", "test-tone.ogg").GetValue();
        const AudioClip clip = RequireClip(File::LoadAudioClip(path));
        REQUIRE(clip.IsValid());
        REQUIRE(clip.GetSampleRate() == 22050);
        REQUIRE(clip.GetChannelCount() == 1);
        REQUIRE(clip.GetFrameCount() == Catch::Approx(22050).margin(22050 * 0.02));
        // A sine of amplitude a has an RMS of a / sqrt(2); lossy coding moves it a little.
        REQUIRE(Rms(clip.GetData()) == Catch::Approx(0.5f / std::sqrt(2.0f)).margin(0.03f));
        f32 peak = 0.0f;
        for (const f32 sample : clip.GetData())
        {
            peak = std::max(peak, std::abs(sample));
        }
        REQUIRE(peak < 0.6f);
    }

    SECTION("Garbage is corrupt data")
    {
        const u8 garbage[] = {'R', 'I', 'F', 'F', 1, 2, 3, 4, 'W', 'A', 'V', 'E', 0, 0, 0, 0};
        REQUIRE(ErrorOf(File::DecodeAudioClip(garbage, AudioFileFormat::OggVorbis)) == ErrorCode::CorruptData);
        REQUIRE(ErrorOf(File::DecodeAudioClip(Opal::ArrayView<const u8>{}, AudioFileFormat::OggVorbis)) == ErrorCode::CorruptData);
    }
}

TEST_CASE("Loading audio from disk", "[audio]")
{
    SECTION("A missing file is reported as missing")
    {
        REQUIRE(ErrorOf(File::LoadAudioClip("does-not-exist.wav")) == ErrorCode::FileNotFound);
    }

    SECTION("An extension no decoder claims is unsupported")
    {
        const Opal::StringUtf8 path = Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "default-texture.png").GetValue();
        REQUIRE(ErrorOf(File::LoadAudioClip(path)) == ErrorCode::UnsupportedFormat);
    }
}
