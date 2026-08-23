# Audio

Basic sound for games: load a clip, play it with volume, pan and pitch, loop it, route it through a volume bus,
pause everything. Two-dimensional only, everything decoded into memory. It is on by default (`RNDR_AUDIO`) and on
Windows plays through WASAPI.

The public types live under `include/rndr/audio/`:

| Type | What it is |
|---|---|
| `AudioClip` | Decoded PCM: interleaved f32 frames, mono or stereo, at a fixed rate. The `Bitmap` of audio. |
| `AudioSystem` | What a game holds. Owns the clips, plays sounds, runs the output device. |
| `AudioMixer` | The engine under `AudioSystem`: voices, commands, the mix. Has no thread; drive it yourself to test. |
| `AudioDevice` | The platform half: a thread that pulls stereo f32 from a callback and pushes it to the endpoint. |

---

## Using it

```cpp
#include "rndr/audio/audio-system.hpp"

Rndr::AudioSystem audio;   // throws Rndr::AudioDeviceException when there is no output device

const Rndr::AudioClipHandle jump = audio.LoadClip("assets/audio/jump.wav");
const Rndr::AudioClipHandle music = audio.LoadClip("assets/audio/theme.ogg");

audio.SetBusVolume(1, 0.6f);
const Rndr::SoundHandle theme = audio.Play(music, {.loop = true, .bus = 1});

// Later, from gameplay:
audio.Play(jump, {.volume = 0.8f, .pan = -0.3f, .pitch = 1.1f});
audio.SetMasterVolume(0.5f);
audio.PauseAll();   // pause menu
audio.ResumeAll();
audio.Stop(theme);
```

Nothing needs to be called every frame. The audio thread runs on its own; the main thread only queues commands.

### Clips

`File::LoadAudioClip` reads `.wav` (PCM 8/16/24/32-bit and 32-bit float, plain or `WAVE_FORMAT_EXTENSIBLE`) and
`.ogg` (Vorbis). Both refuse more than two channels rather than downmix. `File::DecodeAudioClip` runs a decoder on
a file already in memory, which is how the decoder tests avoid the disk.

An `AudioClip` is plain data until `AudioSystem::CreateClip` takes it - the system owns the sample memory from then
on and hands back an `AudioClipHandle`. `LoadClip` does both steps. `DestroyClip` stops every sound on the clip and
frees it once the audio thread is provably done reading it; it is safe to call while sounds are playing.

Clips are resampled to the mix rate on the way through (linear interpolation), so a 44.1 kHz file plays correctly
through the default 48 kHz mix. Decoding to the mix rate ahead of time is still the better choice for anything
played often.

### Sounds

`Play` returns a `SoundHandle` and the sound starts on the next audio callback. Handles are generational: once the
sound ends or is stopped, the handle is stale and every call that takes it - `Stop`, `SetVolume`, `Pause`, ... - is
a no-op. A game can keep a handle around and never check it. `IsPlaying` is true while the voice is alive, which
includes paused and not-yet-started.

`Play` returns an invalid handle, and logs why, when the clip handle is stale, the command queue is full, or every
voice is busy with sounds it may not take one from. It never throws.

`PlaySoundDesc` and the setters take `volume` (linear gain, `1` is unity), `pan` in `[-1, 1]` (constant-power; acts
as balance on a stereo clip), `pitch` as a rate multiplier (clamped to `[1/16, 16]`), `loop`, `start_paused`,
`priority` and a `bus` index. Gain changes and stops are ramped over 2 ms so they do not click.

### Priority and stealing

There are `max_voices` (default 64) voices and a busy fight wants more, so a sound takes a voice from a sound that
matters less. A sound already on its way out goes first. Failing that, only sounds of equal or lower `priority`
(default 128) are candidates - a higher priority is never interrupted - and among them the lowest priority goes,
then the quietest, then the one that has been playing longest. A paused sound is never taken: it is silent now and
wanted back when it resumes. With nothing to take, `Play` refuses.

A stolen sound ends: its handle is stale immediately, exactly as if it had finished, so a game holding one needs no
new check. What it was playing keeps mixing for one 2 ms ramp while the new sound fades in, so the swap is a
crossfade and not a click. `GetStolenVoiceCount` counts the sounds this has happened to - a number that climbs
fast means `max_voices` is too low or something is playing more copies of one sound than anyone can hear.

### Buses and master volume

There are `k_audio_bus_count` (8) buses. Bus 0 is the default; what the rest mean is up to the game - SFX, music,
voice, UI. A sound's gain is `volume * bus_volume * master_volume`. Bus and master volume are applied on the audio
thread without going through the command queue, so they take effect on the next callback even when the queue is
full.

---

## How it works

```
main thread                                       audio thread (WindowsAudioDevice)
-----------                                       ---------------------------------
AudioSystem / AudioMixer                          WASAPI event loop
  Play, Stop, SetVolume, ...  --SPSC queue-->       drain commands
  CreateClip, DestroyClip                           mix every active voice into the endpoint buffer
  IsPlaying, GetActiveVoiceCount  <--atomics--      finished generations, mix epoch
```

The split is strict. Voice state belongs to the audio thread and is only changed by the commands it drains. Clip
storage belongs to the main thread. Master and bus volumes and the per-voice "finished" generations are atomics.
The audio thread never allocates, locks or blocks.

**Voice slots.** The main thread allocates a voice slot when `Play` is called - so it can hand back a handle at
once - and bumps the slot's generation. The audio thread writes that generation into an atomic when the voice ends.
A slot is free when the two agree. That one atomic per voice is the whole of `IsPlaying`.

**Clip lifetime.** `DestroyClip` queues a stop for every voice on the clip and records the mix epoch (a counter the
audio thread bumps after every callback). The slot is reclaimed on the main thread once two more mixes have
completed: by then the command has been drained and the callback that drained it has finished. Reclaiming happens
lazily inside `CreateClip` and `DestroyClip`, so the sample memory is only ever freed on the main thread and there
is no per-frame call.

**Command queue.** `Opal::ChannelSPSC`, capacity from `AudioSystemDesc::command_queue_capacity` (1024). The main
thread uses `TrySend` and never blocks on the audio thread; when the queue is full - over a thousand calls between
two callbacks, roughly 10 ms apart - the command is dropped with a warning and `GetDroppedCommandCount` goes up. A
dropped `Play` hands its voice back and returns an invalid handle; a dropped `DestroyClip` leaves the clip live so
it can be tried again.

**The device.** `WindowsAudioDevice` opens the default render endpoint in shared, event-driven mode, asking for
stereo f32 at the mix rate with `AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM`, so Windows converts to whatever the endpoint
runs at and the mixer never has to adapt. All COM work happens on the audio thread; the thread that builds the
device never needs `CoInitializeEx`. The constructor waits for the thread's first attempt at the stream and throws
`AudioDeviceException` if it failed. When the endpoint disappears - headphones unplugged, default device changed -
the thread reopens against the new default every half second; the mixer's state is untouched, so playback carries
on. The thread asks MMCSS for "Pro Audio" scheduling, best effort.

---

## Tests

`[audio]` is headless and deterministic: the decoders against WAV images built in the test and the OGG fixture
under `assets/audio/`, and the mixer driven directly - `Play`, then `Mix` into a buffer, then look at the samples.
`[audio-device]` opens the real endpoint, plays through it and checks timing. It skips on a machine with no output
device, which from the outside looks like a pass; set `RNDR_TEST_REQUIRE_AUDIO=1` to make that a failure.

    ./build/msvc-debug/Debug/rndr-test.exe "[audio]"
    RNDR_TEST_REQUIRE_AUDIO=1 ./build/msvc-debug/Debug/rndr-test.exe "[audio-device]"

`audio-sample` is a window that makes noises: Space for a panned blip, O for the OGG fixture, M for a looping chord
on its own bus, Up/Down for master volume, P to pause everything.

---

## Not here yet

- **Streaming.** Every clip is decoded whole. Five minutes of stereo at 48 kHz is 110 MB as f32; long music wants
  a decoder fed from disk on the audio thread.
- **3D.** No listener, no distance attenuation, no spatial pan. A game can compute pan and volume from positions
  and set them per sound.
- **Effects and sends.** No filters, no reverb, no bus-to-bus routing beyond a single gain per bus.
- **Other platforms.** `AudioDevice::Create` is where another backend would be picked; the mixer does not care.
