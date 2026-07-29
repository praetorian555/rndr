#pragma once

#include "volk/volk.h"

#include "opal/clonable-base.h"
#include "opal/container/dynamic-array.h"
#include "opal/container/hash-map.h"
#include "opal/container/ref.h"
#include "opal/variant.h"

#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"
#include "rndr/forge/forward.hpp"

namespace Rndr::Forge
{

enum class DescriptorType : u8
{
    SampledImage = 0,
    Sampler,
    CombinedImageSampler,
    ConstantBuffer,
    StorageBuffer,
    StorageImage,

    EnumCount
};

struct DescriptorPoolDesc : Opal::ClonableBase<DescriptorPoolDesc>
{
    Opal::DynamicArray<Opal::Pair<DescriptorType, u32>> descriptor_types;
    u32 max_sets = 1;
    bool use_update_after_bind = true;

    // DescriptorPoolDesc(Opal::DynamicArray<Opal::Pair<DescriptorType, u32>> in_descriptor_types, u32 in_max_sets,
    //                            bool in_use_update_after_bind)
    //     : descriptor_types(std::move(in_descriptor_types)), max_sets(in_max_sets), use_update_after_bind(in_use_update_after_bind)
    // {
    // }
    //
    OPAL_CLONE_FIELDS(descriptor_types, max_sets, use_update_after_bind);

    void Add(DescriptorType descriptor_type, u32 max_size);
};

struct DescriptorSetLayoutDesc : Opal::ClonableBase<DescriptorSetLayoutDesc>
{
    struct Binding
    {
        DescriptorType descriptor_type;
        u32 descriptor_count;
        ShaderTypeBits shader_types;
    };
    Opal::DynamicArray<Binding> bindings;

    OPAL_CLONE_FIELDS(bindings);

    void AddBinding(DescriptorType descriptor_type, u32 descriptor_count, ShaderTypeBits shader_types);
};

struct DescriptorSetUpdateBinding
{
    struct BufferInfo : Opal::ClonableBase<BufferInfo>
    {
        Opal::Ref<Buffer> buffer;
        u64 offset = 0;
        u64 size = 0;
        OPAL_CLONE_FIELDS(buffer, offset, size);
    };
    struct ImageInfo : Opal::ClonableBase<ImageInfo>
    {
        Opal::Ref<Sampler> sampler;
        Opal::Ref<Texture> image;
        ImageLayout image_layout;
        OPAL_CLONE_FIELDS(sampler, image, image_layout);
    };

    DescriptorType descriptor_type = DescriptorType::CombinedImageSampler;
    u32 binding = 0;
    Opal::Variant<BufferInfo, ImageInfo> resource_info;

    DescriptorSetUpdateBinding Clone(Opal::AllocatorBase* allocator = nullptr) const;
};

class DescriptorPool
{
public:
    DescriptorPool() = default;
    explicit DescriptorPool(const Device& device, const DescriptorPoolDesc& desc = {});
    ~DescriptorPool();

    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;
    DescriptorPool(DescriptorPool&& other) noexcept;
    DescriptorPool& operator=(DescriptorPool&& other) noexcept;

    void Destroy();

    [[nodiscard]] VkDescriptorPool GetNativeDescriptorPool() const { return m_pool; }
    [[nodiscard]] const DescriptorPoolDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] VkDevice GetNativeDevice() const;

private:
    Opal::Ref<const Device> m_device;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    DescriptorPoolDesc m_desc;
};

class DescriptorSetLayout
{
public:
    DescriptorSetLayout() = default;
    explicit DescriptorSetLayout(const Device& device, const DescriptorSetLayoutDesc& desc = {});
    ~DescriptorSetLayout();

    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept;
    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept;

    void Destroy();

    [[nodiscard]] VkDescriptorSetLayout GetNativeDescriptorSetLayout() const { return m_layout; }
    [[nodiscard]] const DescriptorSetLayoutDesc& GetDesc() const { return m_desc; }

private:
    Opal::Ref<const Device> m_device;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    DescriptorSetLayoutDesc m_desc;
};

class DescriptorSet
{
public:
    DescriptorSet() = default;
    explicit DescriptorSet(const DescriptorPool& pool, const DescriptorSetLayout& layout,
                                   u32 variable_descriptor_count = 0);
    ~DescriptorSet();

    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;
    DescriptorSet(DescriptorSet&& other) noexcept;
    DescriptorSet& operator=(DescriptorSet&& other) noexcept;

    void Destroy();

    [[nodiscard]] VkDescriptorSet GetNativeDescriptorSet() const { return m_set; }

    void UpdateDescriptorSets(const Opal::DynamicArray<DescriptorSetUpdateBinding>& updates);

private:
    VkDevice m_device;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
};

}  // namespace Rndr::Forge
