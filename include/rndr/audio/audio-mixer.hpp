#pragma once

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/threading/atomic.h"
#include "opal/threading/channel-spsc.h"

#include "rndr/audio/audio-clip.hpp"
#include "rndr/audio/audio-types.hpp"

namespace Rndr
{

struct AudioMixerDesc
{
    /** Rate of the buffers Mix fills. Clips at any other rate are resampled on the way through. */
    u32 sample_rate = 48000;
    /** How many sounds can play at once. Play returns an invalid handle once they are all busy. */
    u32 max_voices = 64;
    /** How many clips can be live at once. CreateClip throws once they are all taken. */
    u32 max_clips = 256;
    /** How many calls the main thread can make between two Mix calls before they start being dropped. */
    u32 command_queue_capacity = 1024;
};

/**
 * The engine under AudioSystem: owns the clips and the voices, takes commands from the main thread and fills
 * interleaved stereo f32 buffers from the audio thread. It has no thread of its own and never touches a device,
 * which is what makes it testable - call Mix yourself and look at the buffer.
 *
 * Two threads use it and the split is strict. Everything except Mix is for the main thread. Mix is for the audio
 * thread, and is the only thing that may run concurrently with the rest. The two communicate through a
 * single-producer single-consumer queue of commands and a handful of atomics; voice state is owned by the audio
 * thread and clip storage by the main thread.
 *
 * A clip's memory is freed on the main thread, and only after the audio thread has provably stopped reading it:
 * DestroyClip queues a stop for every voice on the clip and records the mix epoch, and the slot is reclaimed once
 * two more mixes have completed. Reclaiming happens lazily inside CreateClip and DestroyClip, so nothing needs to
 * be called every frame.
 */
class AudioMixer
{
public:
    explicit AudioMixer(const AudioMixerDesc& desc = {});
    ~AudioMixer();

    AudioMixer(const AudioMixer&) = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;
    AudioMixer(AudioMixer&&) = delete;
    AudioMixer& operator=(AudioMixer&&) = delete;

    /** Main thread. Takes ownership of the clip. Throws when the clip is empty or every clip slot is live. */
    AudioClipHandle CreateClip(AudioClip&& clip);

    /**
     * Main thread. Stops every voice playing the clip - at once, not ramped - and frees it once the audio thread is
     * done with it. A stale or invalid handle is a no-op. When the command queue is full the clip stays live and an
     * error is logged; call again later.
     */
    void DestroyClip(AudioClipHandle handle);

    [[nodiscard]] bool IsClipValid(AudioClipHandle handle) const;

    /**
     * Main thread. Starts the clip on a free voice and returns its handle. Returns an invalid handle, with a log
     * line saying why, when the clip handle is stale, every voice is busy, or the command queue is full.
     */
    SoundHandle Play(AudioClipHandle clip, const PlaySoundDesc& desc = {});

    /** Main thread. Every call below is a no-op on a stale handle. Volume, pan and pitch changes are ramped. */
    void Stop(SoundHandle sound);
    void StopAll();
    void Pause(SoundHandle sound);
    void Resume(SoundHandle sound);
    void PauseAll();
    void ResumeAll();
    void SetVolume(SoundHandle sound, f32 volume);
    void SetPan(SoundHandle sound, f32 pan);
    void SetPitch(SoundHandle sound, f32 pitch);
    void SetLooping(SoundHandle sound, bool loop);

    /** Main thread. True while the voice is alive, which includes paused and not yet started by the audio thread. */
    [[nodiscard]] bool IsPlaying(SoundHandle sound) const;

    void SetMasterVolume(f32 volume);
    [[nodiscard]] f32 GetMasterVolume() const;
    void SetBusVolume(u8 bus, f32 volume);
    [[nodiscard]] f32 GetBusVolume(u8 bus) const;

    [[nodiscard]] u32 GetSampleRate() const { return m_desc.sample_rate; }
    [[nodiscard]] u32 GetMaxVoices() const { return m_desc.max_voices; }
    /** Voices the audio thread had going at the end of its last Mix. */
    [[nodiscard]] u32 GetActiveVoiceCount() const;
    /** Number of completed Mix calls. */
    [[nodiscard]] u64 GetMixEpoch() const;
    /** Commands Play and the rest had to drop because the queue was full. */
    [[nodiscard]] u32 GetDroppedCommandCount() const { return m_dropped_command_count; }

    /**
     * Audio thread. Applies every queued command, then writes GetSize() / 2 interleaved stereo frames into @p out,
     * replacing what was there. Never allocates and never blocks. The size must be even.
     */
    void Mix(Opal::ArrayView<f32> out);

private:
    struct Command
    {
        enum class Type : u8
        {
            None,
            Play,
            Stop,
            StopAll,
            Pause,
            Resume,
            PauseAll,
            ResumeAll,
            SetVolume,
            SetPan,
            SetPitch,
            SetLooping,
            DestroyClip,
        };

        Type type = Type::None;
        SoundHandle sound;
        u32 clip_index = 0;
        PlaySoundDesc desc;
        f32 value = 0.0f;
        bool flag = false;
    };

    struct ClipSlot
    {
        // Main thread writes these; the audio thread reads `clip` while a voice plays it.
        AudioClip clip;
        u32 generation = 0;
        bool is_live = false;
        bool is_pending_free = false;
        u64 free_after_epoch = 0;
    };

    struct Voice
    {
        // Main thread only.
        u32 generation = 0;

        // Audio thread only.
        u32 playing_generation = 0;
        u32 clip_index = 0;
        bool is_active = false;
        bool is_paused = false;
        bool is_stopping = false;
        bool loop = false;
        u8 bus = 0;
        f64 position = 0.0;
        f32 volume = 1.0f;
        f32 pan = 0.0f;
        f32 pitch = 1.0f;
        f32 gain_left = 0.0f;
        f32 gain_right = 0.0f;

        // Written by the audio thread when the voice ends, read by the main thread to tell a free slot from a
        // busy one: the slot is free when this equals `generation`.
        Opal::Atomic<u32> finished_generation = 0;
    };

    bool Send(const Command& command);
    /** Main thread. Finds a voice whose last sound has finished and claims it with a new generation. */
    SoundHandle AllocateVoice();
    [[nodiscard]] bool IsLive(SoundHandle sound) const;
    void ReclaimClipSlots();

    // Audio thread.
    void Execute(const Command& command);
    void FinishVoice(Voice& voice);
    void MixVoice(Voice& voice, Opal::ArrayView<f32> out, f32 master_volume);

    AudioMixerDesc m_desc;
    Opal::ChannelSPSC<Command, false> m_commands;
    Opal::DynamicArray<ClipSlot> m_clips;
    Opal::DynamicArray<Voice> m_voices;
    u32 m_next_voice = 0;
    u32 m_dropped_command_count = 0;
    u32 m_gain_ramp_frames = 0;

    Opal::Atomic<f32> m_master_volume = 1.0f;
    Opal::Atomic<f32> m_bus_volumes[k_audio_bus_count];
    Opal::Atomic<u32> m_active_voice_count = 0;
    Opal::Atomic<u64> m_mix_epoch = 0;
};

}  // namespace Rndr
