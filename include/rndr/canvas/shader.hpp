#pragma once

#include "rndr/canvas/vertex-layout.hpp"

namespace Rndr
{
namespace Canvas
{

/**
 * Shader compiled from Slang. Runtime compilation (debug) provides reflection data used to drive
 * vertex layout inference, uniform binding, and texture binding. Precompiled path (release) skips
 * reflection overhead.
 */
class Shader
{
public:
    /**
     * Compile a shader from Slang source. Provides full reflection and validation.
     * @param path Path to the .slang source file.
     * @return A valid Shader object.
     */
    [[nodiscard]] static Shader FromSource(const char* path);

    /**
     * Load a precompiled shader. Skips reflection overhead.
     * @param path Path to the precompiled shader binary.
     * @return A valid Shader object.
     */
    [[nodiscard]] static Shader FromCompiled(const char* path);

    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    [[nodiscard]] Shader Clone() const;
    void Destroy();

    /**
     * Infer vertex layout from shader reflection data.
     * @return VertexLayout describing the shader's vertex input requirements.
     */
    [[nodiscard]] VertexLayout GetVertexLayout() const;

    [[nodiscard]] bool IsValid() const;

private:
    u32 m_handle = 0;
};

}  // namespace Canvas
}  // namespace Rndr
