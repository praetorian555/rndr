#include "rndr/advanced/advanced-descriptor-set.hpp"

#include "opal/container/in-place-array.h"
#include "rndr/advanced/advanced-buffer.hpp"
#include "rndr/advanced/advanced-texture.hpp"

#include "rndr/advanced/device.hpp"

namespace
{
VkDescriptorType FromAdvancedDescriptorType(Rndr::AdvancedDescriptorType descriptor_type)
{
    switch (descriptor_type)
    {
        case Rndr::AdvancedDescriptorType::SampledImage:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case Rndr::AdvancedDescriptorType::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case Rndr::AdvancedDescriptorType::CombinedImageSampler:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case Rndr::AdvancedDescriptorType::ConstantBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case Rndr::AdvancedDescriptorType::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case Rndr::AdvancedDescriptorType::StorageImage:
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

void Rndr::AdvancedDescriptorPoolDesc::Add(AdvancedDescriptorType descriptor_type, u32 max_size)
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

void Rndr::AdvancedDescriptorSetLayoutDesc::AddBinding(AdvancedDescriptorType descriptor_type, u32 descriptor_count,
                                                       ShaderTypeBits shader_types)
{
    Binding binding;
    binding.descriptor_type = descriptor_type;
    binding.descriptor_count = descriptor_count;
    binding.shader_types = shader_types;
    bindings.PushBack(binding);
}

// AdvancedDescriptorPool

Rndr::AdvancedDescriptorPool::AdvancedDescriptorPool(const AdvancedDevice& device, const AdvancedDescriptorPoolDesc& desc)
    : m_device(device), m_desc(desc)
{
    Opal::DynamicArray<VkDescriptorPoolSize> pool_sizes(Opal::GetScratchAllocator());
    for (const auto& pair : desc.descriptor_types)
    {
        const VkDescriptorPoolSize pool_size{
            .type = FromAdvancedDescriptorType(pair.key),
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
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    }

    const VkResult result = vkCreateDescriptorPool(device.GetNativeDevice(), &pool_info, nullptr, &m_pool);
    if (result != VK_SUCCESS)
    {
        throw Opal::Exception("Failed to create descriptor pool!");
    }
}

Rndr::AdvancedDescriptorPool::~AdvancedDescriptorPool()
{
    Destroy();
}

Rndr::AdvancedDescriptorPool::AdvancedDescriptorPool(AdvancedDescriptorPool&& other) noexcept
    : m_device(std::move(other.m_device)), m_pool(other.m_pool), m_desc(other.m_desc)
{
    other.m_pool = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::AdvancedDescriptorPool& Rndr::AdvancedDescriptorPool::operator=(AdvancedDescriptorPool&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_pool = other.m_pool;
        m_desc = other.m_desc;
        other.m_pool = VK_NULL_HANDLE;
        other.m_device = nullptr;
    }
    return *this;
}

void Rndr::AdvancedDescriptorPool::Destroy()
{
    if (m_pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device->GetNativeDevice(), m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
}

VkDevice Rndr::AdvancedDescriptorPool::GetNativeDevice() const
{
    return m_device->GetNativeDevice();
}

// AdvancedDescriptorSetLayout

Rndr::AdvancedDescriptorSetLayout::AdvancedDescriptorSetLayout(const AdvancedDevice& device, const AdvancedDescriptorSetLayoutDesc& desc)
    : m_device(device), m_desc(desc)
{
    Opal::DynamicArray<VkDescriptorSetLayoutBinding> bindings(desc.bindings.GetSize());
    Opal::DynamicArray<VkDescriptorBindingFlags> binding_flags_array(desc.bindings.GetSize());

    for (i32 i = 0; i < bindings.GetSize(); i++)
    {
        VkDescriptorSetLayoutBinding& binding = bindings[i];
        binding.binding = i;
        binding.descriptorType = FromAdvancedDescriptorType(desc.bindings[i].descriptor_type);
        binding.descriptorCount = desc.bindings[i].descriptor_count;
        binding.stageFlags = FromShaderTypeBits(desc.bindings[i].shader_types);
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
        throw Opal::Exception("Failed to create descriptor set layout!");
    }
}

Rndr::AdvancedDescriptorSetLayout::~AdvancedDescriptorSetLayout()
{
    Destroy();
}

Rndr::AdvancedDescriptorSetLayout::AdvancedDescriptorSetLayout(AdvancedDescriptorSetLayout&& other) noexcept
    : m_device(std::move(other.m_device)), m_layout(other.m_layout), m_desc(std::move(other.m_desc))
{
    other.m_layout = VK_NULL_HANDLE;
    other.m_device = nullptr;
}

Rndr::AdvancedDescriptorSetLayout& Rndr::AdvancedDescriptorSetLayout::operator=(AdvancedDescriptorSetLayout&& other) noexcept
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

void Rndr::AdvancedDescriptorSetLayout::Destroy()
{
    if (m_layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device->GetNativeDevice(), m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
}

// AdvancedDescriptorSet

Rndr::AdvancedDescriptorSet::AdvancedDescriptorSet(const AdvancedDescriptorPool& pool, const AdvancedDescriptorSetLayout& layout,
                                                   u32 variable_descriptor_count)
    : m_device(pool.GetNativeDevice())
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
        throw Opal::Exception("Failed to allocate descriptor set!");
    }
}

Rndr::AdvancedDescriptorSet::~AdvancedDescriptorSet()
{
    Destroy();
}

Rndr::AdvancedDescriptorSet::AdvancedDescriptorSet(AdvancedDescriptorSet&& other) noexcept : m_device(other.m_device), m_set(other.m_set)
{
    other.m_set = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
}

Rndr::AdvancedDescriptorSet& Rndr::AdvancedDescriptorSet::operator=(AdvancedDescriptorSet&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = other.m_device;
        m_set = other.m_set;
        other.m_set = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }
    return *this;
}

void Rndr::AdvancedDescriptorSet::Destroy()
{
    m_set = VK_NULL_HANDLE;
}

void Rndr::AdvancedDescriptorSet::UpdateDescriptorSets(const Opal::DynamicArray<AdvancedDescriptorSetUpdateBinding>& updates)
{
    Opal::DynamicArray<VkWriteDescriptorSet> descriptor_writes(updates.GetSize(), Opal::GetScratchAllocator());
    Opal::DynamicArray<VkDescriptorBufferInfo> buffer_infos(Opal::GetScratchAllocator());
    Opal::DynamicArray<VkDescriptorImageInfo> image_infos(Opal::GetScratchAllocator());
    for (i32 i = 0; i < updates.GetSize(); i++)
    {
        VkWriteDescriptorSet& descriptor_write = descriptor_writes[i];
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.pNext = nullptr;
        descriptor_write.dstSet = m_set;
        descriptor_write.dstBinding = updates[i].binding;
        descriptor_write.descriptorType = FromAdvancedDescriptorType(updates[i].descriptor_type);
        descriptor_write.descriptorCount = 1;

        if (updates[i].descriptor_type == AdvancedDescriptorType::StorageBuffer ||
            updates[i].descriptor_type == AdvancedDescriptorType::ConstantBuffer)
        {
            buffer_infos.PushBack({.buffer = updates[i].buffer_info.buffer->GetNativeBuffer(),
                                   .offset = 0,
                                   .range = updates[i].buffer_info.buffer->GetSize()});
            descriptor_write.pBufferInfo = &buffer_infos.Back();
        }
        else
        {
            image_infos.PushBack({.sampler = updates[i].image_info.sampler->GetNativeSampler(),
                                  .imageView = updates[i].image_info.image->GetNativeImageView(),
                                  .imageLayout = static_cast<VkImageLayout>(updates[i].image_info.image_layout)});
            descriptor_write.pImageInfo = &image_infos.Back();
        }
        descriptor_write.pTexelBufferView = nullptr;
    }

    vkUpdateDescriptorSets(m_device, static_cast<u32>(descriptor_writes.GetSize()), descriptor_writes.GetData(), 0,
                           nullptr);
}
