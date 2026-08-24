#include "rndr/forge/shader.hpp"

#include "opal/exceptions.h"

#include "spirv_reflect.h"

#include "rndr/core/shader-cache.hpp"
#include "rndr/core/shader-compiler.hpp"
#include "rndr/file.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/log.hpp"

static Opal::Optional<VkShaderStageFlagBits> ToNativeShaderStage(SpvReflectShaderStageFlagBits stage)
{
    switch (stage)
    {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
            return Opal::Optional<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT);
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
            return Opal::Optional<VkShaderStageFlagBits>(VK_SHADER_STAGE_FRAGMENT_BIT);
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
            return Opal::Optional<VkShaderStageFlagBits>(VK_SHADER_STAGE_COMPUTE_BIT);
        case SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT:
            return Opal::Optional<VkShaderStageFlagBits>(VK_SHADER_STAGE_TASK_BIT_EXT);
        case SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT:
            return Opal::Optional<VkShaderStageFlagBits>(VK_SHADER_STAGE_MESH_BIT_EXT);
        default:
            return {};
    }
}

static Opal::Optional<Rndr::ShaderTypeBits> ToShaderTypeBits(SpvReflectShaderStageFlagBits stage)
{
    switch (stage)
    {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
            return Opal::Optional<Rndr::ShaderTypeBits>(Rndr::ShaderTypeBits::Vertex);
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
            return Opal::Optional<Rndr::ShaderTypeBits>(Rndr::ShaderTypeBits::Fragment);
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
            return Opal::Optional<Rndr::ShaderTypeBits>(Rndr::ShaderTypeBits::Compute);
        case SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT:
            return Opal::Optional<Rndr::ShaderTypeBits>(Rndr::ShaderTypeBits::Task);
        case SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT:
            return Opal::Optional<Rndr::ShaderTypeBits>(Rndr::ShaderTypeBits::Mesh);
        default:
            return {};
    }
}

namespace
{
/** Turns a reflection failure into the same line whichever of the enumerators produced it. */
Rndr::ErrorCode RequireReflected(SpvReflectResult result, const char* what)
{
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        RNDR_LOG_ERROR("Forge: failed to read the {} of a SPIR-V module", what);
        return Rndr::ErrorCode::CorruptData;
    }
    return Rndr::ErrorCode::Success;
}

/**
 * Closes the reflect module whichever way creation leaves it. Several of the steps that read from it give
 * up, and it is not something Opal owns and unwinds on its own.
 */
struct ReflectModuleGuard
{
    SpvReflectShaderModule* module;
    ~ReflectModuleGuard() { spvReflectDestroyShaderModule(module); }
};
}  // namespace

/**
 * The type of a specialization constant, from the flags and scalar traits reflection reports. A width or a
 * kind with no counterpart here is refused rather than rounded to the nearest one: a value written into a
 * constant of the wrong width is not something the caller could notice afterwards.
 *
 * @param out_byte_size Bytes the constant takes in the blob handed to Vulkan, which the specification wants
 *                      to be the byte size of the declared type. Reported apart from the type because a
 *                      narrow integer is reported as its 32 bit counterpart while still owing Vulkan its own
 *                      width, and because a bool is four bytes whatever SPIR-V calls it.
 */
static Opal::Optional<Rndr::Forge::SpecializationType> ToSpecializationType(const SpvReflectTypeDescription& type, Rndr::u32& out_byte_size)
{
    using Rndr::Forge::SpecializationType;

    if ((type.type_flags & SPV_REFLECT_TYPE_FLAG_BOOL) != 0)
    {
        // VkBool32, so four bytes rather than the one a bool takes here.
        out_byte_size = 4;
        return Opal::Optional<SpecializationType>(SpecializationType::Bool);
    }
    const Rndr::u32 width = type.traits.numeric.scalar.width;
    if ((type.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) != 0)
    {
        if (width == 32)
        {
            out_byte_size = 4;
            return Opal::Optional<SpecializationType>(SpecializationType::Float32);
        }
        if (width == 64)
        {
            out_byte_size = 8;
            return Opal::Optional<SpecializationType>(SpecializationType::Float64);
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
            return Opal::Optional<SpecializationType>(is_signed ? SpecializationType::Int32 : SpecializationType::UInt32);
        }
        if (width == 64)
        {
            out_byte_size = 8;
            return Opal::Optional<SpecializationType>(is_signed ? SpecializationType::Int64 : SpecializationType::UInt64);
        }
    }
    return {};
}

namespace
{
/**
 * The vertex attributes one entry point reads. Built-ins are dropped: they occupy no location and come from
 * the implementation rather than from a vertex buffer, so a vertex input desc has nothing to say about them.
 */
Rndr::ErrorCode ReadInputs(const SpvReflectShaderModule& reflect_module, const char* entry_point,
                           Opal::DynamicArray<Rndr::Forge::ShaderInputInfo>& out_inputs)
{
    Rndr::u32 count = 0;
    RNDR_FORGE_CHECK(
        RequireReflected(spvReflectEnumerateEntryPointInputVariables(&reflect_module, entry_point, &count, nullptr), "inputs"));
    if (count == 0)
    {
        return Rndr::ErrorCode::Success;
    }
    Opal::DynamicArray<SpvReflectInterfaceVariable*> variables(static_cast<Rndr::i32>(count));
    RNDR_FORGE_CHECK(
        RequireReflected(spvReflectEnumerateEntryPointInputVariables(&reflect_module, entry_point, &count, variables.GetData()), "inputs"));
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
    return Rndr::ErrorCode::Success;
}

/**
 * The kind Forge models a reflected descriptor as. A kind it does not model is refused rather than left
 * out: Forge can build neither a layout, a set nor an update for such a binding, so a shader declaring one
 * cannot use the descriptor path at all, and saying so at creation beats a binding that quietly went
 * missing from everything that reads this.
 */
Opal::Optional<Rndr::Forge::DescriptorType> ToDescriptorType(SpvReflectDescriptorType type)
{
    using Rndr::Forge::DescriptorType;

    switch (type)
    {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
            return Opal::Optional<DescriptorType>(DescriptorType::Sampler);
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            return Opal::Optional<DescriptorType>(DescriptorType::CombinedImageSampler);
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            return Opal::Optional<DescriptorType>(DescriptorType::SampledImage);
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            return Opal::Optional<DescriptorType>(DescriptorType::StorageImage);
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            return Opal::Optional<DescriptorType>(DescriptorType::ConstantBuffer);
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return Opal::Optional<DescriptorType>(DescriptorType::StorageBuffer);
        default:
            return {};
    }
}

/** The descriptors one entry point reads, across every set. */
Rndr::ErrorCode ReadBindings(const SpvReflectShaderModule& reflect_module, const char* entry_point,
                             Opal::DynamicArray<Rndr::Forge::ShaderBindingInfo>& out_bindings)
{
    Rndr::u32 count = 0;
    RNDR_FORGE_CHECK(
        RequireReflected(spvReflectEnumerateEntryPointDescriptorBindings(&reflect_module, entry_point, &count, nullptr), "bindings"));
    if (count == 0)
    {
        return Rndr::ErrorCode::Success;
    }
    Opal::DynamicArray<SpvReflectDescriptorBinding*> bindings(static_cast<Rndr::i32>(count));
    RNDR_FORGE_CHECK(RequireReflected(
        spvReflectEnumerateEntryPointDescriptorBindings(&reflect_module, entry_point, &count, bindings.GetData()), "bindings"));
    for (Rndr::i32 i = 0; i < bindings.GetSize(); ++i)
    {
        const SpvReflectDescriptorBinding& binding = *bindings[i];
        Rndr::Forge::ShaderBindingInfo info;
        info.name = binding.name != nullptr ? Opal::StringUtf8(binding.name) : Opal::StringUtf8();
        info.set = binding.set;
        info.binding = binding.binding;
        const Opal::Optional<Rndr::Forge::DescriptorType> descriptor_type = ToDescriptorType(binding.descriptor_type);
        if (!descriptor_type.HasValue())
        {
            RNDR_LOG_ERROR("Forge: a shader declares a descriptor of a kind Forge does not model");
            return Rndr::ErrorCode::UnsupportedFormat;
        }
        info.descriptor_type = descriptor_type.GetValue();
        info.descriptor_count = binding.count;
        out_bindings.PushBack(std::move(info));
    }
    return Rndr::ErrorCode::Success;
}

/** The push constant blocks one entry point reads. */
Rndr::ErrorCode ReadPushConstants(const SpvReflectShaderModule& reflect_module, const char* entry_point,
                                  Opal::DynamicArray<Rndr::Forge::ShaderPushConstantInfo>& out_push_constants)
{
    Rndr::u32 count = 0;
    RNDR_FORGE_CHECK(RequireReflected(spvReflectEnumerateEntryPointPushConstantBlocks(&reflect_module, entry_point, &count, nullptr),
                                      "push constant blocks"));
    if (count == 0)
    {
        return Rndr::ErrorCode::Success;
    }
    Opal::DynamicArray<SpvReflectBlockVariable*> blocks(static_cast<Rndr::i32>(count));
    RNDR_FORGE_CHECK(RequireReflected(
        spvReflectEnumerateEntryPointPushConstantBlocks(&reflect_module, entry_point, &count, blocks.GetData()), "push constant blocks"));
    for (Rndr::i32 i = 0; i < blocks.GetSize(); ++i)
    {
        const SpvReflectBlockVariable& block = *blocks[i];
        Rndr::Forge::ShaderPushConstantInfo info;
        info.name = block.name != nullptr ? Opal::StringUtf8(block.name) : Opal::StringUtf8();
        info.offset = block.offset;
        info.size = block.size;
        out_push_constants.PushBack(std::move(info));
    }
    return Rndr::ErrorCode::Success;
}
}  // namespace

/**
 * Whether a blob is long enough and marked well enough to be handed to a SPIR-V parser at all.
 *
 * Not a courtesy check. spirv-reflect reads the third word of the header - the generator - before it looks
 * at whether its own parser accepted the module, so an empty blob has it allocate one byte and then read four
 * at offset eight. The sanitizer catches that; a release build reads whatever is there. The parser is the
 * thing being asked to decide whether the bytes are SPIR-V, and it has to survive being handed bytes that
 * are not, so the size and the magic number are checked out here where the caller's blob still is.
 *
 * @param spirv_data The blob as the caller handed it over.
 * @return An explanation of what is wrong with it, or null when nothing is.
 */
static const char* WhyNotSpirv(Opal::ArrayView<const Rndr::u8> spirv_data)
{
    // Five words of header, then at least one instruction; and vkCreateShaderModule wants a byte count that
    // divides into words whatever reflection makes of it.
    constexpr Rndr::i64 k_header_size = 5 * sizeof(Rndr::u32);
    if (spirv_data.GetSize() < k_header_size)
    {
        return "SPIR-V data is shorter than a SPIR-V header!";
    }
    if (spirv_data.GetSize() % sizeof(Rndr::u32) != 0)
    {
        return "SPIR-V data is not a whole number of 32-bit words!";
    }
    constexpr Rndr::u32 k_spirv_magic = 0x07230203;
    Rndr::u32 magic = 0;
    memcpy(&magic, spirv_data.GetData(), sizeof(magic));
    if (magic != k_spirv_magic)
    {
        return "SPIR-V data does not start with the SPIR-V magic number!";
    }
    return nullptr;
}

Opal::Expected<Rndr::Forge::Shader, Rndr::ErrorCode> Rndr::Forge::Shader::FromSpirvInMemory(const Device& device,
                                                                                            Opal::ArrayView<const u8> spirv_data,
                                                                                            const ShaderDesc& desc)
{
    using Result = Opal::Expected<Shader, ErrorCode>;

    const char* not_spirv = WhyNotSpirv(spirv_data);
    if (not_spirv != nullptr)
    {
        RNDR_LOG_ERROR("Forge: {}", not_spirv);
        return Result(ErrorCode::CorruptData);
    }

    Shader shader;
    shader.m_device = device;
    shader.m_entry_point = desc.entry_point.Clone();

    // Use spirv-reflect to detect the shader stage from the specified entry point.
    SpvReflectShaderModule reflect_module = {};
    const SpvReflectResult reflect_result = spvReflectCreateShaderModule(spirv_data.GetSize(), spirv_data.GetData(), &reflect_module);
    if (reflect_result != SPV_REFLECT_RESULT_SUCCESS)
    {
        RNDR_LOG_ERROR("Forge: failed to create a SPIR-V reflect module");
        return Result(ErrorCode::CorruptData);
    }
    // Everything below reads from the module and several of those steps give up, so closing it stops being
    // the business of each of them.
    const ReflectModuleGuard reflect_guard{&reflect_module};

    const SpvReflectEntryPoint* entry_point = spvReflectGetEntryPoint(&reflect_module, shader.m_entry_point.GetData());
    if (entry_point == nullptr)
    {
        RNDR_LOG_ERROR("Forge: the SPIR-V module has no entry point named {}",
                       reinterpret_cast<const char*>(shader.m_entry_point.GetData()));
        return Result(ErrorCode::InvalidArgument);
    }
    const Opal::Optional<VkShaderStageFlagBits> native_stage = ToNativeShaderStage(entry_point->shader_stage);
    const Opal::Optional<ShaderTypeBits> stage = ToShaderTypeBits(entry_point->shader_stage);
    if (!native_stage.HasValue() || !stage.HasValue())
    {
        RNDR_LOG_ERROR("Forge: the entry point is of a shader stage Forge does not model");
        return Result(ErrorCode::UnsupportedFormat);
    }
    shader.m_native_stage = native_stage.GetValue();
    shader.m_stage = stage.GetValue();

    // Read while the reflect module is already open, so nothing is reflected twice and no reflection state
    // outlives this constructor - the name below is copied, not pointed at.
    u32 constant_count = 0;
    SpvReflectResult constants_result = spvReflectEnumerateSpecializationConstants(&reflect_module, &constant_count, nullptr);
    if (constants_result != SPV_REFLECT_RESULT_SUCCESS)
    {
        // Not taken to mean the module declares none, which would come back later as a pipeline refusing a
        // name the shader does in fact declare.
        RNDR_LOG_ERROR("Forge: failed to count the specialization constants of a SPIR-V module");
        return Result(ErrorCode::CorruptData);
    }
    if (constant_count > 0)
    {
        Opal::DynamicArray<SpvReflectSpecializationConstant*> constants(static_cast<i32>(constant_count));
        constants_result = spvReflectEnumerateSpecializationConstants(&reflect_module, &constant_count, constants.GetData());
        if (constants_result != SPV_REFLECT_RESULT_SUCCESS)
        {
            RNDR_LOG_ERROR("Forge: failed to read the specialization constants of a SPIR-V module");
            return Result(ErrorCode::CorruptData);
        }
        for (i32 i = 0; i < constants.GetSize(); ++i)
        {
            const SpvReflectSpecializationConstant& constant = *constants[i];
            if (constant.type_description == nullptr)
            {
                RNDR_LOG_ERROR("Forge: a specialization constant came back without a type");
                return Result(ErrorCode::CorruptData);
            }
            SpecializationConstantInfo info;
            info.name = constant.name != nullptr ? Opal::StringUtf8(constant.name) : Opal::StringUtf8();
            info.constant_id = constant.constant_id;
            const Opal::Optional<SpecializationType> constant_type = ToSpecializationType(*constant.type_description, info.byte_size);
            if (!constant_type.HasValue())
            {
                RNDR_LOG_ERROR("Forge: a specialization constant is of a type Forge does not model");
                return Result(ErrorCode::UnsupportedFormat);
            }
            info.type = constant_type.GetValue();
            info.default_value.type = info.type;
            // Reflection reports the default as raw bytes, four for anything 32 bit or smaller and eight for
            // the rest, low-order word first, so this copies the pattern rather than reinterpreting it.
            if (constant.default_value != nullptr && constant.default_value_size > 0)
            {
                const u32 size = constant.default_value_size < 8 ? constant.default_value_size : 8;
                memcpy(&info.default_value.bits, constant.default_value, size);
            }
            shader.m_specialization_constants.PushBack(std::move(info));
        }
    }

    // Everything else this entry point reads: vertex attributes, descriptors and push constant blocks. Entry
    // point scoped rather than module wide, since a Slang file holds several of them and the module wide
    // enumerators report the union - a vertex shader would come back claiming the bindings of the fragment
    // shader beside it.
    RNDR_FORGE_CHECK_EXPECTED(ReadInputs(reflect_module, shader.m_entry_point.GetData(), shader.m_inputs), Result);
    RNDR_FORGE_CHECK_EXPECTED(ReadBindings(reflect_module, shader.m_entry_point.GetData(), shader.m_bindings), Result);
    RNDR_FORGE_CHECK_EXPECTED(ReadPushConstants(reflect_module, shader.m_entry_point.GetData(), shader.m_push_constants), Result);

    const VkShaderModuleCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv_data.GetSize(),
        .pCode = reinterpret_cast<const u32*>(spirv_data.GetData()),
    };
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateShaderModule(device.GetNativeDevice(), &create_info, nullptr, &shader.m_shader_module),
                                 "vkCreateShaderModule", Result);
    return Result(std::move(shader));
}

Opal::Expected<Rndr::Forge::Shader, Rndr::ErrorCode> Rndr::Forge::Shader::FromSpirvFile(const Device& device, const Opal::StringUtf8& path,
                                                                                        const ShaderDesc& desc)
{
    using Result = Opal::Expected<Shader, ErrorCode>;

    Opal::DynamicArray<u8> spirv_data = File::ReadEntireFile(path);
    if (spirv_data.IsEmpty())
    {
        RNDR_LOG_ERROR("Forge: the SPIR-V file could not be read or is empty: {}", reinterpret_cast<const char*>(path.GetData()));
        return Result(ErrorCode::FileNotFound);
    }
    return FromSpirvInMemory(device, Opal::ArrayView<const u8>(spirv_data.GetData(), spirv_data.GetSize()), desc);
}

Opal::Expected<Rndr::Forge::Shader, Rndr::ErrorCode> Rndr::Forge::Shader::FromSourceInMemory(const Device& device,
                                                                                             const Opal::StringUtf8& source,
                                                                                             const ShaderDesc& desc)
{
    using Result = Opal::Expected<Shader, ErrorCode>;

    if (source.IsEmpty())
    {
        RNDR_LOG_ERROR("Forge: the shader source is empty");
        return Result(ErrorCode::InvalidArgument);
    }

    // Slang is the whole cost of getting here, so it is asked last. A hit reads a file and never creates a
    // session - even the build tag in the key comes from a free function.
    ShaderCacheKey key;
    if (desc.cache != nullptr)
    {
        key = ShaderCacheKey::Make(source, desc.entry_point, ShaderOutputFormat::SpirV);
        const Opal::DynamicArray<u8> cached = desc.cache->Find(key);
        if (!cached.IsEmpty())
        {
            return FromSpirvInMemory(device, Opal::ArrayView<const u8>(cached.GetData(), cached.GetSize()), desc);
        }
    }

    // ShaderCompiler is shared with Canvas and reports by throwing, which is the one thing that reaches
    // Forge from outside its own convention. Caught here so it stops at the boundary and comes out as a
    // code like everything else; the message it carries is what the log line below says.
    Opal::DynamicArray<u8> code;
    try
    {
        ShaderCompiler compiler;
        compiler.LoadModule(source, ShaderOutputFormat::SpirV);
        CompileResult result = compiler.CompileEntryPoint(desc.entry_point);
        code = std::move(result.code);
    }
    catch (const Opal::Exception& exception)
    {
        RNDR_LOG_ERROR("Forge: compiling the shader failed: {}", *exception.What());
        return Result(ErrorCode::ShaderCompilationError);
    }
    if (desc.cache != nullptr)
    {
        desc.cache->Store(key, {code.GetData(), code.GetSize()});
    }
    return FromSpirvInMemory(device, Opal::ArrayView<const u8>(code.GetData(), code.GetSize()), desc);
}

Opal::Expected<Rndr::Forge::Shader, Rndr::ErrorCode> Rndr::Forge::Shader::FromSource(const Device& device, const Opal::StringUtf8& path,
                                                                                     const ShaderDesc& desc)
{
    using Result = Opal::Expected<Shader, ErrorCode>;

    const Opal::StringUtf8 source = File::ReadEntireTextFile(path);
    if (source.IsEmpty())
    {
        RNDR_LOG_ERROR("Forge: the shader file could not be read or is empty: {}", reinterpret_cast<const char*>(path.GetData()));
        return Result(ErrorCode::FileNotFound);
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
