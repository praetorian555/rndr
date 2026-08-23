#pragma once

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"

#include "rndr/types.hpp"

namespace Rndr
{

/**
 * Decoded audio in CPU memory: interleaved 32-bit float frames, one or two channels, at a fixed sample rate. The
 * audio equivalent of Bitmap - plain data that an AudioSystem takes ownership of to play.
 *
 * Move-only, like the array it holds. A default-constructed clip is empty and IsValid() is false.
 */
class AudioClip
{
public:
    AudioClip() = default;

    /**
     * @param sample_rate Frames per second. Must be greater than 0.
     * @param channel_count 1 for mono, 2 for stereo. Anything else throws.
     * @param interleaved_frames Samples in [-1, 1], interleaved by channel. Its size must be a whole number of frames
     *        and there must be at least one. Copied.
     * @throw Opal::Exception on any argument that does not meet the above.
     */
    AudioClip(u32 sample_rate, u32 channel_count, Opal::ArrayView<const f32> interleaved_frames);

    AudioClip(const AudioClip&) = delete;
    AudioClip& operator=(const AudioClip&) = delete;
    AudioClip(AudioClip&&) noexcept = default;
    AudioClip& operator=(AudioClip&&) noexcept = default;

    [[nodiscard]] bool IsValid() const { return m_frame_count > 0; }

    [[nodiscard]] u32 GetSampleRate() const { return m_sample_rate; }
    [[nodiscard]] u32 GetChannelCount() const { return m_channel_count; }
    [[nodiscard]] u64 GetFrameCount() const { return m_frame_count; }
    [[nodiscard]] f64 GetDurationSeconds() const;

    /** Interleaved samples; GetFrameCount() * GetChannelCount() of them. */
    [[nodiscard]] Opal::ArrayView<const f32> GetData() const { return {m_data.GetData(), m_data.GetSize()}; }

private:
    u32 m_sample_rate = 0;
    u32 m_channel_count = 0;
    u64 m_frame_count = 0;
    Opal::DynamicArray<f32> m_data;
};

}  // namespace Rndr
