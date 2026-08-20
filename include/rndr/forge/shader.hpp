#pragma once

#include "volk/volk.h"

#include "opal/container/array-view.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/ref.h"
#include "opal/container/string.h"

#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"

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

struct ShaderDesc
{
    Opal::StringUtf8 entry_point = "main";
};

class Shader
{
public:
    /**
     * Create a shader by compiling a Slang source file. The desc.entry_point selects which
     * annotated entry point to compile.
     */
    [[nodiscard]] static Shader FromSource(const Device& device, const Opal::StringUtf8& path,
                                                   const ShaderDesc& desc = {});

    /**
     * Create a shader by compiling Slang source code in memory. The desc.entry_point selects
     * which annotated entry point to compile.
     */
    [[nodiscard]] static Shader FromSourceInMemory(const Device& device, const Opal::StringUtf8& source,
                                                           const ShaderDesc& desc = {});

    /**
     * Create a shader from a SPIR-V binary file. The desc.entry_point selects which entry point
     * inside the SPIR-V module is used to determine the shader stage.
     */
    [[nodiscard]] static Shader FromSpirvFile(const Device& device, const Opal::StringUtf8& path,
                                                      const ShaderDesc& desc = {});

    /**
     * Create a shader from SPIR-V data in memory. The desc.entry_point selects which entry
     * point inside the SPIR-V module is used to determine the shader stage.
     */
    [[nodiscard]] static Shader FromSpirvInMemory(const Device& device, Opal::ArrayView<const u8> spirv_data,
                                                          const ShaderDesc& desc = {});

    Shader() = default;
    explicit Shader(const Device& device, Opal::ArrayView<const u8> spirv_data,
                            const ShaderDesc& desc = {});
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

private:
    Opal::Ref<const Device> m_device;
    VkShaderModule m_shader_module = VK_NULL_HANDLE;
    VkShaderStageFlagBits m_native_stage = {};
    ShaderTypeBits m_stage = {};
    Opal::StringUtf8 m_entry_point;
    Opal::DynamicArray<SpecializationConstantInfo> m_specialization_constants;
};

}  // namespace Rndr::Forge
