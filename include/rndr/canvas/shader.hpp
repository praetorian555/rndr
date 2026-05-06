#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/canvas/context.hpp"
#include "rndr/canvas/vertex-layout.hpp"
#include "rndr/core/shader-compiler.hpp"

namespace Rndr
{
namespace Canvas
{

// Backward compatibility — these types moved to Rndr namespace in core/shader-compiler.hpp.
using NumThreads = Rndr::NumThreads;
using ParameterCategory = Rndr::ParameterCategory;
using ShaderParameter = Rndr::ShaderParameter;

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
     * @param debug_name Name used for debug.
     * @return A valid Shader object. Empty by default.
     */
    [[nodiscard]] static Shader FromSource(const Opal::StringUtf8& path, Opal::StringUtf8 debug_name = "");

    /**
     * Create a shader program from source code in memory containing both vertex and fragment entry
     * points.
     * @param source Slang shader source code.
     * @param debug_name Name used for debug. Empty by default.
     * @return A valid Shader object.
     */
    [[nodiscard]] static Shader FromSourceInMemory(const Opal::StringUtf8& source, Opal::StringUtf8 debug_name = "");

    /**
     * Create a shader program from two separate source files for vertex and fragment stages.
     * @param vertex_path Path to the vertex stage Slang source file.
     * @param fragment_path Path to the fragment stage Slang source file.
     * @param debug_name Name used for debug. Empty by default.
     * @return A valid Shader object.
     */
    [[nodiscard]] static Shader FromSources(const Opal::StringUtf8& vertex_path, const Opal::StringUtf8& fragment_path, Opal::StringUtf8 debug_name = "");

    /**
     * Create a shader program from two separate source strings for vertex and fragment stages.
     * @param vertex_source Slang source code for the vertex stage.
     * @param fragment_source Slang source code for the fragment stage.
     * @param debug_name Name used for debug. Empty by default.
     * @return A valid Shader object.
     */
    [[nodiscard]] static Shader FromSourcesInMemory(const Opal::StringUtf8& vertex_source, const Opal::StringUtf8& fragment_source, Opal::StringUtf8 debug_name = "");

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

    /** Debug name. */
    Opal::StringUtf8 m_debug_name;

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