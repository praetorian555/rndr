#include "rndr/audio/audio-clip.hpp"

#include "rndr/log.hpp"

Rndr::AudioClip::AudioClip(u32 sample_rate, u32 channel_count, Opal::ArrayView<const f32> interleaved_frames)
    : m_sample_rate(sample_rate),
      m_channel_count(channel_count),
      m_frame_count(interleaved_frames.GetSize() / channel_count),
      m_data(interleaved_frames.GetData(), interleaved_frames.GetSize())
{
}

Opal::Expected<Rndr::AudioClip, Rndr::ErrorCode> Rndr::AudioClip::Create(u32 sample_rate, u32 channel_count,
                                                                         Opal::ArrayView<const f32> interleaved_frames)
{
    using Result = Opal::Expected<AudioClip, ErrorCode>;
    if (sample_rate == 0)
    {
        RNDR_LOG_ERROR("AudioClip: sample rate must be greater than 0");
        return Result(ErrorCode::InvalidArgument);
    }
    if (channel_count != 1 && channel_count != 2)
    {
        RNDR_LOG_ERROR("AudioClip: channel count must be 1 or 2, got {}", channel_count);
        return Result(ErrorCode::UnsupportedFormat);
    }
    if (interleaved_frames.GetSize() == 0)
    {
        RNDR_LOG_ERROR("AudioClip: a clip needs at least one frame");
        return Result(ErrorCode::InvalidArgument);
    }
    if (interleaved_frames.GetSize() % channel_count != 0)
    {
        RNDR_LOG_ERROR("AudioClip: sample count is not a whole number of frames");
        return Result(ErrorCode::InvalidArgument);
    }
    return Result(AudioClip(sample_rate, channel_count, interleaved_frames));
}

Rndr::f64 Rndr::AudioClip::GetDurationSeconds() const
{
    return m_sample_rate == 0 ? 0.0 : static_cast<f64>(m_frame_count) / static_cast<f64>(m_sample_rate);
}
