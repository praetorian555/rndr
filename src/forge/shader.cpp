#include "rndr/forge/shader.hpp"

#include "opal/exceptions.h"

#include "spirv_reflect.h"

#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-exception.hpp"
#include "rndr/core/shader-compiler.hpp"
#include "rndr/file.hpp"

static VkShaderStageFlagBits ToNativeShaderStage(SpvReflectShaderStageFlagBits stage)
{
    switch (stage)
    {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT:
            return VK_SHADER_STAGE_TASK_BIT_EXT;
        case SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT:
            return VK_SHADER_STAGE_MESH_BIT_EXT;
        default:
            throw Opal::Exception("Unsupported shader stage");
    }
}

static Rndr::ShaderTypeBits ToShaderTypeBits(SpvReflectShaderStageFlagBits stage)
{
    switch (stage)
    {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
            return Rndr::ShaderTypeBits::Vertex;
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
            return Rndr::ShaderTypeBits::Fragment;
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
            return Rndr::ShaderTypeBits::Compute;
        case SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT:
            return Rndr::ShaderTypeBits::Task;
        case SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT:
            return Rndr::ShaderTypeBits::Mesh;
        default:
            throw Opal::Exception("Unsupported shader stage");
    }
}

Rndr::Forge::Shader::Shader(const Device& device, Opal::ArrayView<const u8> spirv_data,
                                     const ShaderDesc& desc)
    : m_device(device), m_entry_point(desc.entry_point.Clone())
{
    // Use spirv-reflect to detect the shader stage from the specified entry point.
    SpvReflectShaderModule reflect_module = {};
    const SpvReflectResult reflect_result =
        spvReflectCreateShaderModule(spirv_data.GetSize(), spirv_data.GetData(), &reflect_module);
    if (reflect_result != SPV_REFLECT_RESULT_SUCCESS)
    {
        throw Opal::Exception("Failed to create SPIR-V reflect module");
    }
    const SpvReflectEntryPoint* entry_point =
        spvReflectGetEntryPoint(&reflect_module, m_entry_point.GetData());
    if (entry_point == nullptr)
    {
        spvReflectDestroyShaderModule(&reflect_module);
        throw Opal::Exception("Entry point not found in SPIR-V module");
    }
    m_native_stage = ToNativeShaderStage(entry_point->shader_stage);
    m_stage = ToShaderTypeBits(entry_point->shader_stage);
    spvReflectDestroyShaderModule(&reflect_module);

    const VkShaderModuleCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv_data.GetSize(),
        .pCode = reinterpret_cast<const u32*>(spirv_data.GetData()),
    };
    const VkResult result = vkCreateShaderModule(m_device->GetNativeDevice(), &create_info, nullptr, &m_shader_module);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateShaderModule");
    }
}

Rndr::Forge::Shader Rndr::Forge::Shader::FromSpirvInMemory(const Device& device, Opal::ArrayView<const u8> spirv_data,
                                                             const ShaderDesc& desc)
{
    return Shader(device, spirv_data, desc);
}

Rndr::Forge::Shader Rndr::Forge::Shader::FromSpirvFile(const Device& device, const Opal::StringUtf8& path,
                                                         const ShaderDesc& desc)
{
    Opal::DynamicArray<u8> spirv_data = File::ReadEntireFile(path);
    if (spirv_data.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Failed to read SPIR-V file or file is empty!");
    }
    return Shader(device, Opal::ArrayView<const u8>(spirv_data.GetData(), spirv_data.GetSize()), desc);
}

Rndr::Forge::Shader Rndr::Forge::Shader::FromSourceInMemory(const Device& device, const Opal::StringUtf8& source,
                                                              const ShaderDesc& desc)
{
    if (source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Shader source is empty!");
    }

    ShaderCompiler compiler;
    compiler.LoadModule(source, ShaderOutputFormat::SpirV);
    const CompileResult result = compiler.CompileEntryPoint(desc.entry_point);
    return Shader(device, Opal::ArrayView<const u8>(result.code.GetData(), result.code.GetSize()), desc);
}

Rndr::Forge::Shader Rndr::Forge::Shader::FromSource(const Device& device, const Opal::StringUtf8& path,
                                                     const ShaderDesc& desc)
{
    const Opal::StringUtf8 source = File::ReadEntireTextFile(path);
    if (source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Failed to read shader file or file is empty!");
    }
    return FromSourceInMemory(device, source, desc);
}

Rndr::Forge::Shader::~Shader()
{
    Destroy();
}

Rndr::Forge::Shader::Shader(Shader&& other) noexcept
    : m_device(std::move(other.m_device)),
      m_shader_module(other.m_shader_module),
      m_native_stage(other.m_native_stage),
      m_stage(other.m_stage),
      m_entry_point(std::move(other.m_entry_point))
{
    other.m_shader_module = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::Forge::Shader& Rndr::Forge::Shader::operator=(Shader&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_shader_module = other.m_shader_module;
        m_native_stage = other.m_native_stage;
        m_stage = other.m_stage;
        m_entry_point = std::move(other.m_entry_point);
        other.m_shader_module = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::Forge::Shader::Destroy()
{
    if (m_shader_module != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device->GetNativeDevice(), m_shader_module, nullptr);
        m_shader_module = VK_NULL_HANDLE;
    }
    m_device = nullptr;
}