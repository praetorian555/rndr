#include "rndr/forge/descriptor-set.hpp"

#include "opal/container/in-place-array.h"
#include "rndr/forge/buffer.hpp"
#include "rndr/forge/texture.hpp"

#include "rndr/forge/device.hpp"
#include "rndr/forge/shader.hpp"
#include "rndr/forge/vulkan-result.hpp"
#include "rndr/log.hpp"

namespace
{
Opal::Optional<VkDescriptorType> FromDescriptorType(Rndr::Forge::DescriptorType descriptor_type)
{
    switch (descriptor_type)
    {
        case Rndr::Forge::DescriptorType::SampledImage:
            return Opal::Optional<VkDescriptorType>(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        case Rndr::Forge::DescriptorType::Sampler:
            return Opal::Optional<VkDescriptorType>(VK_DESCRIPTOR_TYPE_SAMPLER);
        case Rndr::Forge::DescriptorType::CombinedImageSampler:
            return Opal::Optional<VkDescriptorType>(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        case Rndr::Forge::DescriptorType::ConstantBuffer:
            return Opal::Optional<VkDescriptorType>(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        case Rndr::Forge::DescriptorType::StorageBuffer:
            return Opal::Optional<VkDescriptorType>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        case Rndr::Forge::DescriptorType::StorageImage:
            return Opal::Optional<VkDescriptorType>(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        default:
            return {};
    }
}

VkShaderStageFlags FromShaderTypeBits(Rndr::ShaderTypeBits shader_types)
{
    VkShaderStageFlags flags = 0;
    if (!!(shader_types & Rndr::ShaderTypeBits::Vertex))
    {
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if (!!(shader_types & Rndr::ShaderTypeBits::Fragment))
    {
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    if (!!(shader_types & Rndr::ShaderTypeBits::Compute))
    {
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    }
    if (!!(shader_types & Rndr::ShaderTypeBits::Task))
    {
        flags |= VK_SHADER_STAGE_TASK_BIT_EXT;
    }
    if (!!(shader_types & Rndr::ShaderTypeBits::Mesh))
    {
        flags |= VK_SHADER_STAGE_MESH_BIT_EXT;
    }
    if (!!(shader_types & Rndr::ShaderTypeBits::AllGraphics))
    {
        flags |= VK_SHADER_STAGE_ALL_GRAPHICS;
    }
    return flags;
}

}  // namespace

Rndr::ErrorCode Rndr::Forge::DescriptorPoolDesc::Add(DescriptorType descriptor_type, u32 max_size)
{
    for (const auto& pair : descriptor_types)
    {
        if (pair.key == descriptor_type)
        {
            RNDR_LOG_ERROR("Forge: this pool desc already names the descriptor type {}", static_cast<u32>(descriptor_type));
            return ErrorCode::InvalidArgument;
        }
    }
    descriptor_types.PushBack({.key = descriptor_type, .value = max_size});
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::Forge::DescriptorSetLayoutDesc::AddBinding(u32 binding, DescriptorType descriptor_type, u32 descriptor_count,
                                                                 ShaderTypeBits shader_types,
                                                                 Opal::ArrayView<const Opal::Ref<const Sampler>> immutable_samplers,
                                                                 DescriptorBindingFlagBits flags)
{
    for (const Binding& existing : bindings)
    {
        if (existing.binding == binding)
        {
            RNDR_LOG_ERROR("Forge: binding index {} is already used by this layout", binding);
            return ErrorCode::InvalidArgument;
        }
    }
    if (!immutable_samplers.IsEmpty())
    {
        if (descriptor_type != DescriptorType::Sampler && descriptor_type != DescriptorType::CombinedImageSampler)
        {
            RNDR_LOG_ERROR("Forge: immutable samplers are only valid on sampler descriptors");
            return ErrorCode::InvalidArgument;
        }
        if (immutable_samplers.GetSize() != static_cast<u64>(descriptor_count))
        {
            RNDR_LOG_ERROR("Forge: {} immutable samplers were given for a binding holding {} descriptors", immutable_samplers.GetSize(),
                           descriptor_count);
            return ErrorCode::InvalidArgument;
        }
    }

    Binding new_binding;
    new_binding.binding = binding;
    new_binding.descriptor_type = descriptor_type;
    new_binding.descriptor_count = descriptor_count;
    new_binding.shader_types = shader_types;
    new_binding.flags = flags;
    for (const Opal::Ref<const Sampler>& sampler : immutable_samplers)
    {
        if (!sampler.IsValid())
        {
            RNDR_LOG_ERROR("Forge: an immutable sampler of this binding is empty");
            return ErrorCode::InvalidArgument;
        }
        new_binding.immutable_samplers.PushBack(sampler.Clone());
    }
    bindings.PushBack(std::move(new_binding));
    return ErrorCode::Success;
}

// DescriptorPool

Rndr::Forge::DescriptorSetUpdateBinding Rndr::Forge::DescriptorSetUpdateBinding::Clone(Opal::AllocatorBase* allocator) const
{
    DescriptorSetUpdateBinding clone{.descriptor_type = descriptor_type,
                                     .binding = binding,
                                     .array_element = array_element,
                                     .resource_info = resource_info.Clone(allocator)};
    return clone;
}

Opal::Expected<Rndr::Forge::DescriptorPool, Rndr::ErrorCode> Rndr::Forge::DescriptorPool::Create(const Device& device,
                                                                                                 const DescriptorPoolDesc& desc)
{
    using Result = Opal::Expected<DescriptorPool, ErrorCode>;

    Opal::DynamicArray<VkDescriptorPoolSize> pool_sizes;
    for (const auto& pair : desc.descriptor_types)
    {
        RNDR_FORGE_TRANSLATE_EXPECTED(descriptor_type, FromDescriptorType(pair.key), "the descriptor type of a pool size", Result);
        const VkDescriptorPoolSize pool_size{
            .type = descriptor_type,
            .descriptorCount = pair.value,
        };
        pool_sizes.PushBack(pool_size);
    }

    if (pool_sizes.IsEmpty())
    {
        RNDR_LOG_ERROR("Forge: a descriptor pool needs at least one kind of descriptor");
        return Result(ErrorCode::InvalidArgument);
    }

    DescriptorPool pool;
    pool.m_device = device;
    pool.m_desc = desc.Clone();

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<u32>(pool_sizes.GetSize());
    pool_info.pPoolSizes = pool_sizes.GetData();
    pool_info.maxSets = desc.max_sets;
    if (desc.use_update_after_bind)
    {
        pool_info.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    }
    if (desc.free_individual_sets)
    {
        pool_info.flags |= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    }

    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateDescriptorPool(device.GetNativeDevice(), &pool_info, nullptr, &pool.m_pool),
                                 "vkCreateDescriptorPool", Result);
    return Result(std::move(pool));
}

Rndr::Forge::DescriptorPool::~DescriptorPool()
{
    Destroy();
}

Rndr::Forge::DescriptorPool::DescriptorPool(DescriptorPool&& other) noexcept
    : m_device(std::move(other.m_device)), m_pool(other.m_pool), m_desc(std::move(other.m_desc))
{
    other.m_pool = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::Forge::DescriptorPool& Rndr::Forge::DescriptorPool::operator=(DescriptorPool&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_pool = other.m_pool;
        m_desc = std::move(other.m_desc);
        other.m_pool = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::Forge::DescriptorPool::Destroy()
{
    if (m_pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device->GetNativeDevice(), m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
}

Rndr::ErrorCode Rndr::Forge::DescriptorPool::Reset()
{
    if (m_pool == VK_NULL_HANDLE)
    {
        return ErrorCode::Success;
    }
    RNDR_FORGE_VK_CHECK(vkResetDescriptorPool(m_device->GetNativeDevice(), m_pool, 0), "vkResetDescriptorPool");
    return ErrorCode::Success;
}

VkDevice Rndr::Forge::DescriptorPool::GetNativeDevice() const
{
    return m_device->GetNativeDevice();
}

// DescriptorSetLayout

namespace
{
/** Reads as "CombinedImageSampler" rather than "2", so a disagreement says which two kinds disagreed. */
const char* DescriptorTypeName(Rndr::Forge::DescriptorType type)
{
    switch (type)
    {
        case Rndr::Forge::DescriptorType::SampledImage:
            return "SampledImage";
        case Rndr::Forge::DescriptorType::Sampler:
            return "Sampler";
        case Rndr::Forge::DescriptorType::CombinedImageSampler:
            return "CombinedImageSampler";
        case Rndr::Forge::DescriptorType::ConstantBuffer:
            return "ConstantBuffer";
        case Rndr::Forge::DescriptorType::StorageBuffer:
            return "StorageBuffer";
        case Rndr::Forge::DescriptorType::StorageImage:
            return "StorageImage";
        default:
            return "unknown";
    }
}

/** Whether `declared` names the stage a shader of that type runs at, AllGraphics counting for all but compute. */
bool CoversStage(Rndr::ShaderTypeBits declared, Rndr::ShaderTypeBits stage)
{
    if (!!(declared & stage))
    {
        return true;
    }
    return !!(declared & Rndr::ShaderTypeBits::AllGraphics) && stage != Rndr::ShaderTypeBits::Compute;
}

/**
 * Check a hand-written layout against the shaders it is meant to match, and give each binding the name the
 * shader uses for it.
 *
 * Every disagreement here is one Vulkan only complains about much later, at the draw that binds the set: a
 * binding declared as the wrong kind, one sized for fewer descriptors than the shader indexes, one whose
 * stages leave out a stage that reads it, and a binding the shaders read that the layout never declared.
 *
 * A binding the layout declares that no shader reads is *not* one of them. A descriptor nothing samples is
 * optimised out of the SPIR-V, so reflection cannot tell such a binding apart from one that was never
 * declared - the sample's own metallic roughness texture is bound and, for now, unread. Those bindings keep
 * their name empty and stay out of the by-name lookup, which is the whole of the cost.
 */
Rndr::ErrorCode CheckAgainstShaders(Rndr::Forge::DescriptorSetLayoutDesc& desc)
{
    for (Rndr::Forge::DescriptorSetLayoutDesc::Binding& target : desc.bindings)
    {
        Rndr::ShaderTypeBits declaring_stages = {};
        const Rndr::Forge::ShaderBindingInfo* found = nullptr;
        for (const Opal::Ref<const Rndr::Forge::Shader>& shader : desc.shaders)
        {
            const Opal::ArrayView<const Rndr::Forge::ShaderBindingInfo> bindings = shader->GetBindings();
            for (Rndr::i32 i = 0; i < bindings.GetSize(); ++i)
            {
                const Rndr::Forge::ShaderBindingInfo& candidate = bindings[i];
                if (candidate.set != desc.set_index || candidate.binding != target.binding)
                {
                    continue;
                }
                found = &candidate;
                declaring_stages |= shader->GetShaderStage();
            }
        }
        if (found == nullptr)
        {
            // Not an error: a descriptor no shader samples is not in the SPIR-V to be found. It keeps an
            // empty name, so only the by-name lookup notices it is missing.
            continue;
        }
        if (found->descriptor_type != target.descriptor_type)
        {
            RNDR_LOG_ERROR("Forge: binding {} is declared as a {} by the shader and as a {} here", target.binding,
                           DescriptorTypeName(found->descriptor_type), DescriptorTypeName(target.descriptor_type));
            return Rndr::ErrorCode::InvalidArgument;
        }
        // A layout may hold more descriptors than the shader indexes - that is how a bindless array is
        // written - but never fewer.
        if (target.descriptor_count < found->descriptor_count)
        {
            RNDR_LOG_ERROR("Forge: binding {} holds {} descriptors and the shader reads {} of them", target.binding,
                           target.descriptor_count, found->descriptor_count);
            return Rndr::ErrorCode::InvalidArgument;
        }
        for (const Rndr::ShaderTypeBits stage : {Rndr::ShaderTypeBits::Vertex, Rndr::ShaderTypeBits::Fragment,
                                                 Rndr::ShaderTypeBits::Compute, Rndr::ShaderTypeBits::Task, Rndr::ShaderTypeBits::Mesh})
        {
            if (!!(declaring_stages & stage) && !CoversStage(target.shader_types, stage))
            {
                RNDR_LOG_ERROR("Forge: binding {} is read by a stage this layout does not name", target.binding);
                return Rndr::ErrorCode::InvalidArgument;
            }
        }
        target.name = found->name.Clone();
    }

    for (const Opal::Ref<const Rndr::Forge::Shader>& shader : desc.shaders)
    {
        const Opal::ArrayView<const Rndr::Forge::ShaderBindingInfo> bindings = shader->GetBindings();
        for (Rndr::i32 i = 0; i < bindings.GetSize(); ++i)
        {
            const Rndr::Forge::ShaderBindingInfo& declared = bindings[i];
            if (declared.set != desc.set_index)
            {
                continue;
            }
            bool present = false;
            for (const Rndr::Forge::DescriptorSetLayoutDesc::Binding& target : desc.bindings)
            {
                if (target.binding == declared.binding)
                {
                    present = true;
                    break;
                }
            }
            if (!present)
            {
                RNDR_LOG_ERROR("Forge: a shader reads {} at binding {} of set {} and this layout does not declare it",
                               reinterpret_cast<const char*>(declared.name.GetData()), declared.binding, desc.set_index);
                return Rndr::ErrorCode::InvalidArgument;
            }
        }
    }
    return Rndr::ErrorCode::Success;
}

}  // namespace

Opal::Expected<Rndr::Forge::DescriptorSetLayout, Rndr::ErrorCode> Rndr::Forge::DescriptorSetLayout::Create(
    const Device& device, const DescriptorSetLayoutDesc& desc)
{
    using Result = Opal::Expected<DescriptorSetLayout, ErrorCode>;

    DescriptorSetLayout layout;
    layout.m_device = device;
    layout.m_desc = desc.Clone();
    // Checked against the clone rather than desc: the names it fills in are what this layout hands to every
    // set allocated from it, and desc is the caller's to keep unchanged.
    if (!layout.m_desc.shaders.IsEmpty())
    {
        RNDR_FORGE_CHECK_EXPECTED(CheckAgainstShaders(layout.m_desc), Result);
        // Dropped once they have been read. Nothing later needs them, and keeping the references would make
        // GetDesc() hand out handles to shaders the caller was told it may destroy.
        layout.m_desc.shaders.Clear();
    }
    Opal::DynamicArray<VkDescriptorSetLayoutBinding> bindings(desc.bindings.GetSize());
    Opal::DynamicArray<VkDescriptorBindingFlags> binding_flags_array(desc.bindings.GetSize());

    // One array for every immutable sampler of every binding, sized up front so that the pointers handed to
    // pImmutableSamplers stay valid until vkCreateDescriptorSetLayout has read them.
    u64 immutable_sampler_count = 0;
    for (const DescriptorSetLayoutDesc::Binding& source : desc.bindings)
    {
        immutable_sampler_count += source.immutable_samplers.GetSize();
    }
    Opal::DynamicArray<VkSampler> immutable_samplers(immutable_sampler_count);
    u64 next_immutable_sampler = 0;

    // The binding a variable count may sit on is the one with the highest index, which is not necessarily the
    // last one in the desc, since 2.6 let bindings arrive in any order.
    u32 highest_binding_index = 0;
    for (const DescriptorSetLayoutDesc::Binding& source : desc.bindings)
    {
        highest_binding_index = Opal::Max(highest_binding_index, source.binding);
    }

    const DeviceFeatures& features = device.GetFeatures();
    bool has_update_after_bind = false;
    for (i32 i = 0; i < bindings.GetSize(); i++)
    {
        const DescriptorSetLayoutDesc::Binding& source = desc.bindings[i];
        VkDescriptorSetLayoutBinding& binding = bindings[i];
        binding.binding = source.binding;
        RNDR_FORGE_TRANSLATE_EXPECTED(descriptor_type, FromDescriptorType(source.descriptor_type),
                                      "the descriptor type of a layout binding", Result);
        binding.descriptorType = descriptor_type;
        binding.descriptorCount = source.descriptor_count;
        binding.stageFlags = FromShaderTypeBits(source.shader_types);
        binding.pImmutableSamplers = nullptr;
        if (!source.immutable_samplers.IsEmpty())
        {
            binding.pImmutableSamplers = immutable_samplers.GetData() + next_immutable_sampler;
            for (const Opal::Ref<const Sampler>& sampler : source.immutable_samplers)
            {
                immutable_samplers[next_immutable_sampler++] = sampler->GetNativeSampler();
            }
        }

        // The values of DescriptorBindingFlagBits mirror VkDescriptorBindingFlagBits, so the mask is a cast.
        binding_flags_array[i] = static_cast<VkDescriptorBindingFlags>(source.flags);
        if (!!(source.flags & DescriptorBindingFlagBits::UpdateAfterBind))
        {
            if (!features.update_after_bind_descriptors)
            {
                RNDR_LOG_ERROR(
                    "Forge: an update after bind binding needs the device created with "
                    "DeviceFeatures::update_after_bind_descriptors");
                return Result(ErrorCode::InvalidArgument);
            }
            has_update_after_bind = true;
        }
        if (!!(source.flags & DescriptorBindingFlagBits::PartiallyBound) && !features.partially_bound_descriptors)
        {
            RNDR_LOG_ERROR("Forge: a partially bound binding needs the device created with DeviceFeatures::partially_bound_descriptors");
            return Result(ErrorCode::InvalidArgument);
        }
        if (!!(source.flags & DescriptorBindingFlagBits::VariableDescriptorCount))
        {
            if (!features.variable_descriptor_count)
            {
                RNDR_LOG_ERROR("Forge: a variable count binding needs the device created with DeviceFeatures::variable_descriptor_count");
                return Result(ErrorCode::InvalidArgument);
            }
            if (source.binding != highest_binding_index)
            {
                RNDR_LOG_ERROR("Forge: only the binding with the highest index may have a variable descriptor count");
                return Result(ErrorCode::InvalidArgument);
            }
        }
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info{};
    binding_flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags_info.bindingCount = static_cast<u32>(binding_flags_array.GetSize());
    binding_flags_info.pBindingFlags = binding_flags_array.GetData();

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<u32>(bindings.GetSize());
    layout_info.pBindings = bindings.GetData();
    // Only asked for when a binding actually wants it, since a layout that has it can only be allocated from
    // a pool that has the matching flag.
    layout_info.flags = has_update_after_bind ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0;
    layout_info.pNext = &binding_flags_info;

    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateDescriptorSetLayout(device.GetNativeDevice(), &layout_info, nullptr, &layout.m_layout),
                                 "vkCreateDescriptorSetLayout", Result);
    return Result(std::move(layout));
}

Rndr::Forge::DescriptorSetLayout::~DescriptorSetLayout()
{
    Destroy();
}

Rndr::Forge::DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
    : m_device(std::move(other.m_device)), m_layout(other.m_layout), m_desc(std::move(other.m_desc))
{
    other.m_layout = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::Forge::DescriptorSetLayout& Rndr::Forge::DescriptorSetLayout::operator=(DescriptorSetLayout&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_layout = other.m_layout;
        m_desc = std::move(other.m_desc);
        other.m_layout = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::Forge::DescriptorSetLayout::Destroy()
{
    if (m_layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device->GetNativeDevice(), m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
}

// DescriptorSet

Opal::Expected<Rndr::Forge::DescriptorSet, Rndr::ErrorCode> Rndr::Forge::DescriptorSet::Create(const DescriptorPool& pool,
                                                                                               const DescriptorSetLayout& layout,
                                                                                               u32 variable_descriptor_count)
{
    using Result = Opal::Expected<DescriptorSet, ErrorCode>;

    DescriptorSet descriptor_set;
    descriptor_set.m_device = pool.GetNativeDevice();
    descriptor_set.m_pool = pool;

    // The binding a variable count applies to, which is the one the layout marked and there is at most one.
    const DescriptorSetLayoutDesc::Binding* variable_binding = nullptr;
    bool layout_needs_update_after_bind_pool = false;
    for (const DescriptorSetLayoutDesc::Binding& binding : layout.GetDesc().bindings)
    {
        if (!!(binding.flags & DescriptorBindingFlagBits::VariableDescriptorCount))
        {
            variable_binding = &binding;
        }
        layout_needs_update_after_bind_pool =
            layout_needs_update_after_bind_pool || !!(binding.flags & DescriptorBindingFlagBits::UpdateAfterBind);
    }
    // A layout with an update after bind binding can only be allocated from a pool that was told to expect
    // one, and the two are created apart, so this is the first place that can see both.
    if (layout_needs_update_after_bind_pool && !pool.GetDesc().use_update_after_bind)
    {
        RNDR_LOG_ERROR(
            "Forge: a layout with an update after bind binding needs a pool created with "
            "DescriptorPoolDesc::use_update_after_bind");
        return Result(ErrorCode::InvalidArgument);
    }
    if (variable_descriptor_count > 0)
    {
        if (variable_binding == nullptr)
        {
            RNDR_LOG_ERROR(
                "Forge: a variable descriptor count needs a layout binding with "
                "DescriptorBindingFlagBits::VariableDescriptorCount");
            return Result(ErrorCode::InvalidArgument);
        }
        if (variable_descriptor_count > variable_binding->descriptor_count)
        {
            RNDR_LOG_ERROR("Forge: a variable descriptor count of {} is above the {} its binding declares", variable_descriptor_count,
                           variable_binding->descriptor_count);
            return Result(ErrorCode::InvalidArgument);
        }
    }

    VkDescriptorSetLayout native_layout = layout.GetNativeDescriptorSetLayout();
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = pool.GetNativeDescriptorPool();
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &native_layout;

    VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count_info{};
    if (variable_descriptor_count > 0)
    {
        variable_count_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variable_count_info.descriptorSetCount = 1;
        variable_count_info.pDescriptorCounts = &variable_descriptor_count;
        alloc_info.pNext = &variable_count_info;
    }

    RNDR_FORGE_VK_CHECK_EXPECTED(vkAllocateDescriptorSets(descriptor_set.m_device, &alloc_info, &descriptor_set.m_set),
                                 "vkAllocateDescriptorSets", Result);

    // Copied rather than referenced: a layout may be destroyed while sets allocated from it live on,
    // and this is the whole of what the Update overloads need from it.
    descriptor_set.m_binding_types.Reserve(layout.GetDesc().bindings.GetSize());
    for (const DescriptorSetLayoutDesc::Binding& binding : layout.GetDesc().bindings)
    {
        // The variable binding holds what this set was allocated with, not what the layout declared, so an
        // element inside the declared array but outside the allocated one is caught here as well.
        const bool is_variable = variable_descriptor_count > 0 && &binding == variable_binding;
        descriptor_set.m_binding_types.PushBack({.binding = binding.binding,
                                                 .descriptor_type = binding.descriptor_type,
                                                 .descriptor_count = is_variable ? variable_descriptor_count : binding.descriptor_count,
                                                 .name = binding.name.Clone()});
    }
    return Result(std::move(descriptor_set));
}

Rndr::Forge::DescriptorSet::~DescriptorSet()
{
    Destroy();
}

Rndr::Forge::DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept
    : m_device(other.m_device), m_set(other.m_set), m_pool(std::move(other.m_pool)), m_binding_types(std::move(other.m_binding_types))
{
    other.m_set = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
    other.m_pool = nullptr;
}

Rndr::Forge::DescriptorSet& Rndr::Forge::DescriptorSet::operator=(DescriptorSet&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = other.m_device;
        m_set = other.m_set;
        m_pool = std::move(other.m_pool);
        m_binding_types = std::move(other.m_binding_types);
        other.m_set = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
        other.m_pool = nullptr;
    }
    return *this;
}

void Rndr::Forge::DescriptorSet::Destroy()
{
    // A pool without the free-descriptor-set flag hands its memory back only on Reset or on destruction, and calling
    // vkFreeDescriptorSets on it is invalid, so there the handle is all there is to drop.
    if (m_set != VK_NULL_HANDLE && m_pool != nullptr && m_pool->IsValid() && m_pool->GetDesc().free_individual_sets)
    {
        vkFreeDescriptorSets(m_device, m_pool->GetNativeDescriptorPool(), 1, &m_set);
    }
    m_set = VK_NULL_HANDLE;
    m_pool = nullptr;
    m_device = VK_NULL_HANDLE;
    m_binding_types.Clear();
}

Rndr::ErrorCode Rndr::Forge::DescriptorSet::Update(Opal::ArrayView<const DescriptorSetUpdateBinding> updates)
{
    // One slot per update in each array, written by index. PushBack would grow them past the size they were
    // built with, and the pointers already handed to earlier writes would follow the old allocation.
    Opal::DynamicArray<VkWriteDescriptorSet> descriptor_writes(updates.GetSize());
    Opal::DynamicArray<VkDescriptorBufferInfo> buffer_infos(updates.GetSize());
    Opal::DynamicArray<VkDescriptorImageInfo> image_infos(updates.GetSize());
    for (i32 i = 0; i < updates.GetSize(); i++)
    {
        VkWriteDescriptorSet& descriptor_write = descriptor_writes[i];
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.pNext = nullptr;
        descriptor_write.dstSet = m_set;
        // Vulkan writes one descriptor per element and reports nothing when the element is past the end of
        // the binding, so an off-by-one in a bindless array lands somewhere undefined rather than here.
        const Opal::Expected<const BindingInfo&, ErrorCode> binding_info = FindBinding(updates[i].binding);
        if (!binding_info.HasValue())
        {
            return binding_info.GetError();
        }
        if (updates[i].array_element >= binding_info.GetValue().descriptor_count)
        {
            RNDR_LOG_ERROR("Forge: binding {} holds {} descriptors, so there is no element {} to write", updates[i].binding,
                           binding_info.GetValue().descriptor_count, updates[i].array_element);
            return ErrorCode::OutOfBounds;
        }
        descriptor_write.dstBinding = updates[i].binding;
        descriptor_write.dstArrayElement = updates[i].array_element;
        RNDR_FORGE_TRANSLATE(descriptor_type, FromDescriptorType(updates[i].descriptor_type),
                             "the descriptor type of a descriptor set update");
        descriptor_write.descriptorType = descriptor_type;
        descriptor_write.descriptorCount = 1;

        if (updates[i].descriptor_type == DescriptorType::StorageBuffer || updates[i].descriptor_type == DescriptorType::ConstantBuffer)
        {
            const DescriptorSetUpdateBinding::BufferInfo& buffer_info =
                updates[i].resource_info.Get<DescriptorSetUpdateBinding::BufferInfo>();
            const u64 buffer_size = buffer_info.buffer->GetSize();
            if (buffer_info.size == 0)
            {
                RNDR_LOG_ERROR("Forge: the buffer range of a descriptor update is empty");
                return ErrorCode::InvalidArgument;
            }
            // Written so that a large offset cannot overflow the sum and pass, the way Buffer::Update checks it.
            if (buffer_info.offset > buffer_size ||
                (buffer_info.size != k_whole_buffer && buffer_info.size > buffer_size - buffer_info.offset))
            {
                RNDR_LOG_ERROR("Forge: the buffer range of a descriptor update reaches past the end of the buffer");
                return ErrorCode::OutOfBounds;
            }
            buffer_infos[i] = {.buffer = buffer_info.buffer->GetNativeBuffer(),
                               .offset = buffer_info.offset,
                               .range = buffer_info.size == k_whole_buffer ? VK_WHOLE_SIZE : buffer_info.size};
            descriptor_write.pBufferInfo = &buffer_infos[i];
        }
        else
        {
            const DescriptorSetUpdateBinding::TextureInfo& texture_info =
                updates[i].resource_info.Get<DescriptorSetUpdateBinding::TextureInfo>();
            image_infos[i] = {.sampler = texture_info.sampler->GetNativeSampler(),
                              .imageView = texture_info.texture->GetNativeImageView(),
                              .imageLayout = static_cast<VkImageLayout>(texture_info.texture_layout)};
            descriptor_write.pImageInfo = &image_infos[i];
        }
        descriptor_write.pTexelBufferView = nullptr;
    }

    vkUpdateDescriptorSets(m_device, static_cast<u32>(descriptor_writes.GetSize()), descriptor_writes.GetData(), 0, nullptr);
    return ErrorCode::Success;
}

Opal::Expected<const Rndr::Forge::DescriptorSet::BindingInfo&, Rndr::ErrorCode> Rndr::Forge::DescriptorSet::FindBinding(u32 binding) const
{
    using Result = Opal::Expected<const BindingInfo&, ErrorCode>;

    // A linear scan: a layout has a handful of bindings, and a map would cost more to build than this
    // saves to search.
    for (const BindingInfo& binding_info : m_binding_types)
    {
        if (binding_info.binding == binding)
        {
            return Result(binding_info);
        }
    }
    RNDR_LOG_ERROR("Forge: the layout of this descriptor set has no binding {}", binding);
    return Result(ErrorCode::InvalidArgument);
}

Opal::Expected<Rndr::Forge::DescriptorType, Rndr::ErrorCode> Rndr::Forge::DescriptorSet::GetBindingDescriptorType(u32 binding) const
{
    using Result = Opal::Expected<DescriptorType, ErrorCode>;

    const Opal::Expected<const BindingInfo&, ErrorCode> binding_info = FindBinding(binding);
    if (!binding_info.HasValue())
    {
        return Result(binding_info.GetError());
    }
    return Result(binding_info.GetValue().descriptor_type);
}

Opal::Expected<Rndr::u32, Rndr::ErrorCode> Rndr::Forge::DescriptorSet::GetBindingIndex(const Opal::StringUtf8& name) const
{
    using Result = Opal::Expected<u32, ErrorCode>;

    bool any_named = false;
    for (const BindingInfo& binding_info : m_binding_types)
    {
        any_named = any_named || !binding_info.name.IsEmpty();
        if (binding_info.name == name)
        {
            return Result(binding_info.binding);
        }
    }
    if (!any_named)
    {
        RNDR_LOG_ERROR(
            "Forge: this descriptor set carries no binding names - its layout was built without "
            "DescriptorSetLayoutDesc::shaders, which is where the names come from");
        return Result(ErrorCode::InvalidArgument);
    }
    RNDR_LOG_ERROR("Forge: no shader of this descriptor set declares a binding called {}", reinterpret_cast<const char*>(name.GetData()));
    return Result(ErrorCode::InvalidArgument);
}

Rndr::ErrorCode Rndr::Forge::DescriptorSet::Update(const Opal::StringUtf8& name, const Texture& texture, const Sampler& sampler,
                                                   ImageLayout texture_layout, u32 array_element)
{
    const Opal::Expected<u32, ErrorCode> binding = GetBindingIndex(name);
    if (!binding.HasValue())
    {
        return binding.GetError();
    }
    return Update(binding.GetValue(), texture, sampler, texture_layout, array_element);
}

Rndr::ErrorCode Rndr::Forge::DescriptorSet::Update(const Opal::StringUtf8& name, const Buffer& buffer, u64 offset, u64 size,
                                                   u32 array_element)
{
    const Opal::Expected<u32, ErrorCode> binding = GetBindingIndex(name);
    if (!binding.HasValue())
    {
        return binding.GetError();
    }
    return Update(binding.GetValue(), buffer, offset, size, array_element);
}

Rndr::ErrorCode Rndr::Forge::DescriptorSet::Update(u32 binding, const Texture& texture, const Sampler& sampler, ImageLayout texture_layout,
                                                   u32 array_element)
{
    const Opal::Expected<DescriptorType, ErrorCode> descriptor_type = GetBindingDescriptorType(binding);
    if (!descriptor_type.HasValue())
    {
        return descriptor_type.GetError();
    }
    const DescriptorSetUpdateBinding update{
        .descriptor_type = descriptor_type.GetValue(),
        .binding = binding,
        .array_element = array_element,
        .resource_info = DescriptorSetUpdateBinding::TextureInfo{.sampler = sampler, .texture = texture, .texture_layout = texture_layout}};
    return Update({&update, 1});
}

Rndr::ErrorCode Rndr::Forge::DescriptorSet::Update(u32 binding, const Buffer& buffer, u64 offset, u64 size, u32 array_element)
{
    const Opal::Expected<DescriptorType, ErrorCode> descriptor_type = GetBindingDescriptorType(binding);
    if (!descriptor_type.HasValue())
    {
        return descriptor_type.GetError();
    }
    const DescriptorSetUpdateBinding update{
        .descriptor_type = descriptor_type.GetValue(),
        .binding = binding,
        .array_element = array_element,
        .resource_info = DescriptorSetUpdateBinding::BufferInfo{.buffer = buffer, .offset = offset, .size = size}};
    return Update({&update, 1});
}
