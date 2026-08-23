#pragma once

#include "rndr/types.hpp"

namespace Rndr
{

/** Number of volume buses a sound can be routed through. Bus 0 is the default; what the rest mean is up to the game. */
constexpr u8 k_audio_bus_count = 8;

/**
 * Names a playing sound. Generational: once the sound finishes or is stopped, the handle goes stale and every call
 * that takes it becomes a no-op, so a game can keep one around without checking whether the sound is still there.
 */
struct SoundHandle
{
    u32 index = 0;
    u32 generation = 0;

    [[nodiscard]] bool IsValid() const { return generation != 0; }
    bool operator==(const SoundHandle& other) const = default;
};

/** Names a clip owned by an AudioSystem. Generational in the same way as SoundHandle. */
struct AudioClipHandle
{
    u32 index = 0;
    u32 generation = 0;

    [[nodiscard]] bool IsValid() const { return generation != 0; }
    bool operator==(const AudioClipHandle& other) const = default;
};

enum class AudioFileFormat : u8
{
    Wav,
    OggVorbis,
};

struct PlaySoundDesc
{
    /** Linear gain. 1 is unity; values above it are allowed and clipped at the output if they overflow. */
    f32 volume = 1.0f;
    /** Stereo position in [-1, 1]: -1 is hard left, 0 is centre, 1 is hard right. Acts as balance on stereo clips. */
    f32 pan = 0.0f;
    /** Playback rate multiplier. 1 plays the clip at its own sample rate; 2 plays it twice as fast and an octave up. */
    f32 pitch = 1.0f;
    bool loop = false;
    /** Allocate the voice but do not advance it until Resume. */
    bool start_paused = false;
    /**
     * Who wins a voice when every one of them is busy. A sound takes a voice from one of equal or lower priority,
     * never from a higher one, and is refused when there is nothing it may take.
     */
    u8 priority = 128;
    /** Bus in [0, k_audio_bus_count) whose volume is applied on top of the sound's own. */
    u8 bus = 0;
};

}  // namespace Rndr
