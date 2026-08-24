#pragma once

#include "volk/volk.h"

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/container/ref.h"
#include "opal/container/string.h"

#include "rndr/error-codes.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"

namespace Rndr
{
class ShaderCache;
}

namespace Rndr::Forge
{

/**
 * A specialization constant a shader declares, as reflection found it. What a caller needs to know before
 * supplying a value: what it is called, what may go in it, and what it holds when nothing does.
 */
struct SpecializationConstantInfo
{
    Opal::StringUtf8 name;
    /** The numeric id Vulkan keys on. Forge matches by name, so this is here to be read, not to be used. */
    u32 constant_id = 0;
    SpecializationType type = SpecializationType::Int32;
    /**
     * Bytes this constant takes in the blob handed to Vulkan, which the specification requires to be the
     * byte size of the declared type - one for an 8 bit constant, and four for a bool whatever its type
     * says. Not derivable from `type`, which reports anything narrower than 32 bits as its 32 bit
     * counterpart so a caller can write a plain integer for it.
     */
    u32 byte_size = 4;
    /** What the shader falls back to when the pipeline supplies nothing. */
    SpecializationValue default_value;
};

/**
 * A vertex attribute this shader reads, as reflection found it. Built-ins are not among these - they occupy
 * no location and are fed by the implementation rather than by a vertex buffer.
 *
 * Reflection has the location and the format but not the offset or the stride, which describe the vertex
 * struct on the CPU side and appear nowhere in the SPIR-V. VertexInputDesc::FromShader fills those in under
 * a stated assumption; everything here is fact.
 */
struct ShaderInputInfo
{
    Opal::StringUtf8 name;
    u32 location = 0;
    PixelFormat format = PixelFormat::Undefined;
};

/** A descriptor this shader reads, as reflection found it. */
struct ShaderBindingInfo
{
    Opal::StringUtf8 name;
    u32 set = 0;
    u32 binding = 0;
    DescriptorType descriptor_type = DescriptorType::CombinedImageSampler;
    /** Descriptors in this binding. More than one means the shader declared it as an array. */
    u32 descriptor_count = 1;
};

/** A push constant block this shader reads, as reflection found it. */
struct ShaderPushConstantInfo
{
    Opal::StringUtf8 name;
    u32 offset = 0;
    u32 size = 0;
};

struct ShaderDesc
{
    Opal::StringUtf8 entry_point = "main";

    /**
     * Where to look for this entry point's compiled code before asking Slang for it, and where to keep it
     * afterwards. Empty means compile every time, which is what happened before there was a cache.
     *
     * Worth having because Slang is slow and everything after it is not: compiling the two entry points of
     * the sample costs seconds, and building the pipeline from the result costs milliseconds. The cache has
     * to outlive the shaders that fill it to be of any use, so it belongs to the application rather than to
     * anything here.
     */
    Opal::Ref<ShaderCache> cache;
};

class Shader
{
public:
    /**
     * Create a shader by compiling a Slang source file. The desc.entry_point selects which
     * annotated entry point to compile.
     * @return The shader, ErrorCode::FileNotFound for a file that is not there or is empty,
     *         ErrorCode::ShaderCompilationError when Slang refused the source, or whatever
     *         FromSpirvInMemory reported about what came out of it.
     */
    [[nodiscard]] static Opal::Expected<Shader, ErrorCode> FromSource(const Device& device, const Opal::StringUtf8& path,
                                                                      const ShaderDesc& desc = {});

    /**
     * Create a shader by compiling Slang source code in memory. The desc.entry_point selects
     * which annotated entry point to compile.
     * @return The shader, ErrorCode::InvalidArgument for empty source, ErrorCode::ShaderCompilationError
     *         when Slang refused it, or whatever FromSpirvInMemory reported.
     */
    [[nodiscard]] static Opal::Expected<Shader, ErrorCode> FromSourceInMemory(const Device& device, const Opal::StringUtf8& source,
                                                                              const ShaderDesc& desc = {});

    /**
     * Create a shader from a SPIR-V binary file. The desc.entry_point selects which entry point
     * inside the SPIR-V module is used to determine the shader stage.
     * @return The shader, ErrorCode::FileNotFound for a file that is not there or is empty, or whatever
     *         FromSpirvInMemory reported.
     */
    [[nodiscard]] static Opal::Expected<Shader, ErrorCode> FromSpirvFile(const Device& device, const Opal::StringUtf8& path,
                                                                         const ShaderDesc& desc = {});

    /**
     * Create a shader from SPIR-V data in memory. The desc.entry_point selects which entry
     * point inside the SPIR-V module is used to determine the shader stage.
     * @return The shader, ErrorCode::CorruptData for bytes that are not a SPIR-V module,
     *         ErrorCode::InvalidArgument when the module names no such entry point,
     *         ErrorCode::UnsupportedFormat for a stage, a constant type or a descriptor kind Forge does not
     *         model, or whatever the failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<Shader, ErrorCode> FromSpirvInMemory(const Device& device, Opal::ArrayView<const u8> spirv_data,
                                                                             const ShaderDesc& desc = {});

    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) noexcept;
    Shader& operator=(Shader&&) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_shader_module != VK_NULL_HANDLE; }
    [[nodiscard]] VkShaderModule GetNativeShaderModule() const { return m_shader_module; }
    [[nodiscard]] VkShaderStageFlagBits GetNativeShaderStage() const { return m_native_stage; }
    [[nodiscard]] ShaderTypeBits GetShaderStage() const { return m_stage; }
    [[nodiscard]] const Opal::StringUtf8& GetEntryPoint() const { return m_entry_point; }

    /**
     * The specialization constants this module declares, read once when the shader was created. Empty for a
     * shader that declares none, which is most of them.
     *
     * A pipeline supplies values for these by name through GraphicsPipelineDesc::specialization; this is how
     * to find out what those names are without reading the shader source.
     */
    [[nodiscard]] Opal::ArrayView<const SpecializationConstantInfo> GetSpecializationConstants() const
    {
        return {m_specialization_constants.GetData(), m_specialization_constants.GetSize()};
    }

    /**
     * The vertex attributes this entry point reads, in the order reflection reported them. Empty for a stage
     * that takes no vertex input, which is every stage but the vertex one.
     *
     * VertexInputDesc::FromShader turns these into a desc, and a graphics pipeline checks whatever desc it
     * was given against them either way.
     */
    [[nodiscard]] Opal::ArrayView<const ShaderInputInfo> GetInputs() const { return {m_inputs.GetData(), m_inputs.GetSize()}; }

    /**
     * The descriptors this entry point reads, across every set. Scoped to the entry point, so a Slang file
     * holding several of them never has one stage claiming another's bindings.
     *
     * DescriptorSetLayoutDesc::shaders is what these are for: naming the bindings of a hand-written layout,
     * and checking that it says what the shader says.
     */
    [[nodiscard]] Opal::ArrayView<const ShaderBindingInfo> GetBindings() const { return {m_bindings.GetData(), m_bindings.GetSize()}; }

    /** The push constant blocks this entry point reads. PushConstantRangesFromShaders turns these into ranges. */
    [[nodiscard]] Opal::ArrayView<const ShaderPushConstantInfo> GetPushConstants() const
    {
        return {m_push_constants.GetData(), m_push_constants.GetSize()};
    }

private:
    Opal::Ref<const Device> m_device;
    VkShaderModule m_shader_module = VK_NULL_HANDLE;
    VkShaderStageFlagBits m_native_stage = {};
    ShaderTypeBits m_stage = {};
    Opal::StringUtf8 m_entry_point;
    Opal::DynamicArray<SpecializationConstantInfo> m_specialization_constants;
    Opal::DynamicArray<ShaderInputInfo> m_inputs;
    Opal::DynamicArray<ShaderBindingInfo> m_bindings;
    Opal::DynamicArray<ShaderPushConstantInfo> m_push_constants;
};

}  // namespace Rndr::Forge
