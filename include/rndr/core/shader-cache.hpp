#pragma once

#include "opal/clonable-base.h"
#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/types.hpp"
#include "rndr/core/shader-compiler.hpp"

namespace Rndr
{

/**
 * Everything that decides what a Slang compile produces. Two compiles agreeing on all of it produce the
 * same bytes, and a cache entry is only ever handed back to a lookup that agrees on all of it.
 *
 * The source is held whole rather than hashed down to the key, and compared byte for byte on a lookup. A
 * hash names the file on disk and narrows the search; it never decides a hit. Getting that wrong is silent
 * - the caller is handed a shader that is not the source they are reading - so the collision is spent on a
 * recompile instead.
 */
struct ShaderCacheKey : Opal::ClonableBase<ShaderCacheKey>
{
    Opal::StringUtf8 source;
    Opal::StringUtf8 entry_point;
    /** One source compiles to either, and the two share nothing. */
    ShaderOutputFormat format = ShaderOutputFormat::SpirV;
    /**
     * Which Slang produced it, from spGetBuildTagString. Upgrading the compiler invalidates every entry
     * rather than silently mixing output from two versions. A free function, so reading it costs nothing -
     * a cache hit never creates a Slang session.
     */
    Opal::StringUtf8 build_tag;

    /** Fills in `build_tag` from Slang, since no caller has a reason to pass a different one. */
    [[nodiscard]] static ShaderCacheKey Make(const Opal::StringUtf8& source, const Opal::StringUtf8& entry_point,
                                             ShaderOutputFormat format);

    /** Names the file on disk and narrows the in-memory search. Never decides a hit on its own. */
    [[nodiscard]] u64 GetHash() const;

    [[nodiscard]] bool operator==(const ShaderCacheKey& other) const;

    OPAL_CLONE_FIELDS(source, entry_point, format, build_tag);
};

/**
 * Compiled shader code, kept so Slang does not have to produce it twice. Compiling the two entry points of
 * the sample costs seconds; reading them back costs a file read.
 *
 * Two tiers. The in-memory one is always on and lasts as long as the cache object, which is what makes
 * repeated compiles of one source inside a single process free - a Catch2 test case re-runs its body once
 * per section, so a case with nine sections compiles its shaders nine times. The on-disk one is there when
 * the cache was given a directory, and is what survives the process.
 *
 * Nothing here is global and nothing is implicit: an application creates one and hands it over through
 * `Forge::ShaderDesc::cache`. A shader compiled without one behaves exactly as it did before this existed.
 *
 * Only the compiled code is stored, not the reflection `CompileResult` carries beside it - Forge reads its
 * own reflection out of the SPIR-V, and nothing else asks the cache for anything.
 */
class ShaderCache
{
public:
    /** In-memory only. Nothing is written anywhere and nothing survives the object. */
    ShaderCache() = default;

    /**
     * In-memory, plus a directory to read and write blobs in. The directory is created if it is missing;
     * if it cannot be, the cache quietly stays memory-only rather than making shader compilation fail over
     * a cache.
     */
    explicit ShaderCache(const Opal::StringUtf8& directory);

    /**
     * The code compiled for this key, or an empty array when there is none.
     *
     * Never throws and never reports an error. A miss, a blob that does not parse, one truncated by a
     * half-finished write, and one left behind by a different Slang all take the same path out of here, and
     * the caller compiles - which is the only sane answer for a cache and the reason it can be trusted.
     */
    [[nodiscard]] Opal::DynamicArray<u8> Find(const ShaderCacheKey& key) const;

    /** Keep this code for the key, in memory and, when there is a directory, on disk. */
    void Store(const ShaderCacheKey& key, Opal::ArrayView<const u8> code);

    /** Where blobs are written, empty for a memory-only cache. */
    [[nodiscard]] const Opal::StringUtf8& GetDirectory() const { return m_directory; }

    /** The file a key is stored in. Empty for a memory-only cache. Exposed so a test can corrupt one. */
    [[nodiscard]] Opal::StringUtf8 GetFilePath(const ShaderCacheKey& key) const;

    /** How many lookups this cache answered without compiling, and how many it could not. For tests. */
    [[nodiscard]] u32 GetHitCount() const { return m_hits; }
    [[nodiscard]] u32 GetMissCount() const { return m_misses; }

private:
    struct Entry
    {
        u64 hash = 0;
        ShaderCacheKey key;
        Opal::DynamicArray<u8> code;
    };

    Opal::StringUtf8 m_directory;
    /** Small enough that a linear scan past the hash beats building a map. */
    mutable Opal::DynamicArray<Entry> m_entries;
    mutable u32 m_hits = 0;
    mutable u32 m_misses = 0;
};

}  // namespace Rndr
