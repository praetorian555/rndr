#include "builder/rules.hpp"

// KTX-Software's read-only library, ktx_read, as its CMakeLists.txt builds it for rndr: static,
// KTX 1 and 2, Vulkan format queries, no OpenGL upload, no writing. The paths are relative to the
// release archive's root.

// The ASTC library libktx decodes ASTC payloads with. One SIMD level per architecture; x64 takes
// SSE4.1, which every x64 CPU rndr targets has.
BUILDER_MODULE(Astcenc, m, t)
{
    m.public_includes = {"external/astc-encoder/Source"};
    m.sources.Add("external/astc-encoder/Source/astcenc_averages_and_directions.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_block_sizes.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_color_quantize.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_color_unquantize.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_compress_symbolic.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_compute_variance.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_decompress_symbolic.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_diagnostic_trace.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_entry.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_find_best_partitioning.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_ideal_endpoints_and_weights.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_image.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_integer_sequence.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_mathlib.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_mathlib_softfloat.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_partition_tables.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_percentile_tables.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_pick_best_endpoint_format.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_quantization.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_symbolic_physical.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_weight_align.cpp");
    m.sources.Add("external/astc-encoder/Source/astcenc_weight_quant_xfer_tables.cpp");
    m.warnings = Warnings::Off;
    m.exceptions = true;

    const bool msvc = t.toolchain.Is(Compiler::Msvc);
    if (t.architecture == Architecture::Arm64)
    {
        m.private_defines = {"ASTCENC_NEON=1", "ASTCENC_SVE=0", "ASTCENC_SSE=0", "ASTCENC_AVX=0", "ASTCENC_POPCNT=0", "ASTCENC_F16C=0"};
    }
    else
    {
        m.private_defines = {"ASTCENC_NEON=0", "ASTCENC_SVE=0", "ASTCENC_SSE=41", "ASTCENC_AVX=0", "ASTCENC_POPCNT=1", "ASTCENC_F16C=0"};
        if (!msvc)
        {
            m.private_compile_flags = {"-msse4.1", "-mpopcnt"};
        }
    }
    // Invariant output across ISAs, astc-encoder's default.
    if (msvc)
    {
        m.private_defines.push_back("_CRT_SECURE_NO_WARNINGS");
        m.private_compile_flags.push_back("/fp:precise");
    }
    else if (t.toolchain.Is(Compiler::Clang))
    {
        m.private_compile_flags.push_back("-ffp-model=precise");
    }
    else
    {
        m.private_compile_flags.push_back("-ffp-contract=off");
    }
    if (!t.platform.Is(Platform::Windows))
    {
        m.private_compile_flags.push_back("-pthread");
        m.link_flags = {"-pthread"};
    }
}

BUILDER_MODULE(Ktx, m, t)
{
    // System, as rndr's CMake has it: khr_df.h uses C-style casts, which rndr's warnings refuse.
    m.public_system_includes = {"include"};
    m.private_includes = {"external/basisu/transcoder", "external", "external/basisu/zstd", "utils", "external/dfdutils"};
    m.private_system_includes = {"other_include"};
    m.private_deps = {"Astcenc"};
    m.public_defines = {"KHRONOS_STATIC", "KTX_FEATURE_KTX1", "KTX_FEATURE_KTX2"};
    // Reads files itself, so needs no Basis KTX 2 support; FXT1 is left out to save space.
    m.private_defines = {"LIBKTX", "SUPPORT_SOFTWARE_ETC_UNPACK=1", "BASISD_SUPPORT_FXT1=0", "BASISD_SUPPORT_KTX2_ZSTD=0",
                         "BASISD_SUPPORT_KTX2=0"};
    m.c_standard = 11;
    m.warnings = Warnings::Off;
    m.exceptions = true;

    for (const char* source :
         {"lib/astc_codec.cpp",           "lib/basis_transcode.cpp",       "lib/miniz_wrapper.cpp",
          "external/basisu/transcoder/basisu_transcoder.cpp",               "external/basisu/zstd/zstd.c",
          "lib/checkheader.c",            "external/dfdutils/createdfd.c", "external/dfdutils/colourspaces.c",
          "external/dfdutils/interpretdfd.c", "external/dfdutils/printdfd.c", "external/dfdutils/queries.c",
          "external/dfdutils/vk2dfd.c",   "lib/etcunpack.cxx",             "lib/filestream.c",
          "lib/glformat_str.c",           "lib/hashlist.c",                "lib/info.c",
          "lib/memstream.c",              "lib/strings.c",                 "lib/swap.c",
          "lib/texture.c",                "lib/texture2.c",                "lib/vkformat_check.c",
          "lib/vkformat_check_variant.c", "lib/vkformat_str.c",            "lib/vkformat_typesize.c",
          "external/etcdec/etcdec.cxx",   "lib/texture1.c",                "lib/vk_funcs.c",
          "lib/vkloader.c"})
    {
        m.sources.Add(source);
    }
    m.headers.Add("include/ktx.h");
    m.headers.Add("include/ktxvulkan.h");
    m.headers.Add("include/KHR/khr_df.h");

    if (t.toolchain.Is(Compiler::Msvc))
    {
        m.private_defines.push_back("_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR");
    }
    else
    {
        // The basisu transcoder relies on it, which upstream sets for that file alone.
        m.private_compile_flags = {"-fno-strict-aliasing"};
    }
    if (t.platform.Is(Platform::Linux))
    {
        m.libs = {"dl", "pthread"};
    }
}
