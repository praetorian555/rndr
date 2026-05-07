#pragma once

#include "volk/volk.h"

#include "opal/container/array-view.h"
#include "opal/container/ref.h"
#include "opal/container/string.h"

#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

struct AdvancedShaderDesc
{
    Opal::StringUtf8 entry_point = "main";
};

class AdvancedShader
{
public:
    /**
     * Create a shader by compiling a Slang source file. The desc.entry_point selects which
     * annotated entry point to compile.
     */
    [[nodiscard]] static AdvancedShader FromSource(const class AdvancedDevice& device, const Opal::StringUtf8& path,
                                                   const AdvancedShaderDesc& desc = {});

    /**
     * Create a shader by compiling Slang source code in memory. The desc.entry_point selects
     * which annotated entry point to compile.
     */
    [[nodiscard]] static AdvancedShader FromSourceInMemory(const class AdvancedDevice& device, const Opal::StringUtf8& source,
                                                           const AdvancedShaderDesc& desc = {});

    /**
     * Create a shader from a SPIR-V binary file. The desc.entry_point selects which entry point
     * inside the SPIR-V module is used to determine the shader stage.
     */
    [[nodiscard]] static AdvancedShader FromSpirvFile(const class AdvancedDevice& device, const Opal::StringUtf8& path,
                                                      const AdvancedShaderDesc& desc = {});

    /**
     * Create a shader from SPIR-V data in memory. The desc.entry_point selects which entry
     * point inside the SPIR-V module is used to determine the shader stage.
     */
    [[nodiscard]] static AdvancedShader FromSpirvInMemory(const class AdvancedDevice& device, Opal::ArrayView<const u8> spirv_data,
                                                          const AdvancedShaderDesc& desc = {});

    AdvancedShader() = default;
    explicit AdvancedShader(const class AdvancedDevice& device, Opal::ArrayView<const u8> spirv_data,
                            const AdvancedShaderDesc& desc = {});
    ~AdvancedShader();

    AdvancedShader(const AdvancedShader&) = delete;
    AdvancedShader& operator=(const AdvancedShader&) = delete;
    AdvancedShader(AdvancedShader&&) noexcept;
    AdvancedShader& operator=(AdvancedShader&&) noexcept;

    void Destroy();

    [[nodiscard]] VkShaderModule GetNativeShaderModule() const { return m_shader_module; }
    [[nodiscard]] VkShaderStageFlagBits GetNativeShaderStage() const { return m_native_stage; }
    [[nodiscard]] ShaderTypeBits GetShaderStage() const { return m_stage; }
    [[nodiscard]] const Opal::StringUtf8& GetEntryPoint() const { return m_entry_point; }

private:
    Opal::Ref<const class AdvancedDevice> m_device;
    VkShaderModule m_shader_module = VK_NULL_HANDLE;
    VkShaderStageFlagBits m_native_stage = {};
    ShaderTypeBits m_stage = {};
    Opal::StringUtf8 m_entry_point;
};

}  // namespace Rndr