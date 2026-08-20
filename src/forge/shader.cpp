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

namespace
{
/** Turns a reflection failure into the same sentence whichever of the enumerators produced it. */
void RequireReflected(SpvReflectResult result, const char* what)
{
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        throw Opal::Exception(Opal::StringEx("Failed to read the ") + what + " of a SPIR-V module!");
    }
}

/**
 * Closes the reflect module whichever way the constructor leaves it. Several of the steps that read from it
 * throw, and it is not something Opal owns and unwinds on its own.
 */
struct ReflectModuleGuard
{
    SpvReflectShaderModule* module;
    ~ReflectModuleGuard() { spvReflectDestroyShaderModule(module); }
};
}  // namespace

/**
 * The type of a specialization constant, from the flags and scalar traits reflection reports. A width or a
 * kind with no counterpart here throws rather than being rounded to the nearest one: a value written into a
 * constant of the wrong width is not something the caller could notice afterwards.
 *
 * @param out_byte_size Bytes the constant takes in the blob handed to Vulkan, which the specification wants
 *                      to be the byte size of the declared type. Reported apart from the type because a
 *                      narrow integer is reported as its 32 bit counterpart while still owing Vulkan its own
 *                      width, and because a bool is four bytes whatever SPIR-V calls it.
 */
static Rndr::Forge::SpecializationType ToSpecializationType(const SpvReflectTypeDescription& type, Rndr::u32& out_byte_size)
{
    if ((type.type_flags & SPV_REFLECT_TYPE_FLAG_BOOL) != 0)
    {
        // VkBool32, so four bytes rather than the one a bool takes here.
        out_byte_size = 4;
        return Rndr::Forge::SpecializationType::Bool;
    }
    const Rndr::u32 width = type.traits.numeric.scalar.width;
    if ((type.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) != 0)
    {
        if (width == 32)
        {
            out_byte_size = 4;
            return Rndr::Forge::SpecializationType::Float32;
        }
        if (width == 64)
        {
            out_byte_size = 8;
            return Rndr::Forge::SpecializationType::Float64;
        }
    }
    if ((type.type_flags & SPV_REFLECT_TYPE_FLAG_INT) != 0)
    {
        const bool is_signed = type.traits.numeric.scalar.signedness != 0;
        if (width == 8 || width == 16 || width == 32)
        {
            // Reported as the 32 bit type so a caller can write a plain integer for it, but the map entry
            // still has to say 8 or 16 bits: VkSpecializationMapEntry::size must match the declared type.
            out_byte_size = width / 8;
            return is_signed ? Rndr::Forge::SpecializationType::Int32 : Rndr::Forge::SpecializationType::UInt32;
        }
        if (width == 64)
        {
            out_byte_size = 8;
            return is_signed ? Rndr::Forge::SpecializationType::Int64 : Rndr::Forge::SpecializationType::UInt64;
        }
    }
    throw Opal::Exception("Unsupported specialization constant type!");
}

namespace
{
/**
 * The vertex attributes one entry point reads. Built-ins are dropped: they occupy no location and come from
 * the implementation rather than from a vertex buffer, so a vertex input desc has nothing to say about them.
 */
void ReadInputs(const SpvReflectShaderModule& reflect_module, const char* entry_point,
                Opal::DynamicArray<Rndr::Forge::ShaderInputInfo>& out_inputs)
{
    Rndr::u32 count = 0;
    RequireReflected(spvReflectEnumerateEntryPointInputVariables(&reflect_module, entry_point, &count, nullptr), "inputs");
    if (count == 0)
    {
        return;
    }
    Opal::DynamicArray<SpvReflectInterfaceVariable*> variables(static_cast<Rndr::i32>(count));
    RequireReflected(spvReflectEnumerateEntryPointInputVariables(&reflect_module, entry_point, &count, variables.GetData()),
                     "inputs");
    for (Rndr::i32 i = 0; i < variables.GetSize(); ++i)
    {
        const SpvReflectInterfaceVariable& variable = *variables[i];
        if (variable.built_in != -1)
        {
            continue;
        }
        Rndr::Forge::ShaderInputInfo info;
        // Slang names a flattened struct member after the parameter it came from, so this reads "input.Pos"
        // rather than "Pos". Kept as reflection gives it - it is what appears in an error about a location.
        info.name = variable.name != nullptr ? Opal::StringUtf8(variable.name) : Opal::StringUtf8();
        info.location = variable.location;
        // SpvReflectFormat values are VkFormat values, which is what lets this go straight through.
        info.format = Rndr::FromVkFormat(static_cast<VkFormat>(variable.format));
        out_inputs.PushBack(std::move(info));
    }
}

/**
 * The kind Forge models a reflected descriptor as. A kind it does not model throws rather than being left
 * out: Forge can build neither a layout, a set nor an update for such a binding, so a shader declaring one
 * cannot use the descriptor path at all, and saying so at creation beats a binding that quietly went
 * missing from everything that reads this.
 */
Rndr::Forge::DescriptorType ToDescriptorType(SpvReflectDescriptorType type)
{
    switch (type)
    {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
            return Rndr::Forge::DescriptorType::Sampler;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            return Rndr::Forge::DescriptorType::CombinedImageSampler;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            return Rndr::Forge::DescriptorType::SampledImage;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            return Rndr::Forge::DescriptorType::StorageImage;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            return Rndr::Forge::DescriptorType::ConstantBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return Rndr::Forge::DescriptorType::StorageBuffer;
        default:
            throw Opal::Exception("A shader declares a descriptor of a kind Forge does not model!");
    }
}

/** The descriptors one entry point reads, across every set. */
void ReadBindings(const SpvReflectShaderModule& reflect_module, const char* entry_point,
                  Opal::DynamicArray<Rndr::Forge::ShaderBindingInfo>& out_bindings)
{
    Rndr::u32 count = 0;
    RequireReflected(spvReflectEnumerateEntryPointDescriptorBindings(&reflect_module, entry_point, &count, nullptr), "bindings");
    if (count == 0)
    {
        return;
    }
    Opal::DynamicArray<SpvReflectDescriptorBinding*> bindings(static_cast<Rndr::i32>(count));
    RequireReflected(spvReflectEnumerateEntryPointDescriptorBindings(&reflect_module, entry_point, &count, bindings.GetData()),
                     "bindings");
    for (Rndr::i32 i = 0; i < bindings.GetSize(); ++i)
    {
        const SpvReflectDescriptorBinding& binding = *bindings[i];
        Rndr::Forge::ShaderBindingInfo info;
        info.name = binding.name != nullptr ? Opal::StringUtf8(binding.name) : Opal::StringUtf8();
        info.set = binding.set;
        info.binding = binding.binding;
        info.descriptor_type = ToDescriptorType(binding.descriptor_type);
        info.descriptor_count = binding.count;
        out_bindings.PushBack(std::move(info));
    }
}

/** The push constant blocks one entry point reads. */
void ReadPushConstants(const SpvReflectShaderModule& reflect_module, const char* entry_point,
                       Opal::DynamicArray<Rndr::Forge::ShaderPushConstantInfo>& out_push_constants)
{
    Rndr::u32 count = 0;
    RequireReflected(spvReflectEnumerateEntryPointPushConstantBlocks(&reflect_module, entry_point, &count, nullptr),
                     "push constant blocks");
    if (count == 0)
    {
        return;
    }
    Opal::DynamicArray<SpvReflectBlockVariable*> blocks(static_cast<Rndr::i32>(count));
    RequireReflected(spvReflectEnumerateEntryPointPushConstantBlocks(&reflect_module, entry_point, &count, blocks.GetData()),
                     "push constant blocks");
    for (Rndr::i32 i = 0; i < blocks.GetSize(); ++i)
    {
        const SpvReflectBlockVariable& block = *blocks[i];
        Rndr::Forge::ShaderPushConstantInfo info;
        info.name = block.name != nullptr ? Opal::StringUtf8(block.name) : Opal::StringUtf8();
        info.offset = block.offset;
        info.size = block.size;
        out_push_constants.PushBack(std::move(info));
    }
}
}  // namespace

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
    // Everything below reads from the module and several of those steps throw, so closing it stops being
    // the business of each of them.
    const ReflectModuleGuard reflect_guard{&reflect_module};

    const SpvReflectEntryPoint* entry_point =
        spvReflectGetEntryPoint(&reflect_module, m_entry_point.GetData());
    if (entry_point == nullptr)
    {
        throw Opal::Exception("Entry point not found in SPIR-V module");
    }
    m_native_stage = ToNativeShaderStage(entry_point->shader_stage);
    m_stage = ToShaderTypeBits(entry_point->shader_stage);

    // Read while the reflect module is already open, so nothing is reflected twice and no reflection state
    // outlives this constructor - the name below is copied, not pointed at.
    u32 constant_count = 0;
    SpvReflectResult constants_result = spvReflectEnumerateSpecializationConstants(&reflect_module, &constant_count, nullptr);
    if (constants_result != SPV_REFLECT_RESULT_SUCCESS)
    {
        // Not taken to mean the module declares none, which would come back later as a pipeline refusing a
        // name the shader does in fact declare.
        throw Opal::Exception("Failed to count the specialization constants of a SPIR-V module!");
    }
    if (constant_count > 0)
    {
        Opal::DynamicArray<SpvReflectSpecializationConstant*> constants(static_cast<i32>(constant_count));
        constants_result = spvReflectEnumerateSpecializationConstants(&reflect_module, &constant_count, constants.GetData());
        if (constants_result != SPV_REFLECT_RESULT_SUCCESS)
        {
            throw Opal::Exception("Failed to read the specialization constants of a SPIR-V module!");
        }
        for (i32 i = 0; i < constants.GetSize(); ++i)
        {
            const SpvReflectSpecializationConstant& constant = *constants[i];
            if (constant.type_description == nullptr)
            {
                throw Opal::Exception("A specialization constant came back without a type!");
            }
            SpecializationConstantInfo info;
            info.name = constant.name != nullptr ? Opal::StringUtf8(constant.name) : Opal::StringUtf8();
            info.constant_id = constant.constant_id;
            info.type = ToSpecializationType(*constant.type_description, info.byte_size);
            info.default_value.type = info.type;
            // Reflection reports the default as raw bytes, four for anything 32 bit or smaller and eight for
            // the rest, low-order word first, so this copies the pattern rather than reinterpreting it.
            if (constant.default_value != nullptr && constant.default_value_size > 0)
            {
                const u32 size = constant.default_value_size < 8 ? constant.default_value_size : 8;
                memcpy(&info.default_value.bits, constant.default_value, size);
            }
            m_specialization_constants.PushBack(std::move(info));
        }
    }

    // Everything else this entry point reads: vertex attributes, descriptors and push constant blocks. Entry
    // point scoped rather than module wide, since a Slang file holds several of them and the module wide
    // enumerators report the union - a vertex shader would come back claiming the bindings of the fragment
    // shader beside it.
    ReadInputs(reflect_module, m_entry_point.GetData(), m_inputs);
    ReadBindings(reflect_module, m_entry_point.GetData(), m_bindings);
    ReadPushConstants(reflect_module, m_entry_point.GetData(), m_push_constants);

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
      m_entry_point(std::move(other.m_entry_point)),
      m_specialization_constants(std::move(other.m_specialization_constants)),
      m_inputs(std::move(other.m_inputs)),
      m_bindings(std::move(other.m_bindings)),
      m_push_constants(std::move(other.m_push_constants))
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
        m_specialization_constants = std::move(other.m_specialization_constants);
        m_inputs = std::move(other.m_inputs);
        m_bindings = std::move(other.m_bindings);
        m_push_constants = std::move(other.m_push_constants);
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
    m_specialization_constants.Clear();
    m_inputs.Clear();
    m_bindings.Clear();
    m_push_constants.Clear();
}
