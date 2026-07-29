#pragma once

#include "volk/volk.h"

#include "opal/container/array-view.h"
#include "opal/container/ref.h"
#include "opal/container/string.h"

#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"

namespace Rndr::Forge
{

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

    [[nodiscard]] VkShaderModule GetNativeShaderModule() const { return m_shader_module; }
    [[nodiscard]] VkShaderStageFlagBits GetNativeShaderStage() const { return m_native_stage; }
    [[nodiscard]] ShaderTypeBits GetShaderStage() const { return m_stage; }
    [[nodiscard]] const Opal::StringUtf8& GetEntryPoint() const { return m_entry_point; }

private:
    Opal::Ref<const Device> m_device;
    VkShaderModule m_shader_module = VK_NULL_HANDLE;
    VkShaderStageFlagBits m_native_stage = {};
    ShaderTypeBits m_stage = {};
    Opal::StringUtf8 m_entry_point;
};

}  // namespace Rndr::Forge