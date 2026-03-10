#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/canvas/context.hpp"
#include "rndr/canvas/vertex-layout.hpp"

namespace Rndr
{
namespace Canvas
{

struct NumThreads
{
    u32 x = 0;
    u32 y = 0;
    u32 z = 0;
};

enum class ParameterCategory : u8
{
    Uniform,
    Texture,
    Sampler,
    StorageBuffer,
    VaryingInput,
    VaryingOutput,
    EnumCount,
};

struct ShaderParameter
{
    Opal::StringUtf8 name;
    i32 binding_index = -1;
    i32 binding_space = 0;
    i32 offset = 0;
    i32 size = 0;
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

    [[nodiscard]] Shader Clone() const;
    void Destroy();

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] u32 GetNativeHandle() const;
    [[nodiscard]] const Opal::DynamicArray<ShaderParameter>& GetParameters() const;
    [[nodiscard]] const ShaderParameter* FindParameter(const Opal::StringUtf8& name) const;

    /** @return Vertex layout inferred from shader reflection. Empty for compute shaders. */
    [[nodiscard]] const VertexLayout& GetVertexLayout() const;

    /** @return Compute shader thread group size. All zeros for non-compute shaders. */
    [[nodiscard]] const NumThreads& GetNumThreads() const;

private:
    u32 m_program = 0;
    Opal::StringUtf8 m_vertex_source;
    Opal::StringUtf8 m_vertex_entry;
    Opal::StringUtf8 m_fragment_source;  // Empty for single-source case.
    Opal::StringUtf8 m_fragment_entry;
    Opal::DynamicArray<ShaderParameter> m_parameters;
    VertexLayout m_vertex_layout;
    NumThreads m_num_threads;
};

}  // namespace Canvas
}  // namespace Rndr
