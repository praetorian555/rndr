#pragma once

#include "volk/volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/hash-map.h"
#include "opal/container/ref.h"

#include "rndr/graphics-types.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

enum class AdvancedDescriptorType : u8
{
    SampledImage = 0,
    Sampler,
    CombinedImageSampler,
    ConstantBuffer,
    StorageBuffer,
    StorageImage,

    EnumCount
};

struct AdvancedDescriptorPoolDesc
{
    Opal::DynamicArray<Opal::Pair<AdvancedDescriptorType, u32>> descriptor_types;
    u32 max_sets = 1;
    bool use_update_after_bind = true;

    void Add(AdvancedDescriptorType descriptor_type, u32 max_size);
};

struct AdvancedDescriptorSetLayoutDesc
{
    struct Binding
    {
        AdvancedDescriptorType descriptor_type;
        u32 descriptor_count;
        ShaderTypeBits shader_types;
    };
    Opal::DynamicArray<Binding> bindings;

    void AddBinding(AdvancedDescriptorType descriptor_type, u32 descriptor_count, ShaderTypeBits shader_types);
};

struct AdvancedDescriptorSetUpdateBinding
{
    AdvancedDescriptorType descriptor_type = AdvancedDescriptorType::CombinedImageSampler;
    u32 binding = 0;
    union
    {
        struct BufferInfo
        {
            Opal::Ref<class AdvancedBuffer> buffer;
            u64 offset = 0;
            u64 size = 0;
        } buffer_info;
        struct ImageInfo
        {
            Opal::Ref<class AdvancedSampler> sampler;
            Opal::Ref<class AdvancedTexture> image;
            ImageLayout image_layout;
        } image_info;
    };
};

class AdvancedDescriptorPool
{
public:
    AdvancedDescriptorPool() = default;
    explicit AdvancedDescriptorPool(const class AdvancedDevice& device, const AdvancedDescriptorPoolDesc& desc = {});
    ~AdvancedDescriptorPool();

    AdvancedDescriptorPool(const AdvancedDescriptorPool&) = delete;
    AdvancedDescriptorPool& operator=(const AdvancedDescriptorPool&) = delete;
    AdvancedDescriptorPool(AdvancedDescriptorPool&& other) noexcept;
    AdvancedDescriptorPool& operator=(AdvancedDescriptorPool&& other) noexcept;

    void Destroy();

    [[nodiscard]] VkDescriptorPool GetNativeDescriptorPool() const { return m_pool; }
    [[nodiscard]] const AdvancedDescriptorPoolDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] VkDevice GetNativeDevice() const;

private:
    Opal::Ref<const class AdvancedDevice> m_device;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    AdvancedDescriptorPoolDesc m_desc;
};

class AdvancedDescriptorSetLayout
{
public:
    AdvancedDescriptorSetLayout() = default;
    explicit AdvancedDescriptorSetLayout(const class AdvancedDevice& device, const AdvancedDescriptorSetLayoutDesc& desc);
    ~AdvancedDescriptorSetLayout();

    AdvancedDescriptorSetLayout(const AdvancedDescriptorSetLayout&) = delete;
    AdvancedDescriptorSetLayout& operator=(const AdvancedDescriptorSetLayout&) = delete;
    AdvancedDescriptorSetLayout(AdvancedDescriptorSetLayout&& other) noexcept;
    AdvancedDescriptorSetLayout& operator=(AdvancedDescriptorSetLayout&& other) noexcept;

    void Destroy();

    [[nodiscard]] VkDescriptorSetLayout GetNativeDescriptorSetLayout() const { return m_layout; }
    [[nodiscard]] const AdvancedDescriptorSetLayoutDesc& GetDesc() const { return m_desc; }

private:
    Opal::Ref<const class AdvancedDevice> m_device;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    AdvancedDescriptorSetLayoutDesc m_desc;
};

class AdvancedDescriptorSet
{
public:
    AdvancedDescriptorSet() = default;
    explicit AdvancedDescriptorSet(const AdvancedDescriptorPool& pool, const AdvancedDescriptorSetLayout& layout,
                                   u32 variable_descriptor_count = 0);
    ~AdvancedDescriptorSet();

    AdvancedDescriptorSet(const AdvancedDescriptorSet&) = delete;
    AdvancedDescriptorSet& operator=(const AdvancedDescriptorSet&) = delete;
    AdvancedDescriptorSet(AdvancedDescriptorSet&& other) noexcept;
    AdvancedDescriptorSet& operator=(AdvancedDescriptorSet&& other) noexcept;

    void Destroy();

    [[nodiscard]] VkDescriptorSet GetNativeDescriptorSet() const { return m_set; }

    void UpdateDescriptorSets(const Opal::DynamicArray<AdvancedDescriptorSetUpdateBinding>& updates);

private:
    VkDevice m_device;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
};

}  // namespace Rndr
