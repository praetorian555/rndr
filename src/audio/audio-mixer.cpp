#include "rndr/audio/audio-mixer.hpp"

#include <algorithm>
#include <cmath>

#include "opal/exceptions.h"

#include "rndr/log.hpp"

namespace
{

/** How long a gain change takes to land. Short enough to keep attacks, long enough to keep clicks out. */
constexpr Rndr::f32 k_gain_ramp_seconds = 0.002f;

constexpr Rndr::f32 k_min_pitch = 1.0f / 16.0f;
constexpr Rndr::f32 k_max_pitch = 16.0f;
constexpr Rndr::f32 k_pi = 3.14159265358979323846f;

Rndr::f32 ClampVolume(Rndr::f32 volume)
{
    return std::max(volume, 0.0f);
}

Rndr::f32 ClampPan(Rndr::f32 pan)
{
    return std::clamp(pan, -1.0f, 1.0f);
}

Rndr::f32 ClampPitch(Rndr::f32 pitch)
{
    return std::clamp(pitch, k_min_pitch, k_max_pitch);
}

/** Constant-power pan law: both gains are 1/sqrt(2) at the centre and one of them is 1 at either extreme. */
void PanGains(Rndr::f32 pan, Rndr::f32& out_left, Rndr::f32& out_right)
{
    const Rndr::f32 angle = (pan + 1.0f) * k_pi / 4.0f;
    out_left = std::cos(angle);
    out_right = std::sin(angle);
}

/** Moves `current` toward `target` by at most `step`, so a change lands over a ramp rather than in one sample. */
Rndr::f32 Approach(Rndr::f32 current, Rndr::f32 target, Rndr::f32 step)
{
    if (current < target)
    {
        return std::min(current + step, target);
    }
    return std::max(current - step, target);
}

/** Runs before any member is built: the queue halts on a zero capacity rather than report it. */
const Rndr::AudioMixerDesc& ValidateDesc(const Rndr::AudioMixerDesc& desc)
{
    if (desc.sample_rate == 0)
    {
        throw Opal::Exception("AudioMixer: sample rate must be greater than 0");
    }
    if (desc.max_voices == 0 || desc.max_clips == 0 || desc.command_queue_capacity == 0)
    {
        throw Opal::Exception("AudioMixer: voice, clip and command queue capacities must be greater than 0");
    }
    return desc;
}

}  // namespace

Rndr::AudioMixer::AudioMixer(const AudioMixerDesc& desc)
    : m_desc(ValidateDesc(desc)), m_commands(desc.command_queue_capacity), m_clips(desc.max_clips), m_voices(desc.max_voices)
{
    m_gain_ramp_frames = std::max(1u, static_cast<u32>(static_cast<f32>(desc.sample_rate) * k_gain_ramp_seconds));
    for (Opal::Atomic<f32>& bus_volume : m_bus_volumes)
    {
        bus_volume.Store<Opal::MemoryOrder::Relaxed>(1.0f);
    }
}

Rndr::AudioMixer::~AudioMixer() = default;

// Main thread ///////////////////////////////////////////////////////////////////////////////////////////////////////

bool Rndr::AudioMixer::Send(const Command& command)
{
    if (m_commands.transmitter.TrySend(command))
    {
        return true;
    }
    m_dropped_command_count++;
    RNDR_LOG_WARNING("AudioMixer: command queue is full, dropping a command");
    return false;
}

void Rndr::AudioMixer::ReclaimClipSlots()
{
    const u64 epoch = m_mix_epoch.Load<Opal::MemoryOrder::Acquire>();
    for (ClipSlot& slot : m_clips)
    {
        if (slot.is_pending_free && epoch >= slot.free_after_epoch)
        {
            slot.clip = AudioClip();
            slot.is_pending_free = false;
        }
    }
}

Rndr::AudioClipHandle Rndr::AudioMixer::CreateClip(AudioClip&& clip)
{
    if (!clip.IsValid())
    {
        throw Opal::Exception("AudioMixer: cannot create a clip from an empty AudioClip");
    }
    ReclaimClipSlots();
    for (u32 i = 0; i < m_clips.GetSize(); i++)
    {
        ClipSlot& slot = m_clips[i];
        if (slot.is_live || slot.is_pending_free)
        {
            continue;
        }
        slot.clip = std::move(clip);
        slot.generation++;
        if (slot.generation == 0)
        {
            slot.generation = 1;  // zero is the invalid handle
        }
        slot.is_live = true;
        return {i, slot.generation};
    }
    throw Opal::Exception("AudioMixer: every clip slot is in use");
}

bool Rndr::AudioMixer::IsClipValid(AudioClipHandle handle) const
{
    if (!handle.IsValid() || handle.index >= m_clips.GetSize())
    {
        return false;
    }
    const ClipSlot& slot = m_clips[handle.index];
    return slot.is_live && slot.generation == handle.generation;
}

void Rndr::AudioMixer::DestroyClip(AudioClipHandle handle)
{
    ReclaimClipSlots();
    if (!IsClipValid(handle))
    {
        return;
    }
    Command command;
    command.type = Command::Type::DestroyClip;
    command.clip_index = handle.index;
    if (!Send(command))
    {
        RNDR_LOG_ERROR("AudioMixer: could not queue the clip destruction, the clip stays live");
        return;
    }
    // The epoch is read after the send: the mix that picks the command up completes with at most the value read
    // here plus two, and nothing reads the clip after that.
    ClipSlot& slot = m_clips[handle.index];
    slot.is_live = false;
    slot.is_pending_free = true;
    slot.free_after_epoch = m_mix_epoch.Load<Opal::MemoryOrder::Acquire>() + 2;
}

Rndr::SoundHandle Rndr::AudioMixer::AllocateVoice()
{
    for (u32 attempt = 0; attempt < m_voices.GetSize(); attempt++)
    {
        const u32 index = (m_next_voice + attempt) % m_voices.GetSize();
        Voice& voice = m_voices[index];
        if (voice.finished_generation.Load<Opal::MemoryOrder::Acquire>() != voice.generation)
        {
            continue;  // the audio thread has not finished the last sound put here
        }
        voice.generation++;
        if (voice.generation == 0)
        {
            voice.generation = 1;
        }
        m_next_voice = (index + 1) % m_voices.GetSize();
        return {index, voice.generation};
    }
    return {};
}

bool Rndr::AudioMixer::IsLive(SoundHandle sound) const
{
    if (!sound.IsValid() || sound.index >= m_voices.GetSize())
    {
        return false;
    }
    const Voice& voice = m_voices[sound.index];
    return voice.generation == sound.generation && voice.finished_generation.Load<Opal::MemoryOrder::Acquire>() != sound.generation;
}

Rndr::SoundHandle Rndr::AudioMixer::Play(AudioClipHandle clip, const PlaySoundDesc& desc)
{
    if (!IsClipValid(clip))
    {
        RNDR_LOG_ERROR("AudioMixer: Play called with a clip handle that is not live");
        return {};
    }
    const SoundHandle sound = AllocateVoice();
    if (!sound.IsValid())
    {
        RNDR_LOG_DEBUG("AudioMixer: every voice is busy, the sound is not played");
        return {};
    }
    const u32 previous_generation = m_voices[sound.index].finished_generation.Load<Opal::MemoryOrder::Relaxed>();

    Command command;
    command.type = Command::Type::Play;
    command.sound = sound;
    command.clip_index = clip.index;
    command.desc = desc;
    command.desc.volume = ClampVolume(desc.volume);
    command.desc.pan = ClampPan(desc.pan);
    command.desc.pitch = ClampPitch(desc.pitch);
    if (desc.bus >= k_audio_bus_count)
    {
        RNDR_LOG_ERROR("AudioMixer: bus {} is out of range, routing through bus 0", static_cast<u32>(desc.bus));
        command.desc.bus = 0;
    }
    if (!Send(command))
    {
        // The audio thread never saw this generation, so hand the slot back as it was.
        m_voices[sound.index].generation = previous_generation;
        return {};
    }
    return sound;
}

void Rndr::AudioMixer::Stop(SoundHandle sound)
{
    if (!IsLive(sound))
    {
        return;
    }
    Command command;
    command.type = Command::Type::Stop;
    command.sound = sound;
    Send(command);
}

void Rndr::AudioMixer::StopAll()
{
    Command command;
    command.type = Command::Type::StopAll;
    Send(command);
}

void Rndr::AudioMixer::Pause(SoundHandle sound)
{
    if (!IsLive(sound))
    {
        return;
    }
    Command command;
    command.type = Command::Type::Pause;
    command.sound = sound;
    Send(command);
}

void Rndr::AudioMixer::Resume(SoundHandle sound)
{
    if (!IsLive(sound))
    {
        return;
    }
    Command command;
    command.type = Command::Type::Resume;
    command.sound = sound;
    Send(command);
}

void Rndr::AudioMixer::PauseAll()
{
    Command command;
    command.type = Command::Type::PauseAll;
    Send(command);
}

void Rndr::AudioMixer::ResumeAll()
{
    Command command;
    command.type = Command::Type::ResumeAll;
    Send(command);
}

void Rndr::AudioMixer::SetVolume(SoundHandle sound, f32 volume)
{
    if (!IsLive(sound))
    {
        return;
    }
    Command command;
    command.type = Command::Type::SetVolume;
    command.sound = sound;
    command.value = ClampVolume(volume);
    Send(command);
}

void Rndr::AudioMixer::SetPan(SoundHandle sound, f32 pan)
{
    if (!IsLive(sound))
    {
        return;
    }
    Command command;
    command.type = Command::Type::SetPan;
    command.sound = sound;
    command.value = ClampPan(pan);
    Send(command);
}

void Rndr::AudioMixer::SetPitch(SoundHandle sound, f32 pitch)
{
    if (!IsLive(sound))
    {
        return;
    }
    Command command;
    command.type = Command::Type::SetPitch;
    command.sound = sound;
    command.value = ClampPitch(pitch);
    Send(command);
}

void Rndr::AudioMixer::SetLooping(SoundHandle sound, bool loop)
{
    if (!IsLive(sound))
    {
        return;
    }
    Command command;
    command.type = Command::Type::SetLooping;
    command.sound = sound;
    command.flag = loop;
    Send(command);
}

bool Rndr::AudioMixer::IsPlaying(SoundHandle sound) const
{
    return IsLive(sound);
}

void Rndr::AudioMixer::SetMasterVolume(f32 volume)
{
    m_master_volume.Store<Opal::MemoryOrder::Relaxed>(ClampVolume(volume));
}

Rndr::f32 Rndr::AudioMixer::GetMasterVolume() const
{
    return m_master_volume.Load<Opal::MemoryOrder::Relaxed>();
}

void Rndr::AudioMixer::SetBusVolume(u8 bus, f32 volume)
{
    if (bus >= k_audio_bus_count)
    {
        RNDR_LOG_ERROR("AudioMixer: bus {} is out of range", static_cast<u32>(bus));
        return;
    }
    m_bus_volumes[bus].Store<Opal::MemoryOrder::Relaxed>(ClampVolume(volume));
}

Rndr::f32 Rndr::AudioMixer::GetBusVolume(u8 bus) const
{
    if (bus >= k_audio_bus_count)
    {
        RNDR_LOG_ERROR("AudioMixer: bus {} is out of range", static_cast<u32>(bus));
        return 0.0f;
    }
    return m_bus_volumes[bus].Load<Opal::MemoryOrder::Relaxed>();
}

Rndr::u32 Rndr::AudioMixer::GetActiveVoiceCount() const
{
    return m_active_voice_count.Load<Opal::MemoryOrder::Relaxed>();
}

Rndr::u64 Rndr::AudioMixer::GetMixEpoch() const
{
    return m_mix_epoch.Load<Opal::MemoryOrder::Acquire>();
}

// Audio thread //////////////////////////////////////////////////////////////////////////////////////////////////////

void Rndr::AudioMixer::FinishVoice(Voice& voice)
{
    voice.is_active = false;
    voice.is_paused = false;
    voice.is_stopping = false;
    voice.gain_left = 0.0f;
    voice.gain_right = 0.0f;
    voice.finished_generation.Store<Opal::MemoryOrder::Release>(voice.playing_generation);
}

void Rndr::AudioMixer::Execute(const Command& command)
{
    // The per-voice commands name a generation; one that does not match belongs to a sound that has already
    // finished and a newer one now in the slot, and must not touch it.
    Voice* voice = nullptr;
    if (command.sound.IsValid() && command.sound.index < m_voices.GetSize())
    {
        Voice& candidate = m_voices[command.sound.index];
        const bool is_this_sound = candidate.is_active && candidate.playing_generation == command.sound.generation;
        voice = is_this_sound ? &candidate : nullptr;
    }

    switch (command.type)
    {
        case Command::Type::Play:
        {
            Voice& fresh = m_voices[command.sound.index];
            if (fresh.is_active)
            {
                // Cannot happen: the main thread only hands out slots whose last sound has finished. Retire the
                // old sound rather than leak a generation.
                FinishVoice(fresh);
            }
            fresh.playing_generation = command.sound.generation;
            fresh.clip_index = command.clip_index;
            fresh.is_active = true;
            fresh.is_paused = command.desc.start_paused;
            fresh.is_stopping = false;
            fresh.loop = command.desc.loop;
            fresh.bus = command.desc.bus;
            fresh.position = 0.0;
            fresh.volume = command.desc.volume;
            fresh.pan = command.desc.pan;
            fresh.pitch = command.desc.pitch;
            fresh.gain_left = 0.0f;  // ramps up from silence so the first sample does not click
            fresh.gain_right = 0.0f;
            break;
        }
        case Command::Type::Stop:
            if (voice != nullptr)
            {
                // A paused voice has no ramp to play out.
                if (voice->is_paused)
                {
                    FinishVoice(*voice);
                }
                else
                {
                    voice->is_stopping = true;
                }
            }
            break;
        case Command::Type::StopAll:
            for (Voice& v : m_voices)
            {
                if (v.is_active)
                {
                    if (v.is_paused)
                    {
                        FinishVoice(v);
                    }
                    else
                    {
                        v.is_stopping = true;
                    }
                }
            }
            break;
        case Command::Type::Pause:
            // A stopping voice is ramping to silence and needs to keep mixing to get there.
            if (voice != nullptr && !voice->is_stopping)
            {
                voice->is_paused = true;
            }
            break;
        case Command::Type::Resume:
            if (voice != nullptr)
            {
                voice->is_paused = false;
            }
            break;
        case Command::Type::PauseAll:
            for (Voice& v : m_voices)
            {
                if (v.is_active && !v.is_stopping)
                {
                    v.is_paused = true;
                }
            }
            break;
        case Command::Type::ResumeAll:
            for (Voice& v : m_voices)
            {
                if (v.is_active)
                {
                    v.is_paused = false;
                }
            }
            break;
        case Command::Type::SetVolume:
            if (voice != nullptr)
            {
                voice->volume = command.value;
            }
            break;
        case Command::Type::SetPan:
            if (voice != nullptr)
            {
                voice->pan = command.value;
            }
            break;
        case Command::Type::SetPitch:
            if (voice != nullptr)
            {
                voice->pitch = command.value;
            }
            break;
        case Command::Type::SetLooping:
            if (voice != nullptr)
            {
                voice->loop = command.flag;
            }
            break;
        case Command::Type::DestroyClip:
            for (Voice& v : m_voices)
            {
                if (v.is_active && v.clip_index == command.clip_index)
                {
                    FinishVoice(v);
                }
            }
            break;
        case Command::Type::None:
            break;
    }
}

void Rndr::AudioMixer::MixVoice(Voice& voice, Opal::ArrayView<f32> out, f32 master_volume)
{
    const AudioClip& clip = m_clips[voice.clip_index].clip;
    const Opal::ArrayView<const f32> samples = clip.GetData();
    const u64 clip_frame_count = clip.GetFrameCount();
    const u32 channel_count = clip.GetChannelCount();
    const u64 out_frame_count = out.GetSize() / 2;

    f32 pan_left = 0.0f;
    f32 pan_right = 0.0f;
    PanGains(voice.pan, pan_left, pan_right);
    const f32 bus_volume = m_bus_volumes[voice.bus].Load<Opal::MemoryOrder::Relaxed>();
    const f32 gain = voice.is_stopping ? 0.0f : voice.volume * bus_volume * master_volume;
    const f32 target_left = gain * pan_left;
    const f32 target_right = gain * pan_right;
    // Whatever the distance, a gain lands in one ramp length.
    const f32 step_left = std::abs(target_left - voice.gain_left) / static_cast<f32>(m_gain_ramp_frames);
    const f32 step_right = std::abs(target_right - voice.gain_right) / static_cast<f32>(m_gain_ramp_frames);

    const f64 position_step = static_cast<f64>(voice.pitch) * static_cast<f64>(clip.GetSampleRate()) / static_cast<f64>(m_desc.sample_rate);
    const f64 clip_length = static_cast<f64>(clip_frame_count);

    for (u64 frame = 0; frame < out_frame_count; frame++)
    {
        voice.gain_left = Approach(voice.gain_left, target_left, step_left);
        voice.gain_right = Approach(voice.gain_right, target_right, step_right);

        // Linear interpolation between the frame under the read position and the next one. At the end of a
        // looping clip the next frame is the first one; at the end of a one-shot it is the last frame again.
        const auto index = static_cast<u64>(voice.position);
        const auto fraction = static_cast<f32>(voice.position - static_cast<f64>(index));
        u64 next_index = index + 1;
        if (next_index >= clip_frame_count)
        {
            next_index = voice.loop ? 0 : index;
        }

        f32 left = 0.0f;
        f32 right = 0.0f;
        if (channel_count == 1)
        {
            const f32 sample = samples[index] + (samples[next_index] - samples[index]) * fraction;
            left = sample;
            right = sample;
        }
        else
        {
            const f32 left_a = samples[index * 2];
            const f32 right_a = samples[index * 2 + 1];
            left = left_a + (samples[next_index * 2] - left_a) * fraction;
            right = right_a + (samples[next_index * 2 + 1] - right_a) * fraction;
        }
        out[frame * 2] += left * voice.gain_left;
        out[frame * 2 + 1] += right * voice.gain_right;

        voice.position += position_step;
        if (voice.position >= clip_length)
        {
            if (!voice.loop)
            {
                FinishVoice(voice);
                return;
            }
            voice.position = std::fmod(voice.position, clip_length);
        }

        if (voice.is_stopping && voice.gain_left == 0.0f && voice.gain_right == 0.0f)
        {
            FinishVoice(voice);
            return;
        }
    }
}

void Rndr::AudioMixer::Mix(Opal::ArrayView<f32> out)
{
    for (f32& sample : out)
    {
        sample = 0.0f;
    }

    Command command;
    while (m_commands.receiver.TryReceive(command) == Opal::ErrorCode::Success)
    {
        Execute(command);
    }

    const f32 master_volume = m_master_volume.Load<Opal::MemoryOrder::Relaxed>();
    u32 active_count = 0;
    for (Voice& voice : m_voices)
    {
        if (!voice.is_active)
        {
            continue;
        }
        if (!voice.is_paused)
        {
            MixVoice(voice, out, master_volume);
        }
        if (voice.is_active)
        {
            active_count++;
        }
    }

    for (f32& sample : out)
    {
        sample = std::clamp(sample, -1.0f, 1.0f);
    }

    m_active_voice_count.Store<Opal::MemoryOrder::Relaxed>(active_count);
    m_mix_epoch.Store<Opal::MemoryOrder::Release>(m_mix_epoch.Load<Opal::MemoryOrder::Relaxed>() + 1);
}
