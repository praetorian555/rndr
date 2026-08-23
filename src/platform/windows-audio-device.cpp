#include "rndr/platform/windows-audio-device.hpp"

// Not the usual windows-header.hpp: it undefines `far`, which the COM headers still spell out as FAR.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <objbase.h>

#include "rndr/exception.hpp"
#include "rndr/log.hpp"

namespace
{

/** KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, spelled out so this file does not need the ks headers or a GUID library. */
constexpr GUID k_subtype_ieee_float = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

/** SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT. */
constexpr DWORD k_stereo_channel_mask = 0x3;

/** How long the thread waits on the endpoint before it decides the stream is dead rather than merely idle. */
constexpr DWORD k_buffer_wait_timeout_ms = 2000;

/** How long to wait between attempts to reopen a lost endpoint. */
constexpr DWORD k_retry_interval_ms = 500;

template <typename T>
void Release(T*& object)
{
    if (object != nullptr)
    {
        object->Release();
        object = nullptr;
    }
}

}  // namespace

Rndr::WindowsAudioDevice::WindowsAudioDevice(const AudioDeviceDesc& desc, AudioRenderCallback&& callback)
    : m_desc(desc), m_callback(std::move(callback))
{
    if (desc.sample_rate == 0 || desc.buffer_frames == 0)
    {
        throw AudioDeviceException(static_cast<u32>(E_INVALIDARG), "sample rate and buffer size must be greater than 0");
    }

    m_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_buffer_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (m_stop_event == nullptr || m_buffer_event == nullptr)
    {
        const DWORD error = GetLastError();
        if (m_stop_event != nullptr)
        {
            CloseHandle(m_stop_event);
        }
        if (m_buffer_event != nullptr)
        {
            CloseHandle(m_buffer_event);
        }
        throw AudioDeviceException(static_cast<u32>(HRESULT_FROM_WIN32(error)), "could not create the thread events");
    }

    const u32 state_before_start = m_started.GetState();
    Opal::Expected<Opal::ThreadHandle, Opal::ErrorCode> thread = Opal::CreateThread(&WindowsAudioDevice::ThreadMain, this);
    if (!thread.HasValue())
    {
        CloseHandle(m_stop_event);
        CloseHandle(m_buffer_event);
        throw AudioDeviceException(static_cast<u32>(E_FAIL), "could not start the audio thread");
    }
    m_thread = thread.GetValue();

    // The thread reports the outcome of its first attempt at the stream; there is no point returning a device that
    // will never play anything.
    m_started.Wait(state_before_start);
    const i32 result = m_start_result.Load<Opal::MemoryOrder::Acquire>();
    if (FAILED(result))
    {
        Opal::JoinThread(m_thread);
        CloseHandle(m_stop_event);
        CloseHandle(m_buffer_event);
        const char* what = result == static_cast<i32>(E_NOTFOUND) ? "no audio output device" : "could not open the audio output stream";
        throw AudioDeviceException(static_cast<u32>(result), what);
    }
}

Rndr::WindowsAudioDevice::~WindowsAudioDevice()
{
    SetEvent(m_stop_event);
    Opal::JoinThread(m_thread);
    CloseHandle(m_stop_event);
    CloseHandle(m_buffer_event);
}

void Rndr::WindowsAudioDevice::ThreadMain(WindowsAudioDevice* device)
{
    device->Run();
}

void Rndr::WindowsAudioDevice::Run()
{
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com_result))
    {
        m_start_result.Store<Opal::MemoryOrder::Release>(com_result);
        m_started.NotifyAll();
        return;
    }

    // Asks the scheduler to treat this like the audio thread it is. Best effort: the stream works without it.
    DWORD task_index = 0;
    HANDLE mmcss_task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
    if (mmcss_task == nullptr)
    {
        RNDR_LOG_WARNING("WindowsAudioDevice: could not raise the audio thread priority");
    }

    const i32 first_result = InitializeStream();
    m_start_result.Store<Opal::MemoryOrder::Release>(first_result);
    m_started.NotifyAll();

    if (SUCCEEDED(first_result))
    {
        while (true)
        {
            const bool keep_going = Pump();
            ReleaseStream();
            if (!keep_going)
            {
                break;
            }
            // The endpoint went away. Keep trying the new default until one opens or someone pulls the plug on us.
            bool reopened = false;
            while (WaitBeforeRetry())
            {
                if (SUCCEEDED(InitializeStream()))
                {
                    RNDR_LOG_INFO("WindowsAudioDevice: audio output restored");
                    reopened = true;
                    break;
                }
                ReleaseStream();
            }
            if (!reopened)
            {
                break;
            }
        }
    }
    ReleaseStream();
    Release(m_enumerator);

    if (mmcss_task != nullptr)
    {
        AvRevertMmThreadCharacteristics(mmcss_task);
    }
    CoUninitialize();
}

Rndr::i32 Rndr::WindowsAudioDevice::InitializeStream()
{
    HRESULT result = S_OK;
    if (m_enumerator == nullptr)
    {
        result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(&m_enumerator));
        if (FAILED(result))
        {
            return result;
        }
    }

    IMMDevice* endpoint = nullptr;
    result = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &endpoint);
    if (FAILED(result))
    {
        return result;
    }
    result = endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&m_client));
    Release(endpoint);
    if (FAILED(result))
    {
        return result;
    }

    WAVEFORMATEXTENSIBLE format = {};
    format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels = 2;
    format.Format.nSamplesPerSec = m_desc.sample_rate;
    format.Format.wBitsPerSample = 32;
    format.Format.nBlockAlign = format.Format.nChannels * format.Format.wBitsPerSample / 8;
    format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec * format.Format.nBlockAlign;
    format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    format.Samples.wValidBitsPerSample = 32;
    format.dwChannelMask = k_stereo_channel_mask;
    format.SubFormat = k_subtype_ieee_float;

    // Shared mode, so the endpoint keeps its own format and Windows converts ours to it: AUTOCONVERTPCM allows a
    // format the mix engine does not run at, and SRC_DEFAULT_QUALITY picks the resampler when the rates differ.
    // The buffer duration is in 100 ns units; the engine rounds it up to its own period.
    const REFERENCE_TIME buffer_duration =
        static_cast<REFERENCE_TIME>(m_desc.buffer_frames) * 10'000'000 / static_cast<REFERENCE_TIME>(m_desc.sample_rate);
    const DWORD stream_flags =
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    result = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, stream_flags, buffer_duration, 0, &format.Format, nullptr);
    if (FAILED(result))
    {
        return result;
    }
    result = m_client->SetEventHandle(m_buffer_event);
    if (FAILED(result))
    {
        return result;
    }
    result = m_client->GetBufferSize(&m_buffer_frame_count);
    if (FAILED(result))
    {
        return result;
    }
    result = m_client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&m_render_client));
    if (FAILED(result))
    {
        return result;
    }

    // Start from a full buffer of silence so the first callback has a whole period to run in.
    BYTE* data = nullptr;
    result = m_render_client->GetBuffer(m_buffer_frame_count, &data);
    if (FAILED(result))
    {
        return result;
    }
    result = m_render_client->ReleaseBuffer(m_buffer_frame_count, AUDCLNT_BUFFERFLAGS_SILENT);
    if (FAILED(result))
    {
        return result;
    }
    return m_client->Start();
}

void Rndr::WindowsAudioDevice::ReleaseStream()
{
    if (m_client != nullptr)
    {
        m_client->Stop();
    }
    Release(m_render_client);
    Release(m_client);
    m_buffer_frame_count = 0;
}

bool Rndr::WindowsAudioDevice::Pump()
{
    HANDLE events[] = {m_stop_event, m_buffer_event};
    while (true)
    {
        const DWORD wait = WaitForMultipleObjects(2, events, FALSE, k_buffer_wait_timeout_ms);
        if (wait == WAIT_OBJECT_0)
        {
            return false;
        }
        if (wait == WAIT_TIMEOUT)
        {
            RNDR_LOG_WARNING("WindowsAudioDevice: the endpoint stopped asking for data, reopening the stream");
            return true;
        }

        UINT32 padding = 0;
        HRESULT result = m_client->GetCurrentPadding(&padding);
        if (FAILED(result))
        {
            RNDR_LOG_WARNING("WindowsAudioDevice: lost the audio output (0x{:08x}), reopening the stream", static_cast<u32>(result));
            return true;
        }
        const UINT32 frame_count = m_buffer_frame_count - padding;
        if (frame_count == 0)
        {
            continue;
        }

        BYTE* data = nullptr;
        result = m_render_client->GetBuffer(frame_count, &data);
        if (FAILED(result))
        {
            RNDR_LOG_WARNING("WindowsAudioDevice: lost the audio output (0x{:08x}), reopening the stream", static_cast<u32>(result));
            return true;
        }
        // Rendered straight into the endpoint's buffer: the callback overwrites every sample it is given.
        m_callback(Opal::ArrayView<f32>(reinterpret_cast<f32*>(data), static_cast<u64>(frame_count) * 2));
        result = m_render_client->ReleaseBuffer(frame_count, 0);
        if (FAILED(result))
        {
            RNDR_LOG_WARNING("WindowsAudioDevice: lost the audio output (0x{:08x}), reopening the stream", static_cast<u32>(result));
            return true;
        }
    }
}

bool Rndr::WindowsAudioDevice::WaitBeforeRetry()
{
    return WaitForSingleObject(m_stop_event, k_retry_interval_ms) == WAIT_TIMEOUT;
}
