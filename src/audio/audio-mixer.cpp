#include "rndr/audio/audio-mixer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

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

/**
 * Runs before any member is built. A zero field is a bug in the calling code, not a runtime condition, so it trips
 * an assert in a debug build; a release build is left with something that works rather than a queue that halts on
 * a zero capacity.
 */
Rndr::AudioMixerDesc SanitizeDesc(const Rndr::AudioMixerDesc& desc)
{
    RNDR_ASSERT(desc.sample_rate > 0, "AudioMixer: sample rate must be greater than 0");
    RNDR_ASSERT(desc.max_voices > 0, "AudioMixer: voice capacity must be greater than 0");
    RNDR_ASSERT(desc.max_clips > 0, "AudioMixer: clip capacity must be greater than 0");
    RNDR_ASSERT(desc.command_queue_capacity > 0, "AudioMixer: command queue capacity must be greater than 0");

    Rndr::AudioMixerDesc sane = desc;
    sane.sample_rate = std::max(sane.sample_rate, 8000u);
    sane.max_voices = std::max(sane.max_voices, 1u);
    sane.max_clips = std::max(sane.max_clips, 1u);
    sane.command_queue_capacity = std::max(sane.command_queue_capacity, 1u);
    return sane;
}

}  // namespace

Rndr::AudioMixer::AudioMixer(const AudioMixerDesc& desc)
    : m_desc(SanitizeDesc(desc)),
      m_commands(m_desc.command_queue_capacity),
      m_clips(m_desc.max_clips),
      m_streams(m_desc.max_streams),
      m_voices(m_desc.max_voices),
      m_fading_voices(m_desc.max_voices)
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

Opal::Expected<Rndr::AudioClipHandle, Rndr::ErrorCode> Rndr::AudioMixer::CreateClip(AudioClip&& clip)
{
    using Result = Opal::Expected<AudioClipHandle, ErrorCode>;

    if (!clip.IsValid())
    {
        RNDR_LOG_ERROR("AudioMixer: cannot create a clip from an empty AudioClip");
        return Result(ErrorCode::InvalidArgument);
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
        return Result(AudioClipHandle{i, slot.generation});
    }
    RNDR_LOG_ERROR("AudioMixer: every clip slot is in use");
    return Result(ErrorCode::OutOfResources);
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
    command.source_index = handle.index;
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

Rndr::SoundHandle Rndr::AudioMixer::AllocateVoice(u32& out_previous_generation)
{
    for (u32 attempt = 0; attempt < m_voices.GetSize(); attempt++)
    {
        const u32 index = (m_next_voice + attempt) % m_voices.GetSize();
        Voice& voice = m_voices[index];
        if (voice.finished_generation.Load<Opal::MemoryOrder::Acquire>() != voice.generation)
        {
            continue;  // the audio thread has not finished the last sound put here
        }
        out_previous_generation = voice.generation;
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

Rndr::SoundHandle Rndr::AudioMixer::StealVoice(u8 priority, u32& out_previous_generation)
{
    Voice* victim = nullptr;
    for (Voice& voice : m_voices)
    {
        // Only live slots. A slot whose sound has finished would have been found by AllocateVoice.
        if (voice.finished_generation.Load<Opal::MemoryOrder::Acquire>() == voice.generation)
        {
            continue;
        }
        // A paused sound is silent now and wanted back when it resumes; taking it would be heard later.
        if (voice.pause_requested)
        {
            continue;
        }
        // Something already on its way out costs nothing to take, whatever it was.
        if (voice.stop_requested)
        {
            victim = &voice;
            break;
        }
        if (voice.priority > priority)
        {
            continue;
        }
        if (victim == nullptr)
        {
            victim = &voice;
            continue;
        }
        // Lowest priority first, then the quietest of those, then the one that has been playing longest. The gain
        // is what the game asked for, which is all this thread knows.
        const f32 victim_gain = victim->main_volume * m_bus_volumes[victim->main_bus].Load<Opal::MemoryOrder::Relaxed>();
        const f32 voice_gain = voice.main_volume * m_bus_volumes[voice.main_bus].Load<Opal::MemoryOrder::Relaxed>();
        bool is_better_victim = false;
        if (voice.priority != victim->priority)
        {
            is_better_victim = voice.priority < victim->priority;
        }
        else if (voice_gain != victim_gain)
        {
            is_better_victim = voice_gain < victim_gain;
        }
        else
        {
            is_better_victim = voice.start_order < victim->start_order;
        }
        if (is_better_victim)
        {
            victim = &voice;
        }
    }

    if (victim == nullptr)
    {
        return {};
    }

    // Bumping the generation retires the old handle right here: nothing the game holds names this sound any more.
    out_previous_generation = victim->generation;
    victim->generation++;
    if (victim->generation == 0)
    {
        victim->generation = 1;
    }
    m_stolen_voice_count++;
    const auto index = static_cast<u32>(victim - m_voices.GetData());
    return {index, victim->generation};
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
    return StartVoice(clip.index, false, desc);
}

Rndr::SoundHandle Rndr::AudioMixer::StartVoice(u32 source_index, bool is_stream, const PlaySoundDesc& desc)
{
    // What the slot's generation was before it was claimed, so a send that does not go through can put it back. For
    // a stolen slot that is the generation of the sound still playing there, which is what makes the steal undoable.
    u32 previous_generation = 0;
    bool was_stolen = false;
    SoundHandle sound = AllocateVoice(previous_generation);
    if (!sound.IsValid())
    {
        sound = StealVoice(desc.priority, previous_generation);
        was_stolen = sound.IsValid();
    }
    if (!sound.IsValid())
    {
        RNDR_LOG_DEBUG("AudioMixer: every voice is busy with sounds of a higher priority, the sound is not played");
        return {};
    }

    Command command;
    command.type = Command::Type::Play;
    command.sound = sound;
    command.source_index = source_index;
    command.is_stream = is_stream;
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
        // The audio thread never saw this generation, so hand the slot back as it was - which for a stolen slot
        // means the sound that was about to lose it keeps it, handle and all.
        m_voices[sound.index].generation = previous_generation;
        if (was_stolen)
        {
            m_stolen_voice_count--;
        }
        return {};
    }

    // Only now that the command is on its way: until it is, the slot still belongs to whoever was there.
    Voice& voice = m_voices[sound.index];
    voice.priority = desc.priority;
    voice.main_bus = command.desc.bus;
    voice.main_volume = command.desc.volume;
    voice.start_order = m_next_start_order++;
    voice.stop_requested = false;
    voice.pause_requested = command.desc.start_paused;
    return sound;
}

void Rndr::AudioMixer::Stop(SoundHandle sound)
{
    if (!IsLive(sound))
    {
        return;
    }
    // Ahead of the audio thread on purpose: a sound on its way out is the first one a new sound may take the
    // voice from, and that is true from the moment the stop is asked for.
    m_voices[sound.index].stop_requested = true;
    Command command;
    command.type = Command::Type::Stop;
    command.sound = sound;
    Send(command);
}

void Rndr::AudioMixer::StopAll()
{
    for (Voice& voice : m_voices)
    {
        voice.stop_requested = true;
    }
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
    m_voices[sound.index].pause_requested = true;
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
    m_voices[sound.index].pause_requested = false;
    Command command;
    command.type = Command::Type::Resume;
    command.sound = sound;
    Send(command);
}

void Rndr::AudioMixer::PauseAll()
{
    for (Voice& voice : m_voices)
    {
        voice.pause_requested = true;
    }
    Command command;
    command.type = Command::Type::PauseAll;
    Send(command);
}

void Rndr::AudioMixer::ResumeAll()
{
    for (Voice& voice : m_voices)
    {
        voice.pause_requested = false;
    }
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
    m_voices[sound.index].main_volume = ClampVolume(volume);
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

void Rndr::AudioMixer::ReclaimStreamSlots()
{
    const u64 epoch = m_mix_epoch.Load<Opal::MemoryOrder::Acquire>();
    for (StreamSlot& slot : m_streams)
    {
        if (slot.is_pending_free && epoch >= slot.free_after_epoch)
        {
            slot.samples = Opal::DynamicArray<f32>();
            slot.is_pending_free = false;
        }
    }
}

Opal::Expected<Rndr::AudioStreamHandle, Rndr::ErrorCode> Rndr::AudioMixer::CreateStream(const AudioStreamDesc& desc)
{
    using Result = Opal::Expected<AudioStreamHandle, ErrorCode>;

    if (desc.channel_count != 1 && desc.channel_count != 2)
    {
        RNDR_LOG_ERROR("AudioMixer: a stream has 1 or 2 channels, not {}", desc.channel_count);
        return Result(ErrorCode::InvalidArgument);
    }
    ReclaimStreamSlots();
    for (u32 i = 0; i < m_streams.GetSize(); i++)
    {
        StreamSlot& slot = m_streams[i];
        if (slot.is_live || slot.is_pending_free)
        {
            continue;
        }

        // The audio thread has not looked past is_known since the slot's last DestroyStream command, so everything
        // else can be reset in place; the CreateStream command publishes it.
        const u64 capacity_frames = desc.capacity_frames != 0 ? desc.capacity_frames : m_desc.sample_rate / 2;
        slot.samples = Opal::DynamicArray<f32>(capacity_frames * desc.channel_count);
        slot.channel_count = desc.channel_count;
        slot.capacity_frames = capacity_frames;
        slot.sound = {};
        slot.write_frame.Store<Opal::MemoryOrder::Relaxed>(0);
        slot.read_frame.Store<Opal::MemoryOrder::Relaxed>(0);
        slot.flush_frame.Store<Opal::MemoryOrder::Relaxed>(0);
        slot.end_frame.Store<Opal::MemoryOrder::Relaxed>(k_no_stream_end);
        slot.underrun_count.Store<Opal::MemoryOrder::Relaxed>(0);
        slot.is_dry = false;
        slot.mix_start_frame = 0;
        slot.mix_frame_count = 0;
        slot.history_first = 0;
        slot.history_count = 0;
        slot.clock_frame.Store(0);
        slot.clock_delay_frames.Store(0);
        slot.clock_limit_frame.Store(0);
        slot.clock_time.Store(0.0);

        Command command;
        command.type = Command::Type::CreateStream;
        command.source_index = i;
        if (!Send(command))
        {
            slot.samples = Opal::DynamicArray<f32>();
            RNDR_LOG_ERROR("AudioMixer: could not queue the stream creation");
            return Result(ErrorCode::OutOfResources);
        }
        slot.generation++;
        if (slot.generation == 0)
        {
            slot.generation = 1;
        }
        slot.is_live = true;
        return Result(AudioStreamHandle{i, slot.generation});
    }
    RNDR_LOG_ERROR("AudioMixer: every stream slot is in use");
    return Result(ErrorCode::OutOfResources);
}

const Rndr::AudioMixer::StreamSlot* Rndr::AudioMixer::FindStream(AudioStreamHandle handle) const
{
    if (!handle.IsValid() || handle.index >= m_streams.GetSize())
    {
        return nullptr;
    }
    const StreamSlot& slot = m_streams[handle.index];
    return slot.is_live && slot.generation == handle.generation ? &slot : nullptr;
}

Rndr::AudioMixer::StreamSlot* Rndr::AudioMixer::FindStream(AudioStreamHandle handle)
{
    return const_cast<StreamSlot*>(std::as_const(*this).FindStream(handle));
}

bool Rndr::AudioMixer::IsStreamValid(AudioStreamHandle handle) const
{
    return FindStream(handle) != nullptr;
}

void Rndr::AudioMixer::DestroyStream(AudioStreamHandle handle)
{
    ReclaimStreamSlots();
    StreamSlot* slot = FindStream(handle);
    if (slot == nullptr)
    {
        return;
    }
    Command command;
    command.type = Command::Type::DestroyStream;
    command.source_index = handle.index;
    if (!Send(command))
    {
        RNDR_LOG_ERROR("AudioMixer: could not queue the stream destruction, the stream stays live");
        return;
    }
    // Read after the send, for the reason DestroyClip gives.
    slot->is_live = false;
    slot->is_pending_free = true;
    slot->free_after_epoch = m_mix_epoch.Load<Opal::MemoryOrder::Acquire>() + 2;
}

Rndr::SoundHandle Rndr::AudioMixer::PlayStream(AudioStreamHandle stream, const PlaySoundDesc& desc)
{
    StreamSlot* slot = FindStream(stream);
    if (slot == nullptr)
    {
        RNDR_LOG_ERROR("AudioMixer: PlayStream called with a stream handle that is not live");
        return {};
    }
    if (IsLive(slot->sound))
    {
        RNDR_LOG_ERROR("AudioMixer: the stream's last sound is still playing, a stream plays one sound at a time");
        return {};
    }
    PlaySoundDesc stream_desc = desc;
    stream_desc.pitch = 1.0f;
    stream_desc.loop = false;
    slot->sound = StartVoice(stream.index, true, stream_desc);
    return slot->sound;
}

Rndr::u64 Rndr::AudioMixer::WriteStream(AudioStreamHandle stream, Opal::ArrayView<const f32> interleaved)
{
    StreamSlot* slot = FindStream(stream);
    if (slot == nullptr)
    {
        return 0;
    }
    const u64 channel_count = slot->channel_count;
    RNDR_ASSERT(interleaved.GetSize() % channel_count == 0, "AudioMixer: WriteStream takes whole frames");

    const u64 write_frame = slot->write_frame.Load<Opal::MemoryOrder::Relaxed>();
    const u64 read_frame = slot->read_frame.Load<Opal::MemoryOrder::Acquire>();
    const u64 free_frames = slot->capacity_frames - (write_frame - read_frame);
    const u64 frame_count = std::min(interleaved.GetSize() / channel_count, free_frames);

    // The free part of the ring may wrap past its end, so it is filled in two copies at most.
    const u64 start = write_frame % slot->capacity_frames;
    const u64 first_count = std::min(frame_count, slot->capacity_frames - start);
    std::copy_n(interleaved.GetData(), first_count * channel_count, slot->samples.GetData() + start * channel_count);
    std::copy_n(interleaved.GetData() + first_count * channel_count, (frame_count - first_count) * channel_count, slot->samples.GetData());

    if (frame_count > 0)
    {
        slot->end_frame.Store<Opal::MemoryOrder::Relaxed>(k_no_stream_end);
    }
    slot->write_frame.Store<Opal::MemoryOrder::Release>(write_frame + frame_count);
    return frame_count;
}

Rndr::u64 Rndr::AudioMixer::GetStreamFreeFrames(AudioStreamHandle stream) const
{
    const StreamSlot* slot = FindStream(stream);
    if (slot == nullptr)
    {
        return 0;
    }
    const u64 read_frame = slot->read_frame.Load<Opal::MemoryOrder::Acquire>();
    const u64 write_frame = slot->write_frame.Load<Opal::MemoryOrder::Acquire>();
    return slot->capacity_frames - (write_frame - read_frame);
}

void Rndr::AudioMixer::FlushStream(AudioStreamHandle stream)
{
    StreamSlot* slot = FindStream(stream);
    if (slot == nullptr)
    {
        return;
    }
    slot->end_frame.Store<Opal::MemoryOrder::Relaxed>(k_no_stream_end);
    slot->flush_frame.Store<Opal::MemoryOrder::Release>(slot->write_frame.Load<Opal::MemoryOrder::Acquire>());
}

void Rndr::AudioMixer::EndStream(AudioStreamHandle stream)
{
    StreamSlot* slot = FindStream(stream);
    if (slot == nullptr)
    {
        return;
    }
    slot->end_frame.Store<Opal::MemoryOrder::Release>(slot->write_frame.Load<Opal::MemoryOrder::Relaxed>());
}

Rndr::u64 Rndr::AudioMixer::GetStreamQueuedFrames(AudioStreamHandle stream) const
{
    const StreamSlot* slot = FindStream(stream);
    if (slot == nullptr)
    {
        return 0;
    }
    // The write end is read last: it is then at least as far as either of the others.
    const u64 read_frame = slot->read_frame.Load<Opal::MemoryOrder::Acquire>();
    const u64 flush_frame = slot->flush_frame.Load<Opal::MemoryOrder::Acquire>();
    const u64 write_frame = slot->write_frame.Load<Opal::MemoryOrder::Acquire>();
    return write_frame - std::max(read_frame, flush_frame);
}

Rndr::u64 Rndr::AudioMixer::GetStreamPosition(AudioStreamHandle stream, f64 time_seconds) const
{
    const StreamSlot* slot = FindStream(stream);
    if (slot == nullptr)
    {
        return 0;
    }
    u64 frame = 0;
    u64 delay_frames = 0;
    u64 limit_frame = 0;
    f64 clock_time = 0.0;
    while (true)
    {
        const u32 sequence = slot->clock_sequence.Load();
        if ((sequence & 1) != 0)
        {
            continue;
        }
        frame = slot->clock_frame.Load();
        delay_frames = slot->clock_delay_frames.Load();
        limit_frame = slot->clock_limit_frame.Load();
        clock_time = slot->clock_time.Load();
        if (slot->clock_sequence.Load() == sequence)
        {
            break;
        }
    }

    const f64 advance = (time_seconds - clock_time) * static_cast<f64>(m_desc.sample_rate) - static_cast<f64>(delay_frames);
    if (advance < 0.5)
    {
        return frame;
    }
    return frame + std::min(static_cast<u64>(advance + 0.5), limit_frame - frame);
}

Rndr::u32 Rndr::AudioMixer::GetStreamUnderrunCount(AudioStreamHandle stream) const
{
    const StreamSlot* slot = FindStream(stream);
    return slot != nullptr ? slot->underrun_count.Load<Opal::MemoryOrder::Relaxed>() : 0;
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

void Rndr::AudioMixer::MoveToFading(Voice& voice)
{
    Voice* ghost = nullptr;
    for (Voice& candidate : m_fading_voices)
    {
        if (!candidate.is_active)
        {
            ghost = &candidate;
            break;
        }
    }
    if (ghost == nullptr)
    {
        // Every ghost is still fading, which takes a slot stolen more than once inside one ramp. Cut the one
        // closest to silence rather than grow the array on this thread.
        ghost = &m_fading_voices[0];
        for (Voice& candidate : m_fading_voices)
        {
            if (candidate.gain_left + candidate.gain_right < ghost->gain_left + ghost->gain_right)
            {
                ghost = &candidate;
            }
        }
    }

    ghost->source_index = voice.source_index;
    ghost->is_stream = voice.is_stream;
    ghost->is_active = true;
    ghost->is_paused = false;
    ghost->is_stopping = true;  // a ghost exists only to ramp out
    ghost->loop = voice.loop;
    ghost->bus = voice.bus;
    ghost->position = voice.position;
    ghost->volume = voice.volume;
    ghost->pan = voice.pan;
    ghost->pitch = voice.pitch;
    ghost->gain_left = voice.gain_left;
    ghost->gain_right = voice.gain_right;

    // The slot itself is left as a finished voice would leave it, except for finished_generation: the main thread
    // has already moved that slot on to the sound about to fill it.
    voice.is_active = false;
    voice.is_paused = false;
    voice.is_stopping = false;
    voice.gain_left = 0.0f;
    voice.gain_right = 0.0f;
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
                // The slot was taken from a sound that is still playing. Hand what it was playing to a ghost so it
                // ramps out while this one ramps in, rather than cutting it here.
                MoveToFading(fresh);
            }
            if (command.is_stream)
            {
                // A ghost of the stream's last sound would read the ring alongside this one.
                for (Voice& ghost : m_fading_voices)
                {
                    if (ghost.is_active && ghost.is_stream && ghost.source_index == command.source_index)
                    {
                        ghost.is_active = false;
                    }
                }
            }
            fresh.playing_generation = command.sound.generation;
            fresh.source_index = command.source_index;
            fresh.is_stream = command.is_stream;
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
        case Command::Type::DestroyStream:
        {
            const bool is_stream = command.type == Command::Type::DestroyStream;
            for (Voice& v : m_voices)
            {
                if (v.is_active && v.is_stream == is_stream && v.source_index == command.source_index)
                {
                    FinishVoice(v);
                }
            }
            // Ghosts too: the memory is freed two mixes from now, and a ghost that is still ramping out would read
            // it after that on a small enough buffer.
            for (Voice& ghost : m_fading_voices)
            {
                if (ghost.is_active && ghost.is_stream == is_stream && ghost.source_index == command.source_index)
                {
                    ghost.is_active = false;
                }
            }
            if (is_stream)
            {
                m_streams[command.source_index].is_known = false;
            }
            break;
        }
        case Command::Type::CreateStream:
            m_streams[command.source_index].is_known = true;
            break;
        case Command::Type::None:
            break;
    }
}

Rndr::AudioMixer::GainRamp Rndr::AudioMixer::GetGainRamp(const Voice& voice, f32 master_volume) const
{
    f32 pan_left = 0.0f;
    f32 pan_right = 0.0f;
    PanGains(voice.pan, pan_left, pan_right);
    const f32 bus_volume = m_bus_volumes[voice.bus].Load<Opal::MemoryOrder::Relaxed>();
    const f32 gain = voice.is_stopping ? 0.0f : voice.volume * bus_volume * master_volume;
    GainRamp ramp;
    ramp.target_left = gain * pan_left;
    ramp.target_right = gain * pan_right;
    // Whatever the distance, a gain lands in one ramp length.
    ramp.step_left = std::abs(ramp.target_left - voice.gain_left) / static_cast<f32>(m_gain_ramp_frames);
    ramp.step_right = std::abs(ramp.target_right - voice.gain_right) / static_cast<f32>(m_gain_ramp_frames);
    return ramp;
}

void Rndr::AudioMixer::MixVoice(Voice& voice, Opal::ArrayView<f32> out, f32 master_volume)
{
    if (voice.is_stream)
    {
        MixStreamVoice(voice, out, master_volume);
        return;
    }
    const AudioClip& clip = m_clips[voice.source_index].clip;
    const Opal::ArrayView<const f32> samples = clip.GetData();
    const u64 clip_frame_count = clip.GetFrameCount();
    const u32 channel_count = clip.GetChannelCount();
    const u64 out_frame_count = out.GetSize() / 2;
    const GainRamp ramp = GetGainRamp(voice, master_volume);

    const f64 position_step = static_cast<f64>(voice.pitch) * static_cast<f64>(clip.GetSampleRate()) / static_cast<f64>(m_desc.sample_rate);
    const f64 clip_length = static_cast<f64>(clip_frame_count);

    for (u64 frame = 0; frame < out_frame_count; frame++)
    {
        voice.gain_left = Approach(voice.gain_left, ramp.target_left, ramp.step_left);
        voice.gain_right = Approach(voice.gain_right, ramp.target_right, ramp.step_right);

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

void Rndr::AudioMixer::MixStreamVoice(Voice& voice, Opal::ArrayView<f32> out, f32 master_volume)
{
    StreamSlot& stream = m_streams[voice.source_index];
    const Opal::ArrayView<const f32> samples(stream.samples.GetData(), stream.samples.GetSize());
    const u64 out_frame_count = out.GetSize() / 2;
    const GainRamp ramp = GetGainRamp(voice, master_volume);

    const u64 read_frame = stream.read_frame.Load<Opal::MemoryOrder::Relaxed>();
    const u64 available = stream.write_frame.Load<Opal::MemoryOrder::Acquire>() - read_frame;
    const u64 frame_count = std::min(available, out_frame_count);
    const bool reaches_end = read_frame + frame_count >= stream.end_frame.Load<Opal::MemoryOrder::Acquire>();

    // Past the frames there are, the gains keep ramping over silence, so a stop still finishes.
    u64 played_count = frame_count;
    bool is_finished = false;
    for (u64 frame = 0; frame < out_frame_count; frame++)
    {
        voice.gain_left = Approach(voice.gain_left, ramp.target_left, ramp.step_left);
        voice.gain_right = Approach(voice.gain_right, ramp.target_right, ramp.step_right);

        if (frame < frame_count)
        {
            const u64 index = (read_frame + frame) % stream.capacity_frames;
            f32 left = 0.0f;
            f32 right = 0.0f;
            if (stream.channel_count == 1)
            {
                left = samples[index];
                right = left;
            }
            else
            {
                left = samples[index * 2];
                right = samples[index * 2 + 1];
            }
            out[frame * 2] += left * voice.gain_left;
            out[frame * 2 + 1] += right * voice.gain_right;
        }

        if (voice.is_stopping && voice.gain_left == 0.0f && voice.gain_right == 0.0f)
        {
            played_count = std::min(frame + 1, frame_count);
            is_finished = true;
            break;
        }
    }
    stream.read_frame.Store<Opal::MemoryOrder::Release>(read_frame + played_count);
    stream.mix_frame_count += played_count;

    if (frame_count == out_frame_count || is_finished)
    {
        stream.is_dry = false;
    }
    else if (!stream.is_dry && !voice.is_stopping)
    {
        stream.is_dry = true;
        if (!reaches_end)
        {
            stream.underrun_count.Store<Opal::MemoryOrder::Relaxed>(stream.underrun_count.Load<Opal::MemoryOrder::Relaxed>() + 1);
        }
    }
    if (is_finished)
    {
        FinishVoice(voice);
    }
}

void Rndr::AudioMixer::BeginStreamMix(StreamSlot& stream)
{
    const u64 flush_frame = stream.flush_frame.Load<Opal::MemoryOrder::Acquire>();
    if (flush_frame > stream.read_frame.Load<Opal::MemoryOrder::Relaxed>())
    {
        stream.read_frame.Store<Opal::MemoryOrder::Release>(flush_frame);
    }
    stream.mix_start_frame = stream.read_frame.Load<Opal::MemoryOrder::Relaxed>();
    stream.mix_frame_count = 0;
}

void Rndr::AudioMixer::EndStreamMix(StreamSlot& stream, const AudioRenderTiming& timing)
{
    // A stream is read by one voice at a time, from the start of the buffer, so what this mix played is one run.
    if (stream.mix_frame_count > 0)
    {
        if (stream.history_count == k_stream_history_size)
        {
            stream.history_first = (stream.history_first + 1) % k_stream_history_size;
            stream.history_count--;
        }
        const u32 last = (stream.history_first + stream.history_count) % k_stream_history_size;
        stream.history[last] = {
            .output_frame = m_output_frame, .stream_frame = stream.mix_start_frame, .frame_count = stream.mix_frame_count};
        stream.history_count++;
    }

    const auto segment_at = [&stream](u32 i) -> const StreamSegment&
    { return stream.history[(stream.history_first + i) % k_stream_history_size]; };
    const auto segment_end = [](const StreamSegment& segment) { return segment.stream_frame + segment.frame_count; };
    // The last stream frame reachable from segment i without a gap in the output or a jump in the stream.
    const auto run_end = [&](u32 i)
    {
        u64 end = segment_end(segment_at(i));
        u64 output_end = segment_at(i).output_frame + segment_at(i).frame_count;
        for (u32 j = i + 1; j < stream.history_count; j++)
        {
            const StreamSegment& next = segment_at(j);
            if (next.output_frame != output_end || next.stream_frame != end)
            {
                break;
            }
            end = segment_end(next);
            output_end += next.frame_count;
        }
        return end;
    };

    // The output frame at the speaker when the device measured its queue: it holds that many ahead of this buffer.
    const i64 speaker = static_cast<i64>(m_output_frame) - static_cast<i64>(timing.queued_frames);

    // Segments the speaker is past are of no more use, but the last of them says where a gap after it starts.
    while (stream.history_count >= 2 && static_cast<i64>(segment_at(1).output_frame + segment_at(1).frame_count) <= speaker)
    {
        stream.history_first = (stream.history_first + 1) % k_stream_history_size;
        stream.history_count--;
    }

    // With nothing played, the speaker is wherever reading would start.
    u64 frame = stream.mix_start_frame + stream.mix_frame_count;
    u64 delay_frames = 0;
    u64 limit_frame = frame;
    if (stream.history_count > 0)
    {
        const StreamSegment& newest = segment_at(stream.history_count - 1);
        frame = segment_end(newest);
        limit_frame = frame;
        for (u32 i = 0; i < stream.history_count; i++)
        {
            const StreamSegment& segment = segment_at(i);
            const auto output_frame = static_cast<i64>(segment.output_frame);
            if (speaker < output_frame)
            {
                // In the silence before this segment: held where the last one ended, and moving once the speaker
                // reaches this one if it carries straight on from there.
                frame = i > 0 ? segment_end(segment_at(i - 1)) : segment.stream_frame;
                limit_frame = frame;
                if (frame == segment.stream_frame)
                {
                    delay_frames = static_cast<u64>(output_frame - speaker);
                    limit_frame = run_end(i);
                }
                break;
            }
            if (speaker < output_frame + static_cast<i64>(segment.frame_count))
            {
                frame = segment.stream_frame + static_cast<u64>(speaker - output_frame);
                limit_frame = run_end(i);
                break;
            }
        }
    }

    const u32 sequence = stream.clock_sequence.Load();
    stream.clock_sequence.Store(sequence + 1);
    stream.clock_frame.Store(frame);
    stream.clock_delay_frames.Store(delay_frames);
    stream.clock_limit_frame.Store(limit_frame);
    stream.clock_time.Store(timing.time_seconds);
    stream.clock_sequence.Store(sequence + 2);
}

void Rndr::AudioMixer::Mix(Opal::ArrayView<f32> out, const AudioRenderTiming& timing)
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

    for (StreamSlot& stream : m_streams)
    {
        if (stream.is_known)
        {
            BeginStreamMix(stream);
        }
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

    // Sounds whose slot was taken, playing out their last ramp. They are nobody's voice any more, so they are not
    // counted and nothing can address them.
    for (Voice& ghost : m_fading_voices)
    {
        if (ghost.is_active)
        {
            MixVoice(ghost, out, master_volume);
        }
    }

    const u64 out_frame_count = out.GetSize() / 2;
    for (StreamSlot& stream : m_streams)
    {
        if (stream.is_known)
        {
            EndStreamMix(stream, timing);
        }
    }
    m_output_frame += out_frame_count;

    for (f32& sample : out)
    {
        sample = std::clamp(sample, -1.0f, 1.0f);
    }

    m_active_voice_count.Store<Opal::MemoryOrder::Relaxed>(active_count);
    m_mix_epoch.Store<Opal::MemoryOrder::Release>(m_mix_epoch.Load<Opal::MemoryOrder::Relaxed>() + 1);
}
