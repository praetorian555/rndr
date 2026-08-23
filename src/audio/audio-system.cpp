#include "rndr/audio/audio-system.hpp"

#include "rndr/file.hpp"

namespace
{

Rndr::AudioMixerDesc MixerDesc(const Rndr::AudioSystemDesc& desc)
{
    return {.sample_rate = desc.sample_rate,
            .max_voices = desc.max_voices,
            .max_clips = desc.max_clips,
            .command_queue_capacity = desc.command_queue_capacity};
}

}  // namespace

Rndr::AudioSystem::AudioSystem(const AudioSystemDesc& desc) : m_mixer(MixerDesc(desc))
{
    m_mixer.SetMasterVolume(desc.master_volume);
    const AudioDeviceDesc device_desc{.sample_rate = desc.sample_rate, .buffer_frames = desc.buffer_frames};
    m_device = AudioDevice::Create(device_desc, [this](Opal::ArrayView<f32> out) { m_mixer.Mix(out); });
}

Rndr::AudioSystem::~AudioSystem() = default;

Rndr::AudioClipHandle Rndr::AudioSystem::CreateClip(AudioClip&& clip)
{
    return m_mixer.CreateClip(std::move(clip));
}

Rndr::AudioClipHandle Rndr::AudioSystem::LoadClip(const Opal::StringUtf8& file_path)
{
    return m_mixer.CreateClip(File::LoadAudioClip(file_path));
}

void Rndr::AudioSystem::DestroyClip(AudioClipHandle clip)
{
    m_mixer.DestroyClip(clip);
}

bool Rndr::AudioSystem::IsClipValid(AudioClipHandle clip) const
{
    return m_mixer.IsClipValid(clip);
}

Rndr::SoundHandle Rndr::AudioSystem::Play(AudioClipHandle clip, const PlaySoundDesc& desc)
{
    return m_mixer.Play(clip, desc);
}

void Rndr::AudioSystem::Stop(SoundHandle sound)
{
    m_mixer.Stop(sound);
}

void Rndr::AudioSystem::StopAll()
{
    m_mixer.StopAll();
}

void Rndr::AudioSystem::Pause(SoundHandle sound)
{
    m_mixer.Pause(sound);
}

void Rndr::AudioSystem::Resume(SoundHandle sound)
{
    m_mixer.Resume(sound);
}

void Rndr::AudioSystem::PauseAll()
{
    m_mixer.PauseAll();
}

void Rndr::AudioSystem::ResumeAll()
{
    m_mixer.ResumeAll();
}

void Rndr::AudioSystem::SetVolume(SoundHandle sound, f32 volume)
{
    m_mixer.SetVolume(sound, volume);
}

void Rndr::AudioSystem::SetPan(SoundHandle sound, f32 pan)
{
    m_mixer.SetPan(sound, pan);
}

void Rndr::AudioSystem::SetPitch(SoundHandle sound, f32 pitch)
{
    m_mixer.SetPitch(sound, pitch);
}

void Rndr::AudioSystem::SetLooping(SoundHandle sound, bool loop)
{
    m_mixer.SetLooping(sound, loop);
}

bool Rndr::AudioSystem::IsPlaying(SoundHandle sound) const
{
    return m_mixer.IsPlaying(sound);
}

void Rndr::AudioSystem::SetMasterVolume(f32 volume)
{
    m_mixer.SetMasterVolume(volume);
}

Rndr::f32 Rndr::AudioSystem::GetMasterVolume() const
{
    return m_mixer.GetMasterVolume();
}

void Rndr::AudioSystem::SetBusVolume(u8 bus, f32 volume)
{
    m_mixer.SetBusVolume(bus, volume);
}

Rndr::f32 Rndr::AudioSystem::GetBusVolume(u8 bus) const
{
    return m_mixer.GetBusVolume(bus);
}

Rndr::u32 Rndr::AudioSystem::GetSampleRate() const
{
    return m_mixer.GetSampleRate();
}

Rndr::u32 Rndr::AudioSystem::GetActiveVoiceCount() const
{
    return m_mixer.GetActiveVoiceCount();
}

Rndr::u32 Rndr::AudioSystem::GetDroppedCommandCount() const
{
    return m_mixer.GetDroppedCommandCount();
}
