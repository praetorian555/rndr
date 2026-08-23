#pragma once

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/threading/atomic.h"
#include "opal/threading/channel-spsc.h"

#include "rndr/audio/audio-clip.hpp"
#include "rndr/audio/audio-types.hpp"
#include "rndr/error-codes.hpp"

namespace Rndr
{

struct AudioMixerDesc
{
    /** Rate of the buffers Mix fills. Clips at any other rate are resampled on the way through. */
    u32 sample_rate = 48000;
    /** How many sounds can play at once. Play returns an invalid handle once they are all busy. */
    u32 max_voices = 64;
    /** How many clips can be live at once. CreateClip reports OutOfResources once they are all taken. */
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
 * When every voice is busy, a sound takes one from a sound that matters less - see StealVoice for the rule. The
 * main thread decides, since it is the one handing out slots, and the sound it takes the slot from is stale from
 * that moment: its handle stops naming anything before the audio thread has even seen the new sound. What it was
 * playing keeps mixing for one ramp as a voice nobody can address, so the swap is a crossfade rather than a click.
 *
 * A clip's memory is freed on the main thread, and only after the audio thread has provably stopped reading it:
 * DestroyClip queues a stop for every voice on the clip and records the mix epoch, and the slot is reclaimed once
 * two more mixes have completed. Reclaiming happens lazily inside CreateClip and DestroyClip, so nothing needs to
 * be called every frame.
 */
class AudioMixer
{
public:
    /**
     * A desc with a zero field is a bug in the calling code rather than something to recover from: it asserts in a
     * debug build and is clamped to a working minimum in a release one. Construction cannot fail.
     */
    explicit AudioMixer(const AudioMixerDesc& desc = {});
    ~AudioMixer();

    AudioMixer(const AudioMixer&) = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;
    AudioMixer(AudioMixer&&) = delete;
    AudioMixer& operator=(AudioMixer&&) = delete;

    /**
     * Main thread. Takes ownership of the clip.
     * @return Its handle, ErrorCode::InvalidArgument for an empty clip, or ErrorCode::OutOfResources when every
     *         clip slot is live.
     */
    Opal::Expected<AudioClipHandle, ErrorCode> CreateClip(AudioClip&& clip);

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
    /** Sounds that were cut short because a sound of at least their priority needed the voice. */
    [[nodiscard]] u32 GetStolenVoiceCount() const { return m_stolen_voice_count; }

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
        // Main thread only: what Play was asked for, kept current by the setters, and read when a voice has to be
        // taken from someone. The gain here is the one the game asked for rather than the amplitude the audio
        // thread is producing, which is the only version the deciding thread can see.
        u32 generation = 0;
        u8 priority = 0;
        u8 main_bus = 0;
        f32 main_volume = 1.0f;
        u64 start_order = 0;
        bool stop_requested = false;
        bool pause_requested = false;

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
    /**
     * Main thread. Finds a voice whose last sound has finished and claims it with a new generation. The generation
     * the slot carried before is handed back, so a Play whose command is dropped can put the slot back as it was.
     */
    SoundHandle AllocateVoice(u32& out_previous_generation);
    /**
     * Main thread. Takes a voice from a sound that matters less than the one at @p priority, and returns the claimed
     * slot. A voice already on its way out goes first; after that only voices of equal or lower priority are
     * candidates, the lowest priority among them, then the quietest, then the oldest. A paused voice is never taken:
     * it is silent now and wanted back later. Returns an invalid handle when there is nothing to take.
     */
    SoundHandle StealVoice(u8 priority, u32& out_previous_generation);
    [[nodiscard]] bool IsLive(SoundHandle sound) const;
    void ReclaimClipSlots();

    // Audio thread.
    void Execute(const Command& command);
    void FinishVoice(Voice& voice);
    /**
     * Hands what a slot was playing to a voice nobody can address, so it can ramp down while the sound that took
     * the slot ramps up. Leaves the slot ready to be filled.
     */
    void MoveToFading(Voice& voice);
    void MixVoice(Voice& voice, Opal::ArrayView<f32> out, f32 master_volume);

    AudioMixerDesc m_desc;
    Opal::ChannelSPSC<Command, false> m_commands;
    Opal::DynamicArray<ClipSlot> m_clips;
    Opal::DynamicArray<Voice> m_voices;
    /** Audio thread only: sounds whose slot was taken, ramping down. No handle names them and no command reaches them. */
    Opal::DynamicArray<Voice> m_fading_voices;
    u32 m_next_voice = 0;
    u64 m_next_start_order = 0;
    u32 m_dropped_command_count = 0;
    u32 m_stolen_voice_count = 0;
    u32 m_gain_ramp_frames = 0;

    Opal::Atomic<f32> m_master_volume = 1.0f;
    Opal::Atomic<f32> m_bus_volumes[k_audio_bus_count];
    Opal::Atomic<u32> m_active_voice_count = 0;
    Opal::Atomic<u64> m_mix_epoch = 0;
};

}  // namespace Rndr
