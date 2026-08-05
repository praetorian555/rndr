#include "rndr/forge/descriptor-set.hpp"

#include "opal/container/in-place-array.h"
#include "rndr/forge/buffer.hpp"
#include "rndr/forge/texture.hpp"

#include "rndr/forge/device.hpp"
#include "rndr/forge/vulkan-exception.hpp"

namespace
{
VkDescriptorType FromDescriptorType(Rndr::Forge::DescriptorType descriptor_type)
{
    switch (descriptor_type)
    {
        case Rndr::Forge::DescriptorType::SampledImage:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case Rndr::Forge::DescriptorType::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case Rndr::Forge::DescriptorType::CombinedImageSampler:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case Rndr::Forge::DescriptorType::ConstantBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case Rndr::Forge::DescriptorType::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case Rndr::Forge::DescriptorType::StorageImage:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        default:
            throw Opal::Exception("Invalid descriptor type!");
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

void Rndr::Forge::DescriptorPoolDesc::Add(DescriptorType descriptor_type, u32 max_size)
{
    for (const auto& pair : descriptor_types)
    {
        if (pair.key == descriptor_type)
        {
            throw Opal::Exception("Descriptor type already provided!");
        }
    }
    descriptor_types.PushBack({.key = descriptor_type, .value = max_size});
}

void Rndr::Forge::DescriptorSetLayoutDesc::AddBinding(u32 binding, DescriptorType descriptor_type, u32 descriptor_count,
                                                      ShaderTypeBits shader_types,
                                                      Opal::ArrayView<const Opal::Ref<const Sampler>> immutable_samplers)
{
    for (const Binding& existing : bindings)
    {
        if (existing.binding == binding)
        {
            throw Opal::Exception("Binding index already used by this layout!");
        }
    }
    if (!immutable_samplers.IsEmpty())
    {
        if (descriptor_type != DescriptorType::Sampler && descriptor_type != DescriptorType::CombinedImageSampler)
        {
            throw Opal::Exception("Immutable samplers are only valid on sampler descriptors!");
        }
        if (immutable_samplers.GetSize() != static_cast<u64>(descriptor_count))
        {
            throw Opal::Exception("Immutable sampler count must match the descriptor count!");
        }
    }

    Binding new_binding;
    new_binding.binding = binding;
    new_binding.descriptor_type = descriptor_type;
    new_binding.descriptor_count = descriptor_count;
    new_binding.shader_types = shader_types;
    for (const Opal::Ref<const Sampler>& sampler : immutable_samplers)
    {
        if (!sampler.IsValid())
        {
            throw Opal::Exception("Immutable sampler is null!");
        }
        new_binding.immutable_samplers.PushBack(sampler.Clone());
    }
    bindings.PushBack(std::move(new_binding));
}

// DescriptorPool

Rndr::Forge::DescriptorSetUpdateBinding Rndr::Forge::DescriptorSetUpdateBinding::Clone(Opal::AllocatorBase* allocator) const
{
    DescriptorSetUpdateBinding clone{
        .descriptor_type = descriptor_type, .binding = binding, .resource_info = resource_info.Clone(allocator)};
    return clone;
}

Rndr::Forge::DescriptorPool::DescriptorPool(const Device& device, const DescriptorPoolDesc& desc)
    : m_device(device), m_desc(desc.Clone())
{
    Opal::DynamicArray<VkDescriptorPoolSize> pool_sizes;
    for (const auto& pair : desc.descriptor_types)
    {
        const VkDescriptorPoolSize pool_size{
            .type = FromDescriptorType(pair.key),
            .descriptorCount = pair.value,
        };
        pool_sizes.PushBack(pool_size);
    }

    if (pool_sizes.IsEmpty())
    {
        throw Opal::Exception("Can't create pool with no descriptors!");
    }

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

    const VkResult result = vkCreateDescriptorPool(device.GetNativeDevice(), &pool_info, nullptr, &m_pool);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateDescriptorPool");
    }
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

void Rndr::Forge::DescriptorPool::Reset()
{
    if (m_pool == VK_NULL_HANDLE)
    {
        return;
    }
    const VkResult result = vkResetDescriptorPool(m_device->GetNativeDevice(), m_pool, 0);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkResetDescriptorPool");
    }
}

VkDevice Rndr::Forge::DescriptorPool::GetNativeDevice() const
{
    return m_device->GetNativeDevice();
}

// DescriptorSetLayout

Rndr::Forge::DescriptorSetLayout::DescriptorSetLayout(const Device& device, const DescriptorSetLayoutDesc& desc)
    : m_device(device), m_desc(desc.Clone())
{
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

    for (i32 i = 0; i < bindings.GetSize(); i++)
    {
        const DescriptorSetLayoutDesc::Binding& source = desc.bindings[i];
        VkDescriptorSetLayoutBinding& binding = bindings[i];
        binding.binding = source.binding;
        binding.descriptorType = FromDescriptorType(source.descriptor_type);
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
    }

    const Opal::InPlaceArray<VkDescriptorBindingFlags, 3> binding_flags = {VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                                           VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                                                                           VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info{};
    binding_flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags_info.bindingCount = static_cast<u32>(binding_flags_array.GetSize());
    binding_flags_info.pBindingFlags = binding_flags_array.GetData();

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<u32>(bindings.GetSize());
    layout_info.pBindings = bindings.GetData();
    layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layout_info.pNext = &binding_flags_info;

    const VkResult result = vkCreateDescriptorSetLayout(device.GetNativeDevice(), &layout_info, nullptr, &m_layout);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateDescriptorSetLayout");
    }
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

Rndr::Forge::DescriptorSet::DescriptorSet(const DescriptorPool& pool, const DescriptorSetLayout& layout,
                                                   u32 variable_descriptor_count)
    : m_device(pool.GetNativeDevice()), m_pool(pool)
{
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

    const VkResult result = vkAllocateDescriptorSets(m_device, &alloc_info, &m_set);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkAllocateDescriptorSets");
    }
}

Rndr::Forge::DescriptorSet::~DescriptorSet()
{
    Destroy();
}

Rndr::Forge::DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept
    : m_device(other.m_device), m_set(other.m_set), m_pool(std::move(other.m_pool))
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
}

void Rndr::Forge::DescriptorSet::UpdateDescriptorSets(Opal::ArrayView<const DescriptorSetUpdateBinding> updates)
{
    Opal::DynamicArray<VkWriteDescriptorSet> descriptor_writes(updates.GetSize());
    Opal::DynamicArray<VkDescriptorBufferInfo> buffer_infos(updates.GetSize());
    Opal::DynamicArray<VkDescriptorImageInfo> image_infos(updates.GetSize());
    for (i32 i = 0; i < updates.GetSize(); i++)
    {
        VkWriteDescriptorSet& descriptor_write = descriptor_writes[i];
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.pNext = nullptr;
        descriptor_write.dstSet = m_set;
        descriptor_write.dstBinding = updates[i].binding;
        descriptor_write.descriptorType = FromDescriptorType(updates[i].descriptor_type);
        descriptor_write.descriptorCount = 1;

        if (updates[i].descriptor_type == DescriptorType::StorageBuffer ||
            updates[i].descriptor_type == DescriptorType::ConstantBuffer)
        {
            const DescriptorSetUpdateBinding::BufferInfo& buffer_info =
                updates[i].resource_info.Get<DescriptorSetUpdateBinding::BufferInfo>();
            buffer_infos.PushBack({.buffer = buffer_info.buffer->GetNativeBuffer(), .offset = 0, .range = buffer_info.buffer->GetSize()});
            descriptor_write.pBufferInfo = &buffer_infos.Back();
        }
        else
        {
            const DescriptorSetUpdateBinding::ImageInfo& image_info =
                updates[i].resource_info.Get<DescriptorSetUpdateBinding::ImageInfo>();
            image_infos.PushBack({.sampler = image_info.sampler->GetNativeSampler(),
                                  .imageView = image_info.image->GetNativeImageView(),
                                  .imageLayout = static_cast<VkImageLayout>(image_info.image_layout)});
            descriptor_write.pImageInfo = &image_infos.Back();
        }
        descriptor_write.pTexelBufferView = nullptr;
    }

    vkUpdateDescriptorSets(m_device, static_cast<u32>(descriptor_writes.GetSize()), descriptor_writes.GetData(), 0, nullptr);
}
