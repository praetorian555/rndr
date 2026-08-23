/**
 * Audio sample: a window that makes noises.
 *
 *   Space       a short blip, panned somewhere at random
 *   O           the OGG fixture under assets/audio, through the music bus
 *   M           start or stop a looping chord on the music bus
 *   Up / Down   master volume
 *   P           pause or resume everything
 *   Escape      quit
 *
 * Every clip but the OGG is generated here, so the sample needs no sound files to run.
 */

#include <algorithm>
#include <cmath>

#include "opal/container/dynamic-array.h"
#include "opal/exceptions.h"
#include "opal/paths.h"
#include "opal/rng.h"
#include "opal/time.h"

#include "rndr/application.hpp"
#include "rndr/audio/audio-system.hpp"
#include "rndr/exception.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/input-system.hpp"
#include "rndr/log.hpp"
#include "rndr/types.hpp"

namespace
{

constexpr Rndr::u32 k_rate = 48000;
constexpr Rndr::f32 k_pi = 3.14159265358979323846f;

enum Bus : Rndr::u8
{
    k_sfx_bus = 0,
    k_music_bus = 1,
};

/** A sine that decays to nothing over its length: a blip. */
Rndr::AudioClip MakeBlip(Rndr::f32 frequency, Rndr::f32 seconds)
{
    using namespace Rndr;
    const auto frame_count = static_cast<u64>(seconds * k_rate);
    Opal::DynamicArray<f32> samples(frame_count);
    for (u64 i = 0; i < frame_count; i++)
    {
        const f32 t = static_cast<f32>(i) / k_rate;
        const f32 envelope = 1.0f - static_cast<f32>(i) / static_cast<f32>(frame_count);
        samples[i] = 0.6f * envelope * envelope * std::sin(2.0f * k_pi * frequency * t);
    }
    return AudioClip(k_rate, 1, {samples.GetData(), samples.GetSize()});
}

/** A slow major chord, stereo, with the fifth leaning left and the third leaning right so the loop has some width. */
Rndr::AudioClip MakeChord(Rndr::f32 root, Rndr::f32 seconds)
{
    using namespace Rndr;
    const auto frame_count = static_cast<u64>(seconds * k_rate);
    Opal::DynamicArray<f32> samples(frame_count * 2);
    const f32 third = root * 5.0f / 4.0f;
    const f32 fifth = root * 3.0f / 2.0f;
    for (u64 i = 0; i < frame_count; i++)
    {
        const f32 t = static_cast<f32>(i) / k_rate;
        const f32 a = std::sin(2.0f * k_pi * root * t);
        const f32 b = std::sin(2.0f * k_pi * third * t);
        const f32 c = std::sin(2.0f * k_pi * fifth * t);
        samples[i * 2] = 0.15f * (a + 0.5f * b + c);
        samples[i * 2 + 1] = 0.15f * (a + b + 0.5f * c);
    }
    return AudioClip(k_rate, 2, {samples.GetData(), samples.GetSize()});
}

}  // namespace

int main()
{
    using namespace Rndr;

    const ApplicationDesc app_desc{.enable_input_system = true};
    auto app = Application::Create(app_desc);
    RNDR_ASSERT(app.IsValid(), "Failed to create Rndr app!");

    const GenericWindowDesc window_desc{.name = "Audio Sample - Space: blip, O: ogg, M: loop, Up/Down: volume, P: pause"};
    auto window = app->CreateGenericWindow(window_desc);
    RNDR_ASSERT(window.IsValid(), "Failed to create a window!");

    // A game that can run without sound catches this and carries on; a sample about sound has no reason to.
    Opal::ScopePtr<AudioSystem> audio;
    try
    {
        audio = Opal::MakeScoped<AudioSystem>(nullptr, AudioSystemDesc{});
    }
    catch (const AudioDeviceException& exception)
    {
        RNDR_LOG_ERROR("Could not open the audio device: {}", exception.what());
        return 1;
    }
    audio->SetBusVolume(k_music_bus, 0.7f);

    const AudioClipHandle blip = audio->CreateClip(MakeBlip(880.0f, 0.25f));
    const AudioClipHandle chord = audio->CreateClip(MakeChord(110.0f, 2.0f));
    AudioClipHandle ogg;
    try
    {
        ogg = audio->LoadClip(Opal::Paths::Combine(RNDR_CORE_ASSETS_DIR, "audio", "test-tone.ogg").GetValue());
    }
    catch (const Opal::Exception& exception)
    {
        RNDR_LOG_WARNING("OGG fixture not loaded, O will do nothing: {}", exception.what());
    }

    SoundHandle chord_sound;
    bool is_paused = false;
    Opal::RNG rng;

    InputContext& input = app->GetInputSystemChecked().GetContextByName("Default");
    input.AddAction("Quit").Bind(Key::Escape, Trigger::Pressed).OnButton([&window](Trigger, bool) { window->RequestClose(); });
    input.AddAction("Blip")
        .Bind(Key::Space, Trigger::Pressed)
        .OnButton(
            [&](Trigger, bool is_repeated)
            {
                if (!is_repeated)
                {
                    const f32 pan = rng.RandomF32(-1.0f, 1.0f);
                    const f32 pitch = rng.RandomF32(0.8f, 1.25f);
                    audio->Play(blip, {.pan = pan, .pitch = pitch, .bus = k_sfx_bus});
                }
            });
    input.AddAction("Ogg")
        .Bind(Key::O, Trigger::Pressed)
        .OnButton(
            [&](Trigger, bool is_repeated)
            {
                if (!is_repeated && ogg.IsValid())
                {
                    audio->Play(ogg, {.volume = 0.5f, .bus = k_music_bus});
                }
            });
    input.AddAction("Toggle loop")
        .Bind(Key::M, Trigger::Pressed)
        .OnButton(
            [&](Trigger, bool is_repeated)
            {
                if (is_repeated)
                {
                    return;
                }
                if (audio->IsPlaying(chord_sound))
                {
                    audio->Stop(chord_sound);
                }
                else
                {
                    chord_sound = audio->Play(chord, {.loop = true, .bus = k_music_bus});
                }
            });
    input.AddAction("Volume up")
        .Bind(Key::UpArrow, Trigger::Pressed)
        .OnButton(
            [&](Trigger, bool)
            {
                const f32 volume = std::min(audio->GetMasterVolume() + 0.1f, 1.0f);
                audio->SetMasterVolume(volume);
                RNDR_LOG_INFO("Master volume {:.1f}", volume);
            });
    input.AddAction("Volume down")
        .Bind(Key::DownArrow, Trigger::Pressed)
        .OnButton(
            [&](Trigger, bool)
            {
                const f32 volume = std::max(audio->GetMasterVolume() - 0.1f, 0.0f);
                audio->SetMasterVolume(volume);
                RNDR_LOG_INFO("Master volume {:.1f}", volume);
            });
    input.AddAction("Pause")
        .Bind(Key::P, Trigger::Pressed)
        .OnButton(
            [&](Trigger, bool is_repeated)
            {
                if (is_repeated)
                {
                    return;
                }
                is_paused = !is_paused;
                if (is_paused)
                {
                    audio->PauseAll();
                }
                else
                {
                    audio->ResumeAll();
                }
                RNDR_LOG_INFO(is_paused ? "Paused" : "Resumed");
            });

    RNDR_LOG_INFO("Space: blip, O: ogg, M: loop on/off, Up/Down: master volume, P: pause/resume, Escape: quit");

    f32 delta_seconds = 0.016f;
    while (!window->IsClosed())
    {
        const f64 start_seconds = Opal::GetSeconds();
        // Nothing to draw, so wait for input rather than spin; the audio thread does not need this loop.
        app->ProcessSystemEvents(16);
        app->GetInputSystemChecked().ProcessSystemEvents(delta_seconds);
        delta_seconds = static_cast<f32>(Opal::GetSeconds() - start_seconds);
    }

    return 0;
}
