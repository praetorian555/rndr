#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/types.hpp"

namespace Rndr
{

/** Shader stage type. */
enum class ShaderStage : u8
{
    Vertex,
    Fragment,
    Compute,
    Unknown,
};

/** Compute shader thread group size, extracted from shader reflection. */
struct NumThreads
{
    u32 x = 0;
    u32 y = 0;
    u32 z = 0;
};

/**
 * Describes what kind of resource a shader parameter represents. Extracted from Slang reflection
 * and used to decide how to bind the resource (e.g., as a UBO, texture, or SSBO).
 */
enum class ParameterCategory : u8
{
    /** Uniform buffer data. Top-level declarations (ConstantBuffer/ParameterBlock) have size == 0.
     *  Individual fields within a UBO, or standalone global uniforms, have size > 0 and a valid
     *  offset within their parent UBO. */
    Uniform,

    /** Sampled texture (e.g., Texture2D, Sampler2D). */
    Texture,

    /** Sampler state (e.g., SamplerState). */
    Sampler,

    /** Read-write storage buffer (e.g., RWStructuredBuffer). */
    StorageBuffer,

    /** Vertex stage input (e.g., struct fields marked as vertex attributes). */
    VaryingInput,

    /** Inter-stage output (e.g., vertex-to-fragment varyings). */
    VaryingOutput,

    EnumCount,
};

/**
 * A single shader parameter extracted from Slang reflection. Parameters are organized as follows:
 *
 * **Explicit ConstantBuffer / ParameterBlock:**
 *   A top-level entry with `size == 0` acts as a UBO declaration (identified by binding_index).
 *   Its individual fields are separate ShaderParameter entries with `size > 0`, sharing the same
 *   binding_index. The offset is the byte offset of that field within the UBO.
 *
 *   Example: `ConstantBuffer<Material> material;` produces:
 *     - { name="material", binding_index=0, size=0 }  (top-level UBO declaration)
 *     - { name="color",    binding_index=0, size=16, offset=0  }  (field)
 *     - { name="roughness",binding_index=0, size=4,  offset=16 }  (field)
 *
 * **Standalone global uniforms:**
 *   Globals like `float4x4 mvp;` are implicitly wrapped into a default UBO by Slang. Each appears
 *   as a ShaderParameter with `size > 0` and a valid offset. Multiple standalone globals share the
 *   same binding_index (the implicit default UBO).
 *
 *   Example: `float4x4 mvp; float4 tint_color;` produces:
 *     - { name="mvp",        binding_index=0, size=64, offset=0  }
 *     - { name="tint_color", binding_index=0, size=16, offset=64 }
 */
struct ShaderParameter
{
    /** Parameter name as declared in the shader source. */
    Opal::StringUtf8 name;

    /** GPU binding slot index (e.g., UBO binding point, texture unit). */
    i32 binding_index = -1;

    /** Binding space / descriptor set (always 0 for OpenGL). */
    i32 binding_space = 0;

    /** Byte offset within the parent UBO. Only meaningful for Uniform params with size > 0. */
    i32 offset = 0;

    /** Size in bytes. Zero for top-level UBO declarations and non-uniform params. */
    i32 size = 0;

    /** Number of elements if this parameter is an array. Zero means it is not an array. */
    i32 array_element_count = 0;

    /** Byte stride between consecutive array elements. Only meaningful when array_element_count > 0. */
    i32 array_stride = 0;

    /** What kind of resource this parameter represents. */
    ParameterCategory category = ParameterCategory::EnumCount;
};

/** Scalar type for vertex input attributes. */
enum class ScalarType : u8
{
    Unknown,
    Float32,
    Int32,
};

/**
 * Describes a single vertex input attribute extracted from shader reflection.
 * For struct vertex inputs, each field is a separate VertexInputAttribute.
 */
struct VertexInputAttribute
{
    /** Attribute name as declared in the shader source (e.g., "position", "uv"). */
    Opal::StringUtf8 name;

    /** Number of vector components (1-4). */
    u8 component_count = 0;

    /** Scalar type of the attribute. */
    ScalarType scalar_type = ScalarType::Unknown;
};

/** Information about a discovered entry point. */
struct EntryPointInfo
{
    Opal::StringUtf8 name;
    ShaderStage stage = ShaderStage::Unknown;
};

/** Result of compiling a single entry point. */
struct CompileResult
{
    /** SPIR-V bytecode. */
    Opal::DynamicArray<u8> spirv;

    /** Detected shader stage. */
    ShaderStage stage = ShaderStage::Unknown;

    /** Reflection parameters extracted from the compiled program. */
    Opal::DynamicArray<ShaderParameter> parameters;

    /** Vertex input attributes. Only populated for vertex stage entry points. */
    Opal::DynamicArray<VertexInputAttribute> vertex_inputs;

    /** Compute thread group size. Only populated for compute stage entry points. */
    NumThreads num_threads;
};

/**
 * Compiles Slang shader source to SPIR-V and extracts reflection data. This is the shared
 * compilation layer used by both Canvas (OpenGL) and Forge (Vulkan) APIs.
 *
 * Usage:
 * @code
 *   ShaderCompiler compiler;
 *   compiler.LoadModule(slang_source);
 *   auto entries = compiler.DiscoverEntryPoints();
 *   auto vs_name = ShaderCompiler::FindSingleEntryPoint(entries, ShaderStage::Vertex, "vertex");
 *   auto result = compiler.CompileEntryPoint(vs_name);
 *   // result.spirv contains the SPIR-V bytecode
 *   // result.parameters contains reflection data
 * @endcode
 */
class ShaderCompiler
{
public:
    ShaderCompiler();
    ~ShaderCompiler();

    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;
    ShaderCompiler(ShaderCompiler&& other) noexcept;
    ShaderCompiler& operator=(ShaderCompiler&& other) noexcept;

    /** Load a Slang module from source code in memory. */
    void LoadModule(const Opal::StringUtf8& source);

    /** Discover all annotated entry points in the loaded module. */
    [[nodiscard]] Opal::DynamicArray<EntryPointInfo> DiscoverEntryPoints() const;

    /** Compile a specific entry point to SPIR-V and extract reflection data. */
    [[nodiscard]] CompileResult CompileEntryPoint(const Opal::StringUtf8& entry_point) const;

    /** Find exactly one entry point of the given stage. Throws if 0 or >1 found. */
    [[nodiscard]] static Opal::StringUtf8 FindSingleEntryPoint(const Opal::DynamicArray<EntryPointInfo>& entries,
                                                                ShaderStage target_stage, const char* stage_name);

    /**
     * Merge parameters from two shader stages, resolving duplicates and checking for conflicts.
     * Skips VaryingOutput from stage_a and VaryingInput from stage_b (inter-stage varyings).
     */
    [[nodiscard]] static Opal::DynamicArray<ShaderParameter> MergeParameters(
        const Opal::DynamicArray<ShaderParameter>& stage_a_params,
        const Opal::DynamicArray<ShaderParameter>& stage_b_params);

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

}  // namespace Rndr