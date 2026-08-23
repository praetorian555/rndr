#include "rndr/file.hpp"

#include <cstring>
#include <limits>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis/stb_vorbis.h"
#undef STB_VORBIS_HEADER_ONLY

#include "opal/file-system.h"
#include "opal/paths.h"

#include "rndr/log.hpp"

namespace
{

using namespace Rndr;

/**
 * Little-endian cursor over a byte view. A read that would run past the end yields zero and puts the reader in a
 * failed state rather than reading anything, so a caller can read a whole chunk's worth of fields and ask once,
 * at the end, whether any of it was really there.
 */
class ByteReader
{
public:
    explicit ByteReader(Opal::ArrayView<const u8> bytes) : m_bytes(bytes) {}

    /** False once a read or a seek has run off the end. Nothing read after that means anything. */
    [[nodiscard]] bool IsValid() const { return m_is_valid; }

    [[nodiscard]] u64 GetOffset() const { return m_offset; }
    [[nodiscard]] u64 GetRemaining() const { return m_offset >= m_bytes.GetSize() ? 0 : m_bytes.GetSize() - m_offset; }
    [[nodiscard]] bool IsAtEnd() const { return m_offset >= m_bytes.GetSize(); }

    void Seek(u64 offset)
    {
        if (offset > m_bytes.GetSize())
        {
            m_is_valid = false;
            return;
        }
        m_offset = offset;
    }

    void Skip(u64 count) { Seek(m_offset + count); }

    u16 ReadU16()
    {
        if (!Take(2))
        {
            return 0;
        }
        const u8* p = m_bytes.GetData() + m_offset - 2;
        return static_cast<u16>(p[0] | (p[1] << 8));
    }

    u32 ReadU32()
    {
        if (!Take(4))
        {
            return 0;
        }
        const u8* p = m_bytes.GetData() + m_offset - 4;
        return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) | (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24);
    }

    /** Reads a four-character chunk id; returns true when it equals @p id and the read was in bounds. */
    bool ReadTag(const char* id)
    {
        char read_id[4] = {};
        ReadId(read_id);
        return m_is_valid && std::memcmp(read_id, id, 4) == 0;
    }

    void ReadId(char out_id[4])
    {
        if (!Take(4))
        {
            return;
        }
        std::memcpy(out_id, m_bytes.GetData() + m_offset - 4, 4);
    }

    [[nodiscard]] const u8* GetCurrent() const { return m_bytes.GetData() + m_offset; }

private:
    /** Advances past @p count bytes when they are there, and fails the reader when they are not. */
    bool Take(u64 count)
    {
        if (!m_is_valid || GetRemaining() < count)
        {
            m_is_valid = false;
            return false;
        }
        m_offset += count;
        return true;
    }

    Opal::ArrayView<const u8> m_bytes;
    u64 m_offset = 0;
    bool m_is_valid = true;
};

constexpr u16 k_wave_format_pcm = 0x0001;
constexpr u16 k_wave_format_ieee_float = 0x0003;
constexpr u16 k_wave_format_extensible = 0xFFFE;

struct WavFormat
{
    u16 format_tag = 0;
    u16 channel_count = 0;
    u32 sample_rate = 0;
    u16 block_align = 0;
    u16 bits_per_sample = 0;
};

ErrorCode ReadFormatChunk(ByteReader& reader, u32 chunk_size, WavFormat& out_format)
{
    if (chunk_size < 16)
    {
        RNDR_LOG_ERROR("WAV: fmt chunk is too short");
        return ErrorCode::CorruptData;
    }
    const u64 chunk_start = reader.GetOffset();
    WavFormat format;
    format.format_tag = reader.ReadU16();
    format.channel_count = reader.ReadU16();
    format.sample_rate = reader.ReadU32();
    reader.ReadU32();  // average bytes per second, derived and not trusted
    format.block_align = reader.ReadU16();
    format.bits_per_sample = reader.ReadU16();

    if (format.format_tag == k_wave_format_extensible)
    {
        // WAVEFORMATEXTENSIBLE: cbSize, wValidBitsPerSample, dwChannelMask, then a GUID whose first two bytes are
        // the real format tag.
        if (chunk_size < 40)
        {
            RNDR_LOG_ERROR("WAV: extensible fmt chunk is too short");
            return ErrorCode::CorruptData;
        }
        reader.ReadU16();  // cbSize
        reader.ReadU16();  // valid bits per sample; the container size in bits_per_sample is what the data uses
        reader.ReadU32();  // channel mask
        format.format_tag = reader.ReadU16();
    }
    reader.Seek(chunk_start + chunk_size);
    if (!reader.IsValid())
    {
        RNDR_LOG_ERROR("WAV: the file ends inside the fmt chunk");
        return ErrorCode::CorruptData;
    }
    out_format = format;
    return ErrorCode::Success;
}

/**
 * Converts one block of interleaved integer or float samples to f32 in [-1, 1]. The caller has checked that
 * @p data holds @p sample_count samples of the given width.
 */
void ConvertSamples(const WavFormat& format, const u8* data, u64 sample_count, f32* out)
{
    const u32 bytes_per_sample = format.bits_per_sample / 8;
    if (format.format_tag == k_wave_format_ieee_float)
    {
        for (u64 i = 0; i < sample_count; i++)
        {
            f32 value = 0.0f;
            std::memcpy(&value, data + i * 4, 4);
            out[i] = value;
        }
        return;
    }
    switch (bytes_per_sample)
    {
        case 1:
            // 8-bit PCM is unsigned with 128 as silence.
            for (u64 i = 0; i < sample_count; i++)
            {
                out[i] = (static_cast<f32>(data[i]) - 128.0f) / 128.0f;
            }
            break;
        case 2:
            for (u64 i = 0; i < sample_count; i++)
            {
                const u8* p = data + i * 2;
                const auto value = static_cast<i16>(p[0] | (p[1] << 8));
                out[i] = static_cast<f32>(value) / 32768.0f;
            }
            break;
        case 3:
            for (u64 i = 0; i < sample_count; i++)
            {
                const u8* p = data + i * 3;
                // Sign-extend by building the value in the top three bytes of an i32.
                const auto value =
                    static_cast<i32>((static_cast<u32>(p[0]) << 8) | (static_cast<u32>(p[1]) << 16) | (static_cast<u32>(p[2]) << 24));
                out[i] = static_cast<f32>(value >> 8) / 8388608.0f;
            }
            break;
        case 4:
            for (u64 i = 0; i < sample_count; i++)
            {
                const u8* p = data + i * 4;
                const auto value = static_cast<i32>(static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) |
                                                    (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24));
                out[i] = static_cast<f32>(static_cast<f64>(value) / 2147483648.0);
            }
            break;
        default:
            // Unreachable: the caller rejects every width but these before it gets here.
            RNDR_HALT("WAV: unsupported sample width reached the converter");
            break;
    }
}

Opal::Expected<AudioClip, ErrorCode> DecodeWav(Opal::ArrayView<const u8> file_bytes)
{
    using Result = Opal::Expected<AudioClip, ErrorCode>;

    ByteReader reader(file_bytes);
    if (!reader.ReadTag("RIFF"))
    {
        RNDR_LOG_ERROR("WAV: not a RIFF file");
        return Result(ErrorCode::CorruptData);
    }
    reader.ReadU32();  // RIFF size; often wrong in files written by streaming encoders, so the chunk walk decides
    if (!reader.ReadTag("WAVE"))
    {
        RNDR_LOG_ERROR("WAV: RIFF file is not a WAVE file");
        return Result(ErrorCode::CorruptData);
    }

    WavFormat format;
    bool has_format = false;
    const u8* data = nullptr;
    u64 data_size = 0;
    bool has_data = false;

    // Chunks come in any order and there can be others in between (LIST, fact, cue, ...), which are skipped.
    while (!reader.IsAtEnd() && !(has_format && has_data))
    {
        if (reader.GetRemaining() < 8)
        {
            break;  // trailing padding
        }
        char chunk_id[4];
        reader.ReadId(chunk_id);
        const u32 chunk_size = reader.ReadU32();

        if (std::memcmp(chunk_id, "fmt ", 4) == 0)
        {
            const ErrorCode error = ReadFormatChunk(reader, chunk_size, format);
            if (error != ErrorCode::Success)
            {
                return Result(error);
            }
            has_format = true;
        }
        else if (std::memcmp(chunk_id, "data", 4) == 0)
        {
            if (chunk_size > reader.GetRemaining())
            {
                RNDR_LOG_ERROR("WAV: data chunk runs past the end of the file");
                return Result(ErrorCode::CorruptData);
            }
            data = reader.GetCurrent();
            data_size = chunk_size;
            has_data = true;
            reader.Skip(chunk_size);
        }
        else
        {
            reader.Skip(chunk_size);
        }
        // Chunks are word-aligned; an odd-sized one is followed by a pad byte that is not counted in its size.
        if ((chunk_size & 1) != 0 && !reader.IsAtEnd())
        {
            reader.Skip(1);
        }
    }

    if (!reader.IsValid())
    {
        RNDR_LOG_ERROR("WAV: the file ends in the middle of a chunk");
        return Result(ErrorCode::CorruptData);
    }
    if (!has_format)
    {
        RNDR_LOG_ERROR("WAV: no fmt chunk");
        return Result(ErrorCode::CorruptData);
    }
    if (!has_data)
    {
        RNDR_LOG_ERROR("WAV: no data chunk");
        return Result(ErrorCode::CorruptData);
    }
    if (format.format_tag != k_wave_format_pcm && format.format_tag != k_wave_format_ieee_float)
    {
        RNDR_LOG_ERROR("WAV: unsupported format tag {}", format.format_tag);
        return Result(ErrorCode::UnsupportedFormat);
    }
    if (format.channel_count != 1 && format.channel_count != 2)
    {
        RNDR_LOG_ERROR("WAV: only mono and stereo are supported, file has {} channels", format.channel_count);
        return Result(ErrorCode::UnsupportedFormat);
    }
    const bool is_valid_width =
        format.format_tag == k_wave_format_ieee_float
            ? format.bits_per_sample == 32
            : (format.bits_per_sample == 8 || format.bits_per_sample == 16 || format.bits_per_sample == 24 || format.bits_per_sample == 32);
    if (!is_valid_width)
    {
        RNDR_LOG_ERROR("WAV: unsupported bit depth {}", format.bits_per_sample);
        return Result(ErrorCode::UnsupportedFormat);
    }
    const u32 bytes_per_frame = format.channel_count * (format.bits_per_sample / 8);
    if (format.block_align != bytes_per_frame)
    {
        RNDR_LOG_ERROR("WAV: block align does not match the channel count and bit depth");
        return Result(ErrorCode::CorruptData);
    }
    const u64 frame_count = data_size / bytes_per_frame;
    if (frame_count == 0)
    {
        RNDR_LOG_ERROR("WAV: data chunk holds no frames");
        return Result(ErrorCode::CorruptData);
    }

    Opal::DynamicArray<f32> samples(frame_count * format.channel_count);
    ConvertSamples(format, data, samples.GetSize(), samples.GetData());
    return AudioClip::Create(format.sample_rate, format.channel_count, {samples.GetData(), samples.GetSize()});
}

Opal::Expected<AudioClip, ErrorCode> DecodeOggVorbis(Opal::ArrayView<const u8> file_bytes)
{
    using Result = Opal::Expected<AudioClip, ErrorCode>;

    if (file_bytes.GetSize() > static_cast<u64>(std::numeric_limits<int>::max()))
    {
        RNDR_LOG_ERROR("OGG: file is larger than the decoder can address");
        return Result(ErrorCode::UnsupportedFormat);
    }
    int error = VORBIS__no_error;
    stb_vorbis* decoder = stb_vorbis_open_memory(file_bytes.GetData(), static_cast<int>(file_bytes.GetSize()), &error, nullptr);
    if (decoder == nullptr)
    {
        RNDR_LOG_ERROR("OGG: not a Vorbis stream (stb_vorbis error {})", error);
        return Result(ErrorCode::CorruptData);
    }

    const stb_vorbis_info info = stb_vorbis_get_info(decoder);
    if (info.channels != 1 && info.channels != 2)
    {
        stb_vorbis_close(decoder);
        RNDR_LOG_ERROR("OGG: only mono and stereo are supported, file has {} channels", info.channels);
        return Result(ErrorCode::UnsupportedFormat);
    }

    const u32 channel_count = static_cast<u32>(info.channels);
    const u32 expected_frames = stb_vorbis_stream_length_in_samples(decoder);
    Opal::DynamicArray<f32> samples;
    samples.Reserve(static_cast<u64>(expected_frames) * channel_count);

    constexpr int k_frames_per_read = 4096;
    f32 chunk[k_frames_per_read * 2];
    while (true)
    {
        const int frames_read = stb_vorbis_get_samples_float_interleaved(decoder, static_cast<int>(channel_count), chunk,
                                                                         k_frames_per_read * static_cast<int>(channel_count));
        if (frames_read <= 0)
        {
            break;
        }
        const u64 sample_count = static_cast<u64>(frames_read) * channel_count;
        for (u64 i = 0; i < sample_count; i++)
        {
            samples.PushBack(chunk[i]);
        }
    }
    const int decode_error = stb_vorbis_get_error(decoder);
    stb_vorbis_close(decoder);

    if (decode_error != VORBIS__no_error)
    {
        RNDR_LOG_ERROR("OGG: decoding failed (stb_vorbis error {})", decode_error);
        return Result(ErrorCode::CorruptData);
    }
    if (samples.GetSize() == 0)
    {
        RNDR_LOG_ERROR("OGG: stream holds no frames");
        return Result(ErrorCode::CorruptData);
    }
    return AudioClip::Create(static_cast<u32>(info.sample_rate), channel_count, {samples.GetData(), samples.GetSize()});
}

}  // namespace

Opal::Expected<Rndr::AudioClip, Rndr::ErrorCode> Rndr::File::DecodeAudioClip(Opal::ArrayView<const u8> file_bytes, AudioFileFormat format)
{
    switch (format)
    {
        case AudioFileFormat::Wav:
            return DecodeWav(file_bytes);
        case AudioFileFormat::OggVorbis:
            return DecodeOggVorbis(file_bytes);
    }
    RNDR_LOG_ERROR("DecodeAudioClip: unknown audio file format");
    return Opal::Expected<AudioClip, ErrorCode>(ErrorCode::InvalidArgument);
}

Opal::Expected<Rndr::AudioClip, Rndr::ErrorCode> Rndr::File::LoadAudioClip(const Opal::StringUtf8& file_path)
{
    using Result = Opal::Expected<AudioClip, ErrorCode>;

    if (!Opal::Exists(file_path))
    {
        RNDR_LOG_ERROR("LoadAudioClip: file does not exist: {}", file_path.GetData());
        return Result(ErrorCode::FileNotFound);
    }

    const Opal::StringUtf8 extension = Opal::Paths::GetExtension(file_path).GetValue();
    AudioFileFormat format;
    if (extension == ".wav" || extension == ".WAV")
    {
        format = AudioFileFormat::Wav;
    }
    else if (extension == ".ogg" || extension == ".OGG")
    {
        format = AudioFileFormat::OggVorbis;
    }
    else
    {
        RNDR_LOG_ERROR("LoadAudioClip: unsupported extension: {}", extension.GetData());
        return Result(ErrorCode::UnsupportedFormat);
    }

    const Opal::DynamicArray<u8> file_bytes = ReadEntireFile(file_path);
    if (file_bytes.GetSize() == 0)
    {
        RNDR_LOG_ERROR("LoadAudioClip: failed to read: {}", file_path.GetData());
        return Result(ErrorCode::CorruptData);
    }
    return DecodeAudioClip({file_bytes.GetData(), file_bytes.GetSize()}, format);
}
