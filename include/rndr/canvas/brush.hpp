#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/canvas/buffer.hpp"
#include "rndr/types.hpp"

#include <cstring>

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

/** Which faces to cull. */
enum class CullMode : u8
{
    /** No culling, both faces visible. */
    None,

    /** Cull back faces (default for closed geometry). */
    Back,

    /** Cull front faces. */
    Front,

    EnumCount
};

/** How polygons are rasterized. */
enum class FillMode : u8
{
    /** Filled polygons. */
    Solid,

    /** Edge outlines only. */
    Wireframe,

    EnumCount
};

/** Comparison function for depth and stencil tests. */
enum class CompareFunc : u8
{
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    Always,
    Never,
    EnumCount
};

/**
 * Describes pipeline state with sensible defaults. All fields except shader, textures, and
 * uniforms live here. Users can construct a BrushDesc, tweak the fields they care about, and
 * pass it to the Brush constructor.
 */
struct BrushDesc
{
    BlendMode blend_mode = BlendMode::None;
    bool depth_test = false;
    bool depth_write = true;
    CompareFunc depth_compare = CompareFunc::Less;
    CullMode cull_mode = CullMode::Back;
    FillMode fill_mode = FillMode::Solid;
    f32 depth_bias_factor = 0.0f;
    f32 depth_bias_units = 0.0f;
};

/** A named uniform value stored as raw bytes. */
struct UniformBinding
{
    Opal::StringUtf8 name;
    Opal::DynamicArray<u8> data;
};

/** A named texture binding. */
struct TextureBinding
{
    Opal::StringUtf8 name;
    const Texture* texture = nullptr;
};

/** A named buffer binding (for storage buffers). */
struct BufferBinding
{
    Opal::StringUtf8 name;
    const Buffer* buffer = nullptr;
};

/**
 * A GPU uniform buffer owned by the Brush, paired with a CPU-side staging area.
 *
 * Created automatically by Brush::SetShader() based on shader reflection. One slot is created for
 * each unique UBO binding point that has uniform fields (size > 0). When SetUniform() is called,
 * the value is written into the CPU staging data at the correct offset and the slot is marked
 * dirty. Call UploadUniforms() to push all dirty slots to the GPU.
 */
struct UniformBufferSlot
{
    /** The GPU buffer that will be bound at this UBO slot during rendering. */
    Buffer gpu_buffer;

    /** CPU-side copy of the UBO data. SetUniform() writes here; UploadUniforms() pushes to GPU. */
    Opal::DynamicArray<u8> cpu_data;

    /** The UBO binding point index, matching ShaderParameter::binding_index. */
    i32 binding_index = -1;

    /** The binding space, matching ShaderParameter::binding_space (always 0 for OpenGL). */
    i32 binding_space = 0;

    /** True if cpu_data has been modified since the last UploadUniforms() call. */
    bool dirty = false;
};

/**
 * Collects all rendering state: shader, blend mode, depth test, etc. Named after the Canvas
 * metaphor — "how you paint", not "what you paint on".
 *
 * ## Uniform buffer management
 *
 * When a Shader is assigned via SetShader(), the Brush inspects the shader's reflection data and
 * automatically creates GPU uniform buffers (UniformBufferSlot) for each UBO binding point. This
 * covers both explicit `ConstantBuffer<T>` declarations and standalone global uniforms (which
 * Slang wraps into an implicit default UBO).
 *
 * The typical workflow is:
 *   1. `brush.SetShader(shader);`    — creates UBO slots from reflection.
 *   2. `brush.SetUniform("mvp", m);` — writes into the correct UBO's CPU staging data.
 *   3. `brush.UploadUniforms();`     — pushes all dirty UBOs to the GPU.
 *
 * If SetUniform() is called with a name that does not match any shader parameter (or if no shader
 * is set), the value is stored in a fallback list (GetUniforms()) for later manual handling.
 *
 * Textures and storage buffers are bound by name and stored as non-owning pointers — the user is
 * responsible for keeping those resources alive.
 */
class Brush
{
public:
    Brush() = default;
    explicit Brush(const BrushDesc& desc);
    ~Brush();

    Brush(const Brush&) = delete;
    Brush& operator=(const Brush&) = delete;
    Brush(Brush&& other) noexcept;
    Brush& operator=(Brush&& other) noexcept;

    [[nodiscard]] Brush Clone() const;

    /**
     * Set the shader program used for rendering. This also inspects the shader's reflection data
     * and creates GPU uniform buffers (UniformBufferSlot) for each UBO binding point that has
     * uniform fields. Any previously created UBO slots are destroyed and replaced.
     *
     * Requires an active OpenGL context (since it creates GPU buffers).
     */
    void SetShader(const Shader& shader);

    /** @return Currently bound shader, or nullptr if none. */
    [[nodiscard]] const Shader* GetShader() const;

    /** Set the blend mode. */
    void SetBlendMode(BlendMode mode);

    /** Enable or disable depth testing. */
    void SetDepthTest(bool enabled);

    /** Enable or disable depth writing. Depth test must be enabled for this to have effect. */
    void SetDepthWrite(bool enabled);

    /** Set the depth comparison function. */
    void SetDepthCompare(CompareFunc func);

    /** Set the face culling mode. */
    void SetCullMode(CullMode mode);

    /** Set the polygon fill mode. */
    void SetFillMode(FillMode mode);

    /**
     * Set depth bias to avoid z-fighting / shadow acne.
     * @param factor Scale factor applied to the slope of the polygon.
     * @param units Constant depth offset in units of the minimum resolvable depth difference.
     */
    void SetDepthBias(f32 factor, f32 units);

    /** @return Current pipeline state descriptor. */
    [[nodiscard]] const BrushDesc& GetDesc() const;

    /**
     * Bind a texture by name. The name must match a texture parameter declared in the shader.
     * @param name Binding name as declared in the shader.
     * @param texture Texture to bind.
     */
    void SetTexture(const char* name, const Texture& texture);

    /**
     * Bind a buffer by name. The name must match a storage buffer parameter declared in the shader.
     * @param name Binding name as declared in the shader.
     * @param buffer Buffer to bind.
     */
    void SetBuffer(const char* name, const Buffer& buffer);

    /**
     * Set a uniform value by name. If a shader is set and the name matches a reflected uniform
     * parameter, the value is written directly into the appropriate UniformBufferSlot's CPU staging
     * data at the correct byte offset (determined by shader reflection). The slot is marked dirty
     * and will be uploaded on the next UploadUniforms() call.
     *
     * If the name does not match any reflected parameter, or if no shader is set, the value is
     * stored in the fallback uniform bindings list (accessible via GetUniforms()).
     *
     * @param name Uniform name as declared in the shader (e.g. "mvp", "tint_color").
     * @param value Value to set. Must be a trivially copyable type matching the shader declaration.
     */
    template<typename T>
    void SetUniform(const char* name, const T& value);

    /** @return Fallback uniform bindings for values that did not match any shader parameter. */
    [[nodiscard]] const Opal::DynamicArray<UniformBinding>& GetUniforms() const;

    /** @return All texture bindings. */
    [[nodiscard]] const Opal::DynamicArray<TextureBinding>& GetTextures() const;

    /** @return All buffer bindings. */
    [[nodiscard]] const Opal::DynamicArray<BufferBinding>& GetBuffers() const;

    /** Upload all dirty uniform buffers to the GPU. */
    void UploadUniforms();

    /**
     * Apply all rendering state to the current OpenGL context. This is the method that translates
     * the Brush's high-level state into actual GL calls. Intended to be called by the DrawList
     * before issuing draw commands.
     *
     * Performs the following, in order:
     *   1. Binds the shader program (glUseProgram).
     *   2. Configures depth state (test, write, compare func).
     *   3. Configures blend state from BlendMode.
     *   4. Configures rasterizer state (cull mode, fill mode, depth bias).
     *   5. Uploads dirty uniform buffers to the GPU (UploadUniforms).
     *   6. Binds UBOs to their respective binding points (glBindBufferBase).
     *   7. Binds textures to their respective texture units (glBindTextureUnit).
     *   8. Binds storage buffers to their respective binding points (glBindBufferBase).
     *
     * Requires a valid shader (IsValid() must be true) and an active OpenGL context.
     */
    void Apply();

    /** @return All uniform buffer slots created from shader reflection. */
    [[nodiscard]] const Opal::DynamicArray<UniformBufferSlot>& GetUniformBufferSlots() const;

    /** @return True if a shader has been assigned. */
    [[nodiscard]] bool IsValid() const;

private:
    /** Non-template core of SetUniform. Routes data to the matching UBO slot or fallback list. */
    void SetUniformRaw(const char* name, const void* data, u64 size);

    /**
     * Scan the current shader's parameters and create one UniformBufferSlot for each unique UBO
     * binding point that has uniform fields (size > 0). Called automatically by SetShader().
     */
    void CreateUniformBufferSlots();
    const Shader* m_shader = nullptr;
    BrushDesc m_desc;
    Opal::DynamicArray<UniformBinding> m_uniforms;
    Opal::DynamicArray<TextureBinding> m_textures;
    Opal::DynamicArray<BufferBinding> m_buffers;
    Opal::DynamicArray<UniformBufferSlot> m_uniform_buffer_slots;
};

template<typename T>
void Brush::SetUniform(const char* name, const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>, "Uniform value must be trivially copyable!");
    SetUniformRaw(name, &value, sizeof(T));
}

}  // namespace Canvas
}  // namespace Rndr
