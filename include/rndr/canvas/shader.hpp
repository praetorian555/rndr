#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/canvas/context.hpp"
#include "rndr/canvas/vertex-layout.hpp"

namespace Rndr
{
namespace Canvas
{

/** Compute shader thread group size, extracted from shader reflection. */
struct NumThreads
{
    u32 x = 0;
    u32 y = 0;
    u32 z = 0;
};

/**
 * Describes what kind of resource a shader parameter represents. Extracted from Slang reflection
 * and used by the Brush to decide how to bind the resource (e.g., as a UBO, texture, or SSBO).
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
 *
 * The Brush uses this data to automatically create GPU uniform buffers: it groups all parameters
 * with `category == Uniform && size > 0` by binding_index, computes the total buffer size as
 * max(offset + size), and creates one UBO per group.
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

    /** What kind of resource this parameter represents. */
    ParameterCategory category = ParameterCategory::EnumCount;
};

/**
 * A compiled GPU shader program. Slang source is compiled to SPIR-V and linked into an OpenGL
 * program.
 *
 * For graphics shaders, the program contains both vertex and fragment stages. Reflection data from
 * both stages is merged. For compute shaders, the program contains a single compute stage.
 *
 * Entry points are auto-discovered from [shader("vertex")], [shader("fragment")], and
 * [shader("compute")] annotations. Single-source factory methods auto-detect whether the source
 * is a graphics shader (exactly one vertex + one fragment entry point) or a compute shader
 * (exactly one compute entry point). Two-source factory methods are graphics-only.
 */
class Shader
{
public:
    /**
     * Create a shader program from a single source file containing both vertex and fragment entry
     * points.
     * @param path Path to the Slang source file.
     * @return A valid Shader object.
     */
    [[nodiscard]] static Shader FromSource(const Opal::StringUtf8& path);

    /**
     * Create a shader program from source code in memory containing both vertex and fragment entry
     * points.
     * @param source Slang shader source code.
     * @return A valid Shader object.
     */
    [[nodiscard]] static Shader FromSourceInMemory(const Opal::StringUtf8& source);

    /**
     * Create a shader program from two separate source files for vertex and fragment stages.
     * @param vertex_path Path to the vertex stage Slang source file.
     * @param fragment_path Path to the fragment stage Slang source file.
     * @return A valid Shader object.
     */
    [[nodiscard]] static Shader FromSources(const Opal::StringUtf8& vertex_path, const Opal::StringUtf8& fragment_path);

    /**
     * Create a shader program from two separate source strings for vertex and fragment stages.
     * @param vertex_source Slang source code for the vertex stage.
     * @param fragment_source Slang source code for the fragment stage.
     * @return A valid Shader object.
     */
    [[nodiscard]] static Shader FromSourcesInMemory(const Opal::StringUtf8& vertex_source, const Opal::StringUtf8& fragment_source);

    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    /** Create a deep copy by recompiling from the stored source. */
    [[nodiscard]] Shader Clone() const;

    /** Release the GL program and clear all reflection data. */
    void Destroy();

    /** @return True if this shader holds a valid GL program. */
    [[nodiscard]] bool IsValid() const;

    /** @return The underlying OpenGL program handle. */
    [[nodiscard]] u32 GetNativeHandle() const;

    /**
     * @return All parameters extracted from shader reflection (uniforms, textures, samplers,
     *         storage buffers, and varying inputs/outputs). See ShaderParameter for the layout
     *         conventions.
     */
    [[nodiscard]] const Opal::DynamicArray<ShaderParameter>& GetParameters() const;

    /**
     * Look up a parameter by name.
     * @param name Parameter name as declared in the shader source.
     * @return Pointer to the parameter, or nullptr if not found.
     */
    [[nodiscard]] const ShaderParameter* FindParameter(const Opal::StringUtf8& name) const;

    /** @return Vertex layout inferred from shader reflection. Empty for compute shaders. */
    [[nodiscard]] const VertexLayout& GetVertexLayout() const;

    /** @return Compute shader thread group size. All zeros for non-compute shaders. */
    [[nodiscard]] const NumThreads& GetNumThreads() const;

private:
    /** OpenGL program handle. 0 means invalid. */
    u32 m_program = 0;

    /** Original Slang source, retained for Clone(). For two-source shaders this is the vertex source. */
    Opal::StringUtf8 m_vertex_source;
    Opal::StringUtf8 m_vertex_entry;

    /** Fragment source, only populated for two-source shaders (FromSources / FromSourcesInMemory). */
    Opal::StringUtf8 m_fragment_source;
    Opal::StringUtf8 m_fragment_entry;

    /** Merged reflection data from all stages. See ShaderParameter for layout conventions. */
    Opal::DynamicArray<ShaderParameter> m_parameters;

    /** Vertex input layout inferred from reflection. Empty for compute shaders. */
    VertexLayout m_vertex_layout;

    /** Compute thread group size. All zeros for non-compute shaders. */
    NumThreads m_num_threads;
};

}  // namespace Canvas
}  // namespace Rndr
