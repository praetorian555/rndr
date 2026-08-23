# Audio roadmap

What an RTS needs from the audio system beyond what [audio.md](audio.md) describes today: clips, voices with
volume/pan/pitch/loop and priority-based stealing, eight buses, master volume, pause-all, and a WASAPI device.
Ordered by how much a game hurts without it. Each item names the layer it lands in, since the mixer is the part
that runs on the audio thread and the part the tests drive.

---

## Must have

### 1. Per-clip instance cap and retrigger cooldown

Forty riflemen firing on the same frame sum forty copies of one clip: clipping, mush, and forty voices gone.

- `AudioClipDesc` at `CreateClip` (or per-clip setters): `max_instances` (default 4) and `min_retrigger_seconds`
  (default 0).
- `Play` returns an invalid handle, without logging, when the cap is hit or the last start of that clip is too
  recent.
- Instance counts live on the main thread next to voice slots, so no audio-thread change.

Layer: mixer.

### 2. Camera-relative positional audio

A top-down game wants sounds panned by where they are on screen and quieter the further they are from the camera
focus. Nothing spatial beyond that - no HRTF, no doppler.

- `AudioSystem::SetListener(Vector2f position, f32 half_width, f32 half_height)` - the world-space rectangle the
  camera shows, updated per frame as the camera moves and zooms.
- `PlaySoundDesc::position` (optional `Vector2f`), `SetPosition(sound, Vector2f)`.
- Pan from the sound's x relative to the listener rectangle; attenuation from distance to the rectangle's edge
  (inside: full volume; beyond `falloff_distance`: silence). Sounds farther than that are not started at all.
- `PlaySoundDesc::is_positional = false` for UI and alerts, which ignore the listener.

Layer: listener state is an atomic-ish snapshot read by the audio thread; pan and gain are recomputed per mix from
position. Sounds that move (`SetPosition`) go through a command like the other setters.

### 3. Long fades

Every gain change lands in 2 ms today. Music crossfades, ambient swaps and level transitions need seconds.

- `FadeTo(sound, volume, seconds)`, `FadeOutAndStop(sound, seconds)`.
- `FadeBusTo(bus, volume, seconds)`.
- A fade is a target and a rate on the voice or bus, advanced per mix; a new fade replaces the old one.

Layer: mixer. Bus fades move the bus gain onto the audio thread (currently an atomic written by the main thread).

### 4. Per-bus pause

Game pause must freeze the world and keep the menu alive.

- `PauseBus(bus)`, `ResumeBus(bus)`.
- `PauseAll` / `ResumeAll` stay and pause every bus.
- A paused bus holds its voices where they are; voices started on a paused bus wait.

Layer: mixer, one flag per bus on the audio thread.

---

## Should have

### 5. Exclusive groups

Unit acknowledgements - "yes, commander" - are one at a time: a new line interrupts the previous one, and
clicking a unit ten times should not queue ten.

- `PlaySoundDesc::exclusive_group` (u8, 0 = none). Playing into a group stops whatever is playing in it.
- Combine with item 1's cooldown on the clip for the spam case.

Layer: mixer, main-thread bookkeeping of "current sound per group".

### 6. Ducking

Voice lines and "base under attack" alerts should pull music and world sfx down while they play.

- `PlaySoundDesc::duck_buses` (bit mask) and `duck_amount` (gain, default 0.5).
- The ducked buses fade down while any ducking sound is alive and back up afterwards, over item 3's fades.

Layer: mixer, on top of items 3 and 4.

### 7. Variation sets

"Rifle fire" is four clips and a little pitch jitter, not one clip.

- `AudioSystem::CreateVariationSet(clips, pitch_jitter, volume_jitter)` returning a handle `Play` accepts like a
  clip; picks at random, never the same one twice in a row.
- Can live entirely on the main thread above the mixer. A game can also do this itself; the helper is here so
  every game does not.

Layer: system.

### 8. Music that does not live in memory as f32

Five minutes of stereo at 48 kHz is 110 MB as f32. Two tracks and the OGG decode at level start is the longest
thing in the load.

- Short term: store clips as i16 and convert on read in the mixer. Halves memory, costs one multiply per sample.
- Real fix: streaming. A voice bound to a streamed clip reads from a ring buffer that a loader thread (or the main
  thread, on a `Update()` call) keeps ahead of it by decoding from disk. The decoder has to be seekable for loops.

Layer: clip + mixer + a loader. Largest item on the list.

---

## Nice

### 9. Alert bus convention

Nothing to build - a naming convention: bus 0 world sfx, 1 music, 2 UI, 3 voice, 4 alerts, with UI/voice/alerts
non-positional. Worth writing down in `audio.md` once items 2 and 4 exist.

### 10. Bus meters

Peak level per bus and master, written by the audio thread each mix, read for a debug overlay. Tells a designer
which bus is clipping.

### 11. Asynchronous clip loading

`LoadClip` decodes on the calling thread; a long OGG takes hundreds of milliseconds. Fine inside a loading screen.
If a game wants to load during play: decode on a worker (Opal has a thread pool) and hand the finished `AudioClip`
to `CreateClip` on the main thread.

---

## Suggested order

Items 1, 2 and 4 are all mixer-side and small on their own; they make one pass, with tests driven through
`AudioMixer::Mix` like the existing ones. Items 3, 5 and 6 are a second pass, since 6 depends on 3 and 4. Item 8
stands alone and is the one that changes the most.
