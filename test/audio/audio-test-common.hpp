#pragma once

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"

#include "rndr/audio/audio-clip.hpp"
#include "rndr/types.hpp"

/**
 * What the audio test files share: a sine clip, a WAV file image built from a sample array, and the environment flag
 * that turns a skipped device test into a failing one. None of it touches a device.
 */
namespace AudioTest
{

constexpr Rndr::f32 k_pi = 3.14159265358979323846f;

/**
 * A sine at the given frequency. The same waveform in every channel, so a mixer case can compare output channels
 * against one another.
 */
inline Rndr::AudioClip MakeSineClip(Rndr::u32 sample_rate, Rndr::u32 channel_count, Rndr::f32 frequency, Rndr::u64 frame_count,
                                    Rndr::f32 amplitude = 0.5f)
{
    using namespace Rndr;
    Opal::DynamicArray<f32> samples(frame_count * channel_count);
    for (u64 frame = 0; frame < frame_count; frame++)
    {
        const f32 value = amplitude * std::sin(2.0f * k_pi * frequency * static_cast<f32>(frame) / static_cast<f32>(sample_rate));
        for (u32 channel = 0; channel < channel_count; channel++)
        {
            samples[frame * channel_count + channel] = value;
        }
    }
    return AudioClip::Create(sample_rate, channel_count, {samples.GetData(), samples.GetSize()}).GetValue();
}

/** A clip whose every sample is the same value, for cases that want to read a gain straight off the output. */
inline Rndr::AudioClip MakeConstantClip(Rndr::u32 sample_rate, Rndr::u32 channel_count, Rndr::u64 frame_count, Rndr::f32 value)
{
    using namespace Rndr;
    Opal::DynamicArray<f32> samples(frame_count * channel_count, value);
    return AudioClip::Create(sample_rate, channel_count, {samples.GetData(), samples.GetSize()}).GetValue();
}

constexpr Rndr::u16 k_wav_pcm = 0x0001;
constexpr Rndr::u16 k_wav_float = 0x0003;

struct WavSpec
{
    Rndr::u16 format_tag = k_wav_pcm;
    Rndr::u16 channel_count = 1;
    Rndr::u32 sample_rate = 44100;
    Rndr::u16 bits_per_sample = 16;
    /** Wrap the format in a WAVEFORMATEXTENSIBLE header, as most tools do for anything beyond 16-bit stereo. */
    bool extensible = false;
    /** Put a LIST chunk between fmt and data, as files with metadata have. */
    bool list_chunk_before_data = false;
};

namespace Impl
{

inline void PutU16(Opal::DynamicArray<Rndr::u8>& out, Rndr::u16 value)
{
    out.PushBack(static_cast<Rndr::u8>(value & 0xFF));
    out.PushBack(static_cast<Rndr::u8>((value >> 8) & 0xFF));
}

inline void PutU32(Opal::DynamicArray<Rndr::u8>& out, Rndr::u32 value)
{
    for (int i = 0; i < 4; i++)
    {
        out.PushBack(static_cast<Rndr::u8>((value >> (8 * i)) & 0xFF));
    }
}

inline void PutTag(Opal::DynamicArray<Rndr::u8>& out, const char* tag)
{
    for (int i = 0; i < 4; i++)
    {
        out.PushBack(static_cast<Rndr::u8>(tag[i]));
    }
}

inline void PutSample(Opal::DynamicArray<Rndr::u8>& out, const WavSpec& spec, Rndr::f32 sample)
{
    using namespace Rndr;
    if (spec.format_tag == k_wav_float)
    {
        u32 bits = 0;
        std::memcpy(&bits, &sample, 4);
        PutU32(out, bits);
        return;
    }
    switch (spec.bits_per_sample)
    {
        case 8:
            out.PushBack(static_cast<u8>(128.0f + sample * 127.0f));
            break;
        case 16:
            PutU16(out, static_cast<u16>(static_cast<i16>(sample * 32767.0f)));
            break;
        case 24:
        {
            const auto value = static_cast<i32>(sample * 8388607.0f);
            out.PushBack(static_cast<u8>(value & 0xFF));
            out.PushBack(static_cast<u8>((value >> 8) & 0xFF));
            out.PushBack(static_cast<u8>((value >> 16) & 0xFF));
            break;
        }
        case 32:
            PutU32(out, static_cast<u32>(static_cast<i32>(static_cast<f64>(sample) * 2147483647.0)));
            break;
        default:
            break;
    }
}

}  // namespace Impl

/**
 * Writes a WAV file image holding @p samples (interleaved, in [-1, 1]) in the layout @p spec asks for. The sizes in
 * the headers are correct, so a case that wants a malformed file edits the result.
 */
inline Opal::DynamicArray<Rndr::u8> BuildWav(const WavSpec& spec, Opal::ArrayView<const Rndr::f32> samples)
{
    using namespace Rndr;
    Opal::DynamicArray<u8> out;

    const u16 bytes_per_frame = static_cast<u16>(spec.channel_count * (spec.bits_per_sample / 8));
    const u32 data_size = static_cast<u32>(samples.GetSize() * (spec.bits_per_sample / 8));
    const u32 fmt_size = spec.extensible ? 40 : 16;
    const u32 list_size = spec.list_chunk_before_data ? 12 : 0;
    const u32 riff_size = 4 + (8 + fmt_size) + (spec.list_chunk_before_data ? 8 + list_size : 0) + (8 + data_size) + (data_size & 1);

    Impl::PutTag(out, "RIFF");
    Impl::PutU32(out, riff_size);
    Impl::PutTag(out, "WAVE");

    Impl::PutTag(out, "fmt ");
    Impl::PutU32(out, fmt_size);
    Impl::PutU16(out, spec.extensible ? static_cast<u16>(0xFFFE) : spec.format_tag);
    Impl::PutU16(out, spec.channel_count);
    Impl::PutU32(out, spec.sample_rate);
    Impl::PutU32(out, spec.sample_rate * bytes_per_frame);
    Impl::PutU16(out, bytes_per_frame);
    Impl::PutU16(out, spec.bits_per_sample);
    if (spec.extensible)
    {
        Impl::PutU16(out, 22);                                   // cbSize
        Impl::PutU16(out, spec.bits_per_sample);                 // valid bits
        Impl::PutU32(out, spec.channel_count == 2 ? 0x3 : 0x4);  // channel mask
        Impl::PutU16(out, spec.format_tag);                      // sub-format GUID, first two bytes carry the tag
        Impl::PutU16(out, 0x0000);
        Impl::PutU16(out, 0x0010);
        Impl::PutU16(out, 0x8000);
        Impl::PutU16(out, 0xAA00);
        Impl::PutU16(out, 0x3800);
        Impl::PutU16(out, 0x9B71);
        Impl::PutU16(out, 0x0000);
    }

    if (spec.list_chunk_before_data)
    {
        Impl::PutTag(out, "LIST");
        Impl::PutU32(out, list_size);
        Impl::PutTag(out, "INFO");
        Impl::PutTag(out, "ISFT");
        Impl::PutU32(out, 0);
    }

    Impl::PutTag(out, "data");
    Impl::PutU32(out, data_size);
    for (const f32 sample : samples)
    {
        Impl::PutSample(out, spec, sample);
    }
    if ((data_size & 1) != 0)
    {
        out.PushBack(0);
    }
    return out;
}

/**
 * Whether an environment variable is set to anything but "0". _dupenv_s rather than getenv, which MSVC deprecates
 * and this build turns into an error.
 */
inline bool IsEnvironmentFlagSet(const char* name)
{
    bool is_set = false;
#if defined(_MSC_VER)
    char* value = nullptr;
    size_t size = 0;
    if (_dupenv_s(&value, &size, name) == 0 && value != nullptr)
    {
        is_set = value[0] != '0';
        free(value);
    }
#else
    const char* value = std::getenv(name);
    is_set = value != nullptr && value[0] != '0';
#endif
    return is_set;
}

}  // namespace AudioTest
