#include "rndr/core/shader-cache.hpp"

#include <cstring>

#include "opal/file-system.h"
#include "opal/hash.h"
#include "opal/paths.h"

#include "slang.h"

#include "rndr/log.hpp"

namespace
{
/**
 * The blob on disk. Every field of the key is written out and compared on the way back in, so a file whose
 * name collides with another key, or one left behind by a different Slang, reads as a miss rather than as
 * the wrong shader.
 */
constexpr Rndr::u32 k_magic = 0x43535221;  // "!RSC"
constexpr Rndr::u32 k_version = 1;

/** Appends the bytes of a POD value, which is how every length in the blob is written. */
template <typename T>
void Append(Opal::DynamicArray<Rndr::u8>& out, const T& value)
{
    const auto offset = out.GetSize();
    out.Resize(offset + static_cast<Rndr::i64>(sizeof(T)));
    memcpy(out.GetData() + offset, &value, sizeof(T));
}

void AppendString(Opal::DynamicArray<Rndr::u8>& out, const Opal::StringUtf8& value)
{
    const auto size = static_cast<Rndr::u64>(value.GetSize());
    Append(out, size);
    if (size == 0)
    {
        return;
    }
    const auto offset = out.GetSize();
    out.Resize(offset + static_cast<Rndr::i64>(size));
    memcpy(out.GetData() + offset, value.GetData(), size);
}

/** Reads a POD value, or reports that the blob ended first. */
template <typename T>
bool Read(Opal::ArrayView<const Rndr::u8> blob, Rndr::u64& cursor, T& out_value)
{
    if (cursor + sizeof(T) > static_cast<Rndr::u64>(blob.GetSize()))
    {
        return false;
    }
    memcpy(&out_value, blob.GetData() + cursor, sizeof(T));
    cursor += sizeof(T);
    return true;
}

bool ReadString(Opal::ArrayView<const Rndr::u8> blob, Rndr::u64& cursor, Opal::StringUtf8& out_value)
{
    Rndr::u64 size = 0;
    if (!Read(blob, cursor, size))
    {
        return false;
    }
    if (cursor + size > static_cast<Rndr::u64>(blob.GetSize()))
    {
        return false;
    }
    out_value = Opal::StringUtf8(reinterpret_cast<const char*>(blob.GetData() + cursor), static_cast<Rndr::i64>(size));
    cursor += size;
    return true;
}
}  // namespace

Rndr::ShaderCacheKey Rndr::ShaderCacheKey::Make(const Opal::StringUtf8& source, const Opal::StringUtf8& entry_point,
                                                ShaderOutputFormat format)
{
    ShaderCacheKey key;
    key.source = source.Clone();
    key.entry_point = entry_point.Clone();
    key.format = format;
    const char* build_tag = spGetBuildTagString();
    key.build_tag = build_tag != nullptr ? Opal::StringUtf8(build_tag) : Opal::StringUtf8();
    return key;
}

Rndr::u64 Rndr::ShaderCacheKey::GetHash() const
{
    u64 hash = Opal::Hash::CalcRawArray(reinterpret_cast<const u8*>(source.GetData()), static_cast<u64>(source.GetSize()));
    hash = Opal::Hash::CalcRawArray(reinterpret_cast<const u8*>(entry_point.GetData()), static_cast<u64>(entry_point.GetSize()), hash);
    hash = Opal::Hash::CalcRawArray(reinterpret_cast<const u8*>(build_tag.GetData()), static_cast<u64>(build_tag.GetSize()), hash);
    const auto format_value = static_cast<u8>(format);
    return Opal::Hash::CalcRawArray(&format_value, sizeof(format_value), hash);
}

bool Rndr::ShaderCacheKey::operator==(const ShaderCacheKey& other) const
{
    return format == other.format && entry_point == other.entry_point && build_tag == other.build_tag && source == other.source;
}

Rndr::ShaderCache::ShaderCache(const Opal::StringUtf8& directory) : m_directory(directory.Clone())
{
    if (m_directory.IsEmpty())
    {
        return;
    }
    if (Opal::Exists(m_directory))
    {
        return;
    }
    if (Opal::CreateDirectory(m_directory) != Opal::ErrorCode::Success)
    {
        // A cache that cannot be written is not a reason to fail a shader compile. Drop to memory only and
        // say so once, rather than reporting the same failure on every Store.
        RNDR_LOG_WARNING("Could not create the shader cache directory {}, caching in memory only.", m_directory.GetData());
        m_directory = Opal::StringUtf8();
    }
}

Opal::StringUtf8 Rndr::ShaderCache::GetFilePath(const ShaderCacheKey& key) const
{
    if (m_directory.IsEmpty())
    {
        return {};
    }
    char name[32] = {};
    snprintf(name, sizeof(name), "%016llx.rsc", static_cast<unsigned long long>(key.GetHash()));
    Opal::Expected<Opal::StringUtf8, Opal::ErrorCode> path = Opal::Paths::Combine(m_directory, Opal::StringUtf8(name));
    return path.HasValue() ? path.GetValue().Clone() : Opal::StringUtf8();
}

Opal::DynamicArray<Rndr::u8> Rndr::ShaderCache::Find(const ShaderCacheKey& key) const
{
    const u64 hash = key.GetHash();
    for (i32 i = 0; i < m_entries.GetSize(); ++i)
    {
        if (m_entries[i].hash == hash && m_entries[i].key == key)
        {
            ++m_hits;
            return m_entries[i].code.Clone();
        }
    }

    const Opal::StringUtf8 path = GetFilePath(key);
    if (path.IsEmpty() || !Opal::Exists(path))
    {
        ++m_misses;
        return {};
    }
    Opal::Expected<Opal::DynamicArray<u8>, Opal::ErrorCode> blob = Opal::ReadFileAsBytes(path);
    if (!blob.HasValue())
    {
        ++m_misses;
        return {};
    }

    // Everything below is a way for the file to be wrong, and every one of them means the same thing: no
    // entry, compile it. Nothing here reports an error, because nothing here is one.
    const Opal::ArrayView<const u8> bytes(blob.GetValue().GetData(), blob.GetValue().GetSize());
    u64 cursor = 0;
    u32 magic = 0;
    u32 version = 0;
    if (!Read(bytes, cursor, magic) || magic != k_magic || !Read(bytes, cursor, version) || version != k_version)
    {
        ++m_misses;
        return {};
    }
    ShaderCacheKey stored;
    u8 format_value = 0;
    if (!ReadString(bytes, cursor, stored.source) || !ReadString(bytes, cursor, stored.entry_point) ||
        !ReadString(bytes, cursor, stored.build_tag) || !Read(bytes, cursor, format_value))
    {
        ++m_misses;
        return {};
    }
    stored.format = static_cast<ShaderOutputFormat>(format_value);
    u64 code_size = 0;
    if (!Read(bytes, cursor, code_size) || cursor + code_size != static_cast<u64>(bytes.GetSize()))
    {
        ++m_misses;
        return {};
    }
    // The whole point of writing the key out: this is what makes a hit a fact rather than a hash collision.
    if (!(stored == key))
    {
        ++m_misses;
        return {};
    }

    Opal::DynamicArray<u8> code(static_cast<i32>(code_size));
    if (code_size > 0)
    {
        memcpy(code.GetData(), bytes.GetData() + cursor, code_size);
    }
    // Promote it, so a second lookup in this process does not read the file again.
    m_entries.PushBack(Entry{.hash = hash, .key = key.Clone(), .code = code.Clone()});
    ++m_hits;
    return code;
}

void Rndr::ShaderCache::Store(const ShaderCacheKey& key, Opal::ArrayView<const u8> code)
{
    const u64 hash = key.GetHash();
    Opal::DynamicArray<u8> owned(static_cast<i32>(code.GetSize()));
    if (code.GetSize() > 0)
    {
        memcpy(owned.GetData(), code.GetData(), static_cast<u64>(code.GetSize()));
    }
    for (i32 i = 0; i < m_entries.GetSize(); ++i)
    {
        if (m_entries[i].hash == hash && m_entries[i].key == key)
        {
            m_entries[i].code = owned.Clone();
            return;
        }
    }
    m_entries.PushBack(Entry{.hash = hash, .key = key.Clone(), .code = owned.Clone()});

    const Opal::StringUtf8 path = GetFilePath(key);
    if (path.IsEmpty())
    {
        return;
    }
    Opal::DynamicArray<u8> blob;
    Append(blob, k_magic);
    Append(blob, k_version);
    AppendString(blob, key.source);
    AppendString(blob, key.entry_point);
    AppendString(blob, key.build_tag);
    Append(blob, static_cast<u8>(key.format));
    Append(blob, static_cast<u64>(owned.GetSize()));
    const auto offset = blob.GetSize();
    blob.Resize(offset + owned.GetSize());
    if (owned.GetSize() > 0)
    {
        memcpy(blob.GetData() + offset, owned.GetData(), static_cast<u64>(owned.GetSize()));
    }
    if (Opal::WriteBytesToFile(path, {blob.GetData(), blob.GetSize()}) != Opal::ErrorCode::Success)
    {
        // The memory tier already has it, so this run is unaffected. Worth a word, since a cache that never
        // writes looks exactly like one that is working until somebody times a second run.
        RNDR_LOG_WARNING("Could not write the shader cache entry {}.", path.GetData());
    }
}
