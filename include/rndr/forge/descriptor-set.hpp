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
#include "rndr/forge/types.hpp"

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
    /**
     * Allow individual sets to be returned to the pool when they are destroyed. Off by default, since a pool that is
     * reset or destroyed as a whole is both cheaper and the common case.
     */
    bool free_individual_sets = false;

    // DescriptorPoolDesc(Opal::DynamicArray<Opal::Pair<DescriptorType, u32>> in_descriptor_types, u32 in_max_sets,
    //                            bool in_use_update_after_bind)
    //     : descriptor_types(std::move(in_descriptor_types)), max_sets(in_max_sets), use_update_after_bind(in_use_update_after_bind)
    // {
    // }
    //
    OPAL_CLONE_FIELDS(descriptor_types, max_sets, use_update_after_bind, free_individual_sets);

    void Add(DescriptorType descriptor_type, u32 max_size);
};

struct DescriptorSetLayoutDesc : Opal::ClonableBase<DescriptorSetLayoutDesc>
{
    struct Binding
    {
        DescriptorType descriptor_type = DescriptorType::CombinedImageSampler;
        u32 descriptor_count = 1;
        ShaderTypeBits shader_types = ShaderTypeBits::AllGraphics;
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
        ImageLayout image_layout = ImageLayout::ShaderReadOnly;
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

    /**
     * Return every set allocated from this pool to it in one call, without touching the pool itself. Every
     * DescriptorSet that came out of this pool is invalid afterwards and must not be destroyed or bound, which makes
     * this the recycle-per-frame counterpart to allocating and freeing sets one by one.
     */
    void Reset();

    [[nodiscard]] bool IsValid() const { return m_pool != VK_NULL_HANDLE; }
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

    [[nodiscard]] bool IsValid() const { return m_layout != VK_NULL_HANDLE; }
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

    /**
     * Return the set to the pool it was allocated from, when that pool was created with
     * DescriptorPoolDesc::free_individual_sets. Otherwise only the handle is dropped and the memory stays with the
     * pool until it is reset or destroyed.
     */
    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_set != VK_NULL_HANDLE; }
    [[nodiscard]] VkDescriptorSet GetNativeDescriptorSet() const { return m_set; }

    void UpdateDescriptorSets(Opal::ArrayView<const DescriptorSetUpdateBinding> updates);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
    Opal::Ref<const DescriptorPool> m_pool;
};

}  // namespace Rndr::Forge
