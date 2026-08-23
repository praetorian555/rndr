#pragma once

#include "opal/container/expected.h"
#include "opal/container/scope-ptr.h"
#include "opal/container/string.h"

#include "rndr/audio/audio-clip.hpp"
#include "rndr/audio/audio-device.hpp"
#include "rndr/audio/audio-mixer.hpp"
#include "rndr/audio/audio-types.hpp"
#include "rndr/error-codes.hpp"

namespace Rndr
{

struct AudioSystemDesc
{
    /** Rate everything is mixed at. The device converts to the endpoint's own rate. */
    u32 sample_rate = 48000;
    /** Frames the device keeps queued ahead of the endpoint: latency against safety from glitches. */
    u32 buffer_frames = 1024;
    /** How many sounds can play at once. Play returns an invalid handle once they are all busy. */
    u32 max_voices = 64;
    /** How many clips can be live at once. CreateClip and LoadClip report OutOfResources once they are all taken. */
    u32 max_clips = 256;
    /** How many calls can be made between two audio callbacks before they start being dropped. */
    u32 command_queue_capacity = 1024;
    f32 master_volume = 1.0f;
};

/**
 * What a game talks to: loads clips, plays them, and keeps the output device fed from a thread of its own.
 *
 * Clips belong to the system. CreateClip takes an AudioClip and hands back a handle; the sample data lives here
 * until DestroyClip or the system goes away, and a clip can be destroyed while sounds are playing it - they stop.
 * Sounds are named by SoundHandle, which goes stale on its own when the sound ends, so holding one past that is
 * fine and every call on it is a no-op. A sound also ends when a sound of at least its priority needs a voice and
 * there is none free, so a handle can go stale without the game asking for it - see PlaySoundDesc::priority.
 *
 * Every method is for the main thread - or any single thread, as long as it is the same one. Nothing needs to be
 * called per frame.
 *
 * Create reports ErrorCode::NoAudioDevice on a machine with no output, so a game that wants to run without sound
 * checks the code and carries on without an AudioSystem. Nothing in this API throws.
 */
class AudioSystem
{
public:
    /**
     * Builds the mixer and opens the output device.
     * @return The running system, ErrorCode::NoAudioDevice when the machine has no output endpoint, or
     *         ErrorCode::PlatformError when one is there and would not open. Heap-allocated because the system owns
     *         a thread and atomics and therefore cannot be moved into the result.
     */
    [[nodiscard]] static Opal::Expected<Opal::ScopePtr<AudioSystem>, ErrorCode> Create(const AudioSystemDesc& desc = {});

    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
    AudioSystem(AudioSystem&&) = delete;
    AudioSystem& operator=(AudioSystem&&) = delete;

    /**
     * Takes ownership of the clip.
     * @return Its handle, ErrorCode::InvalidArgument for an empty clip, or ErrorCode::OutOfResources when every
     *         clip slot is live.
     */
    Opal::Expected<AudioClipHandle, ErrorCode> CreateClip(AudioClip&& clip);
    /** File::LoadAudioClip followed by CreateClip, reporting whichever of the two failed. */
    Opal::Expected<AudioClipHandle, ErrorCode> LoadClip(const Opal::StringUtf8& file_path);
    /** Stops every sound on the clip and frees it once the audio thread is done with it. Stale handle: no-op. */
    void DestroyClip(AudioClipHandle clip);
    [[nodiscard]] bool IsClipValid(AudioClipHandle clip) const;

    /**
     * Starts the clip on a free voice. Returns an invalid handle, with a log line saying why, when the clip handle is
     * stale, every voice is busy, or the command queue is full.
     */
    SoundHandle Play(AudioClipHandle clip, const PlaySoundDesc& desc = {});

    /** Ramps the sound to silence over a couple of milliseconds and retires it. */
    void Stop(SoundHandle sound);
    void StopAll();
    /** Holds the sound where it is; the voice stays alive and IsPlaying stays true. */
    void Pause(SoundHandle sound);
    void Resume(SoundHandle sound);
    void PauseAll();
    void ResumeAll();
    void SetVolume(SoundHandle sound, f32 volume);
    void SetPan(SoundHandle sound, f32 pan);
    void SetPitch(SoundHandle sound, f32 pitch);
    void SetLooping(SoundHandle sound, bool loop);
    /** True while the voice is alive: playing, paused, or queued and not yet picked up by the audio thread. */
    [[nodiscard]] bool IsPlaying(SoundHandle sound) const;

    void SetMasterVolume(f32 volume);
    [[nodiscard]] f32 GetMasterVolume() const;
    void SetBusVolume(u8 bus, f32 volume);
    [[nodiscard]] f32 GetBusVolume(u8 bus) const;

    [[nodiscard]] u32 GetSampleRate() const;
    /** Voices the audio thread had going at its last callback. */
    [[nodiscard]] u32 GetActiveVoiceCount() const;
    /** Calls that were dropped because the command queue was full. Non-zero means something is spamming. */
    [[nodiscard]] u32 GetDroppedCommandCount() const;
    /** Sounds cut short because a sound of at least their priority needed the voice. */
    [[nodiscard]] u32 GetStolenVoiceCount() const;

private:
    explicit AudioSystem(const AudioSystemDesc& desc);

    template <typename T, typename... Args>
    friend T* Opal::New(Opal::AllocatorBase* /*allocator*/, Args&&... /*args*/);

    // Declaration order is destruction order in reverse: the device goes first and takes its thread with it, so
    // nothing is reading the mixer when the mixer, and with it every clip, is freed.
    AudioMixer m_mixer;
    Opal::ScopePtr<AudioDevice> m_device;
};

}  // namespace Rndr
