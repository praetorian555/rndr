#include "rndr/audio/audio-clip.hpp"

#include "opal/exceptions.h"

Rndr::AudioClip::AudioClip(u32 sample_rate, u32 channel_count, Opal::ArrayView<const f32> interleaved_frames)
    : m_sample_rate(sample_rate), m_channel_count(channel_count)
{
    if (sample_rate == 0)
    {
        throw Opal::Exception("AudioClip: sample rate must be greater than 0");
    }
    if (channel_count != 1 && channel_count != 2)
    {
        throw Opal::Exception(Opal::StringEx("AudioClip: channel count must be 1 or 2, got ") + static_cast<u64>(channel_count));
    }
    if (interleaved_frames.GetSize() == 0)
    {
        throw Opal::Exception("AudioClip: a clip needs at least one frame");
    }
    if (interleaved_frames.GetSize() % channel_count != 0)
    {
        throw Opal::Exception("AudioClip: sample count is not a whole number of frames");
    }
    m_frame_count = interleaved_frames.GetSize() / channel_count;
    m_data = Opal::DynamicArray<f32>(interleaved_frames.GetData(), interleaved_frames.GetSize());
}

Rndr::f64 Rndr::AudioClip::GetDurationSeconds() const
{
    return m_sample_rate == 0 ? 0.0 : static_cast<f64>(m_frame_count) / static_cast<f64>(m_sample_rate);
}
