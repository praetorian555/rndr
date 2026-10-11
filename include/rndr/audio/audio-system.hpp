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
    /** How many streams can be live at once. CreateStream reports OutOfResources once they are all taken. */
    u32 max_streams = 4;
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
 * Every method is for the main thread - or any single thread, as long as it is the same one - except the three a
 * stream's writer calls, WriteStream, GetStreamFreeFrames and FlushStream. Nothing needs to be called per frame.
 *
 * A stream is a sound whose samples arrive while it plays, such as the sound of a video being decoded: CreateStream
 * makes a ring buffer, PlayStream plays it as a sound the usual calls control, and a thread of the caller's keeps
 * writing decoded frames into it. GetStreamPosition says which of those frames is being heard, which is the clock a
 * video's frames are shown against.
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
     * Makes a stream: a ring buffer of interleaved frames at GetSampleRate(), written while it plays.
     * @return Its handle, ErrorCode::InvalidArgument for a channel count other than 1 or 2, or
     *         ErrorCode::OutOfResources when every stream slot is live.
     */
    Opal::Expected<AudioStreamHandle, ErrorCode> CreateStream(const AudioStreamDesc& desc = {});
    /** Stops the stream's sound and frees the stream. Its writer must have stopped first. Stale handle: no-op. */
    void DestroyStream(AudioStreamHandle stream);
    [[nodiscard]] bool IsStreamValid(AudioStreamHandle stream) const;

    /**
     * Plays the stream from wherever its writer has got to. One sound per stream at a time; pitch and loop are
     * ignored. Start it paused and Resume once enough is written, or it plays silence and counts an underrun first.
     * Returns an invalid handle, with a log line saying why, when the stream handle is stale, the stream's last sound
     * is still playing, or for the reasons Play gives.
     */
    SoundHandle PlayStream(AudioStreamHandle stream, const PlaySoundDesc& desc = {});

    /**
     * The writer's thread. Appends interleaved frames in the stream's channel count, as many as there is room for.
     * @return The number of frames taken, from the front of @p interleaved.
     */
    u64 WriteStream(AudioStreamHandle stream, Opal::ArrayView<const f32> interleaved);
    /** The writer's thread. Frames WriteStream would take now. */
    [[nodiscard]] u64 GetStreamFreeFrames(AudioStreamHandle stream) const;
    /**
     * The writer's thread, or any thread while nothing writes. Drops what has been written and not played, for a
     * seek. The frames still count in GetStreamPosition, which goes on from where the writer's count is.
     */
    void FlushStream(AudioStreamHandle stream);

    /** Frames written and not yet played or flushed. */
    [[nodiscard]] u64 GetStreamQueuedFrames(AudioStreamHandle stream) const;
    /**
     * The frame of the stream reaching the speaker now, counted from the first frame ever written, flushed ones
     * included. Takes in what the device holds and has not played, and moves smoothly between callbacks.
     */
    [[nodiscard]] u64 GetStreamPosition(AudioStreamHandle stream) const;
    /** Times the stream ran dry while its sound played. A dry spell counts once however long it lasts. */
    [[nodiscard]] u32 GetStreamUnderrunCount(AudioStreamHandle stream) const;

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
