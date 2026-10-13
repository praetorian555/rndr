# Video playback plan

## Context

A video player is being built as its own application, in `D:/Dev/video-player`, on Facet and rndr. Its end goal is
to play films and series streamed from Real-Debrid: an HTTPS link that answers range requests, usually to a Matroska
file holding HEVC 10-bit video (sometimes H.264 or AV1, sometimes HDR), AAC, AC3, E-AC3, DTS or TrueHD audio, and
text or bitmap subtitles. A second goal is to learn Vulkan Video, so the desktop decodes through it rather than
through a platform or software decoder.

This document covers what rndr gains for that, and only that. rndr holds the parts an engine would want for any
video or streamed sound: images with several planes, sampling them as YCbCr, Vulkan Video sessions and decode
commands, importing an Android hardware buffer, and audio that is fed while it plays. Everything that knows about a
codec's bitstream, a container, a network or Real-Debrid stays in the player.

Decisions:

- **Two decode paths.** On the desktop, Vulkan Video decodes on the device's decode queue into images Forge samples
  directly. On Android, `AMediaCodec` decodes into an `AHardwareBuffer` that Forge imports, so the frame never
  leaves the GPU there either. Android drivers rarely offer Vulkan Video; if the phone turns out to (see Open
  questions), the desktop path can run there as well, but the MediaCodec path stays as the one that always works.
- **Audio is decoded in the player, played by rndr.** AC3, DTS and TrueHD decoders have no place in an engine, so
  the player decodes with FFmpeg's libavcodec on the desktop and `AMediaCodec` on Android. rndr gains a sound fed
  with decoded samples while it plays, a way to ask how far playback has got (the clock the video follows), and the
  Android output device it does not have yet.
- **The bitstream parser lives in the player.** Vulkan Video takes the sequence and picture parameters and each
  slice's decoded header as structures (`StdVideoH264SequenceParameterSet`, `StdVideoDecodeH264PictureInfo` and so
  on); reading them out of the bitstream, computing picture order and deciding which reference goes in which slot is
  the codec's business, not Forge's. Forge wraps the Vulkan objects and commands and checks what Vulkan requires of
  them.
- **H.264 first, then H.265, then AV1.** H.264 is the simplest codec to parse by hand and the one the learning starts
  on. Most Real-Debrid content needs H.265 Main10, which is the second step and the one that needs 10-bit planes.
- Error reporting follows the repo's convention throughout: a static `Create` returning `Opal::Expected<T,
  ErrorCode>`, methods returning `ErrorCode`, the detail in the log, nothing thrown (see [forge.md](forge.md)).

Hardware checked on 2026-10-11: the desktop's NVIDIA GeForce RTX 5060 Ti (driver API 1.4.351) offers
`VK_KHR_video_queue`, `VK_KHR_video_decode_queue`, decode of H.264, H.265, AV1 and VP9, and `VK_KHR_video_maintenance1`
and `2`. The phone was not connected and has not been checked.

Phase 0 was verified on 2026-10-11 on that card, with the validation layer on: the whole `[forge]` suite passes in the
Vulkan 1.3 build and in the 1.1 one. The card is used at 1.3 in both, so the 1.1 build's path for a device below 1.3 -
the feature read through `VkPhysicalDeviceSamplerYcbcrConversionFeatures` - has not run on a device that needs it.

## Status

| Phase | State |
|---|---|
| 0 - planes and YCbCr sampling | **Done**: textures filled a plane at a time, `SamplerYcbcrConversion`, the `[forge]` case and `samples/video-frame`; described in [forge.md](forge.md#video-frames). NV12, P010 and I420 are each checked against the CPU |
| 1 - Vulkan Video decode | **H.264 decodes end to end**: decode queue by codec, capabilities, video buffers and textures, `VideoSession`, H.264 `VideoSessionParameters`, the coding scope and `CmdDecodeVideo`, described in [forge.md](forge.md#video-decode); the player decodes thirteen ITU-T conformance streams bit for bit - CAVLC and CABAC, I, P and B pictures, several slices a picture, temporal and spatial direct, explicit and implicit weighted prediction, hierarchical B pictures that are references, `frame_num` wrapping - each with a status query that says the device decoded it (one that, on NVIDIA, also says so of
a buffer of zeros). Long-term references are marked as H.264 says, but NVIDIA's Vulkan driver does not decode a picture that uses one (see [forge.md](forge.md#video-decode)), so MR1_BT_A, MR2_TANDBERG_E and MR4_TANDBERG_C stay out of the tests. **H.265 Main and Main 10 decode end to end** through H.265 `VideoSessionParameters` and `CmdDecodeVideo`: the player decodes twenty-four ITU-T conformance streams bit for bit - CABAC, SAO, deblocking, transform skip, scaling lists, weighted prediction, tiles, wavefronts, dependent slice segments, reference picture sets in the SPS and in slice headers, long-term references (which the driver decodes for H.265), CRA and BLA pictures, every NAL unit type, the output process, 2560x1600, and 10 bits into P010. Left: gaps in H.264's `frame_num`, which no stream here checks without long-term references too, and AV1 |
| 2 - Android hardware buffer import | Not started |
| 3 - streamed audio | **Done**: streams in the mixer - a ring buffer the player writes from its own thread, dry spells counted and an end the writer marks not counted as one, pause, a flush for a seek, and a position that takes in what the device holds - and an AAudio device behind `AudioDevice::Create`; described in [audio.md](audio.md#streams). Checked through WASAPI on the desktop and AAudio on the emulator; the reopening after a lost output on Android has not been through a device |

The phases are in the order they are needed. Phase 0 is the ground both decode paths stand on. Phase 1 can start once
a still frame shows through phase 0, and phase 2 can be done before or after it. Phase 3 is independent of the other
three, and the player needs it before it can play anything with sound.

## Phase 0 - planes and YCbCr sampling

Decoders give out frames in a layout of planes: NV12 is a full-size plane of luma followed by a half-size plane of
interleaved blue and red chroma, and P010 is the same with 10 bits kept in each 16-bit sample. Vulkan holds such a
frame as one image with a format of several planes, and samples it as RGB through a sampler that carries a YCbCr
conversion. Both are core in Vulkan 1.1, so they work on the Vulkan 1.1 build as well.

What Forge gains:

- `PixelFormat` values for `G8_B8R8_2PLANE_420_UNORM` (NV12), `G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16` (P010)
  and `G8_B8_R8_3PLANE_420_UNORM` (I420, which software decoders produce), mapped to their `VkFormat`.
- A `sampler_ycbcr_conversion` field in `DeviceFeatures`.
- A `SamplerYcbcrConversion` object, created from a description of the format, the colour model (BT.601, BT.709,
  BT.2020), the range (narrow or full), where the chroma samples sit, and the chroma filter. On Android the format
  can instead be the external format a hardware buffer reports (phase 2).
- A sampler and a texture view that take such a conversion. Vulkan requires a sampler with a conversion to be an
  immutable sampler of its set layout, which `DescriptorSetLayout` can already express; Forge refuses one written as
  an ordinary sampler, with a log line that says why.
- Uploads to a single plane: a copy from a buffer into plane 0 or plane 1, with the barriers naming the plane's
  aspect. A query of what the format supports on this device (whether its chroma can be sampled with a linear
  filter, whether the chroma is cosited or midpoint) so the caller can build a conversion the device accepts.

Checked by a `[forge]` case: upload a 16x16 frame of sixteen known colours, sample it in a compute shader through a
conversion, and compare every colour with the same conversion worked out on the CPU, within one step - NV12 through
BT.709 narrow range and BT.601 full range, P010 through BT.2020 narrow and full range at 10 bits, and I420 filled by
three copies. It also checks that each rule above is refused, skips a format the device cannot sample, and ends with
no validation error. A sample, `samples/video-frame`, shows one still frame in a window in each of the three formats.

## Phase 1 - Vulkan Video decode

What Forge gains, in the order a decode needs it:

- **A decode queue that can decode the codec.** `DeviceDesc::use_decode_queue` already takes a decode family. The
  family also has to support the codec, which `VkQueueFamilyVideoPropertiesKHR` tells, so the selection takes the
  codecs wanted and the log names the one a device lacks.
- **A video profile and its capabilities.** A `VideoProfile` names the codec, chroma subsampling, bit depth and the
  codec's own profile (H.264 High, H.265 Main10). Forge asks the device for its capabilities under that profile -
  the largest picture, the number of reference slots, the alignment of the bitstream buffer - and for the formats it
  can decode into, including whether the decoded picture buffer and the output can be one image.
- **`VideoSession`.** Created for a profile, a size and a number of reference slots, with its memory bound the way
  the device asks for it.
- **`VideoSessionParameters`.** Holds the codec's parameter sets (for H.264, its SPS and PPS) as the caller hands
  them over, and takes new ones as a stream brings them.
- **Buffers and images for video.** A buffer the bitstream is decoded from, and images for the decoded picture
  buffer and the output, each created with the profile it serves, as Vulkan requires.
- **Commands.** `CmdBeginVideoCoding`, `CmdControlVideoCoding` (to reset the session before the first frame),
  `CmdDecodeVideo` with the picture's parameters and its references, `CmdEndVideoCoding`, and a status query of the
  decode's result through the existing query pools.
- **Synchronization.** The decode queue and the graphics queue are usually different families, so a decoded picture
  moves from one to the other by a queue family ownership transfer, with the synchronization2 stages and accesses
  of video decode.

What stays in the player: reading NAL units, decoding parameter sets and slice headers into the `StdVideo*`
structures, computing picture order, marking references and choosing their slots, and putting the decoded frames
back into display order.

Checked by `[forge]` cases where a device can: the capabilities of an H.264 profile, a session and its parameters
created and destroyed, the bitstream buffer refused when it breaks the alignment the device asks for. A decode from
end to end needs a parser, so it is checked in the player against frames FFmpeg decoded from the same file. Whether
a short stream with its parameters written out by hand is worth adding as a `[forge]` fixture is an open question.

After H.264, the same objects take H.265 (Main and Main10, the latter decoding into P010 from phase 0) and AV1. Each
adds its profile and parameter structures, not new objects.

## Phase 2 - Android hardware buffer import

On Android the player decodes with `AMediaCodec` into an `AImageReader`, whose images are `AHardwareBuffer`s. Forge
imports one as a texture:

- The extensions `VK_ANDROID_external_memory_android_hardware_buffer` and `VK_EXT_queue_family_foreign`, with what
  they depend on.
- `Texture::Create` from an `AHardwareBuffer*`: Forge asks for its properties, which give either an ordinary format
  or an external format only the driver understands, creates the image with that format and the external memory
  flags, and imports the buffer's memory into a dedicated allocation.
- A YCbCr conversion over the external format (phase 0), which is the only way such an image can be sampled.
- The acquire from the foreign queue family before the first use in a frame.

The image does not own the buffer. The player keeps the `AImage` alive until a fence says the GPU is done with the
frame, then releases it back to the reader; the documentation of the call says so.

This is checked on a device rather than in `[forge]`, which does not run on Android yet: the player plays a clip on
the phone with the validation layer on, and the emulator if its GPU offers the extension.

## Phase 3 - streamed audio

`docs/audio-roadmap.md` item 8 plans streaming as a decoder reading from disk. The player needs the more general
half of it: a sound whose samples the caller supplies while it plays.

- A streamed sound: a ring buffer of f32 frames at the device's rate, written from the player's decode thread and
  read by the mixer. Running dry plays silence and counts the times it did, rather than stalling the audio thread.
  Pausing holds the read position; a seek empties the buffer.
- The position of a streamed sound in frames, including what the device holds but has not played yet, so the player
  can show each video frame when the sound reaches its time.
- An AAudio device behind `AudioDevice::Create` for Android, which until now stops at `#error`. Any game on Android
  needs it too; [android-plan.md](android-plan.md) lists it among what was left for later.

Checked by `[audio]` cases driven through `AudioMixer::Mix` as the existing ones are: written frames come out in
order, running dry gives silence and counts it, the position advances by what was mixed.

## What stays in the player

The Real-Debrid client (logging in with the device code flow, listing torrents and downloads, unrestricting a
link), the HTTPS reader with range requests, the Matroska and MP4 demultiplexers, the codec parsers, audio decoding,
the clock that paces frames against the audio position, subtitles, tone mapping HDR to the display, and the user
interface.

## Planned beyond the phases

- **Measuring how long a frame takes to reach the screen.** The sound's position takes in what the audio device holds,
  but a picture made current in an update reaches the screen only when the swap chain shows that frame, some frames
  later. The player makes each picture current that much early, and estimates the delay as two ticks of its loop.
  The measured delay replaces the estimate: Forge presents with `VK_EXT_present_timing`, which reports when the first
  pixel of a present went out (the desktop NVIDIA driver offers it, with that stage), and maps that time onto
  `Opal::GetSeconds` through the swap chain's time domain; Facet keeps, for each frame, the time its tick began and
  gives the window the smoothed time from a tick to its first pixel. Where the extension is missing - Android, unless
  `VK_GOOGLE_display_timing` or the choreographer's expected presentation time stands in - the estimate stays.
- **Colour from the bitstream.** The player takes a picture's matrix, range, transfer and primaries from the Matroska
  `Colour` element alone, and guesses BT.709 above 720 lines when it is missing. Many remuxes carry the colour only in
  the sequence parameter set's VUI: a 4K HEVC clip whose VUI says BT.2020 is drawn through the BT.709 matrix, and an
  HDR10 file tagged the same way would lose its PQ transfer. The H.264 and H.265 parsers read `video_signal_type` from
  the VUI, and what the container leaves unspecified is taken from there.
- **A clock that stops at the end.** Once the last picture is shown and the sound has played out, the clock carries
  on with the wall clock, so the position runs past the duration for as long as the file stays open. It stops where
  the file ends.
- **Test files with their real times.** The Matroska files the player's tests mux from the conformance streams give
  each packet its place in decoding order as its time, so the streams with B pictures show a third to a half of their
  pictures out of order and the player drops them as late. They are muxed again with the times the pictures are shown
  at.

## Open questions

- Whether the phone offers Vulkan Video at all: `adb shell cmd gpu vkjson` lists its extensions. If it does, the
  desktop path can run there too.
- Whether every extension Vulkan Video depends on is available on the Vulkan 1.1 build, or whether video is a Vulkan
  1.3 build feature only. To be settled when phase 1 starts, against the extension dependencies in the registry.
- HDR output: tone mapping to SDR in the player's shader comes first. Presenting HDR10 through a swap chain colour
  space is a later Forge feature, if it is wanted.
- Whether a hand-written H.264 stream earns its place as a `[forge]` fixture, or decode stays checked in the player
  only.
