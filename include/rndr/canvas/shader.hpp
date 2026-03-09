#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/canvas/context.hpp"

namespace Rndr
{
namespace Canvas
{

enum class ShaderType : u8
{
    Vertex,
    Fragment,
    Compute,
    EnumCount,
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
 * A single compiled GPU shader stage. Slang source is compiled to SPIR-V and uploaded to OpenGL.
 */
class Shader
{
public:
    /**
     * Compile a shader from a source file on disk. The shader stage is deduced from the Slang
     * [shader("...")] annotation on the entry point.
     * @param path Path to the Slang source file.
     * @param entry_point Name of the entry point function in the source.
     * @return A valid Shader object.
     */
    [[nodiscard]] static Shader FromSource(const Opal::StringUtf8& path, const Opal::StringUtf8& entry_point);

    /**
     * Compile a shader from source code in memory. The shader stage is deduced from the Slang
     * [shader("...")] annotation on the entry point.
     * @param source Slang shader source code.
     * @param entry_point Name of the entry point function in the source.
     * @return A valid Shader object.
     */
    [[nodiscard]] static Shader FromSourceInMemory(const Opal::StringUtf8& source, const Opal::StringUtf8& entry_point);

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
    [[nodiscard]] ShaderType GetType() const;
    [[nodiscard]] const Opal::DynamicArray<ShaderParameter>& GetParameters() const;
    [[nodiscard]] const ShaderParameter* FindParameter(const Opal::StringUtf8& name) const;

private:
    u32 m_shader = 0;
    ShaderType m_type = ShaderType::EnumCount;
    Opal::StringUtf8 m_source;
    Opal::StringUtf8 m_entry_point;
    Opal::DynamicArray<ShaderParameter> m_parameters;
};

}  // namespace Canvas
}  // namespace Rndr
