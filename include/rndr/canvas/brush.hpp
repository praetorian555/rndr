#pragma once

#include "rndr/types.hpp"

namespace Rndr
{
namespace Canvas
{

class Shader;
class Texture;

/** Simplified blend modes for the Canvas API. */
enum class BlendMode : u8
{
    /** No blending, fragment overwrites destination. */
    None,

    /** Standard alpha blending: src * src_a + dst * (1 - src_a). */
    Alpha,

    /** Additive blending: src + dst. */
    Additive,

    /** Multiply blending: src * dst. */
    Multiply,

    EnumCount
};

/**
 * Collects all rendering state: shader, blend mode, depth test, etc. Named after the Canvas
 * metaphor, "how you paint", not "what you paint on". Uniforms and textures are bound by name,
 * validated against shader reflection. No manual slot indices.
 */
class Brush
{
public:
    Brush() = default;
    ~Brush();

    Brush(const Brush&) = delete;
    Brush& operator=(const Brush&) = delete;
    Brush(Brush&& other) noexcept;
    Brush& operator=(Brush&& other) noexcept;

    [[nodiscard]] Brush Clone() const;

    /** Set the shader program used for rendering. */
    void SetShader(const Shader& shader);

    /** Set the blend mode. */
    void SetBlendMode(BlendMode mode);

    /** Enable or disable depth testing. */
    void SetDepthTest(bool enabled);

    /**
     * Bind a texture by name. Validated against shader reflection in debug builds.
     * @param name Binding name as declared in the shader.
     * @param texture Texture to bind.
     */
    void SetTexture(const char* name, const Texture& texture);

    /**
     * Bind a uniform value by name. Validated against shader reflection in debug builds.
     * Type mismatches are caught at bind time in debug builds.
     * @param name Uniform name as declared in the shader.
     * @param value Value to set.
     */
    template<typename T>
    void SetUniform(const char* name, const T& value);

    [[nodiscard]] bool IsValid() const;

private:
    const Shader* m_shader = nullptr;
    BlendMode m_blend_mode = BlendMode::None;
    bool m_depth_test = false;
};

}  // namespace Canvas
}  // namespace Rndr
