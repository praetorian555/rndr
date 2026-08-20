#pragma once

#include "volk/volk.h"

#include "opal/clonable-base.h"
#include "opal/enum-flags.h"
#include "opal/container/array-view.h"
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

/**
 * What a binding is allowed beyond the plain case, where every descriptor is written once before the set is
 * bound and none of them changes afterwards. Mirrors VkDescriptorBindingFlagBits.
 *
 * Each of these needs the matching DeviceFeatures field, which the layout checks rather than leaving it to
 * the validation layer.
 */
enum class DescriptorBindingFlagBits : u8
{
    None = 0,
    /** Descriptors may be written while the set is bound, even while the device is using it. */
    UpdateAfterBind = 1,
    /** Descriptors may be written while the set is bound to a command buffer that has not been submitted. */
    UpdateUnusedWhilePending = 2,
    /** Not every descriptor has to be written. The shader is only allowed to read the ones that were. */
    PartiallyBound = 4,
    /**
     * The length of this binding is chosen when a set is allocated rather than when the layout is created,
     * with descriptor_count as the upper bound. Only the last binding of a layout may have it.
     */
    VariableDescriptorCount = 8
};
OPAL_ENUM_CLASS_FLAGS(DescriptorBindingFlagBits);

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
    struct Binding : Opal::ClonableBase<Binding>
    {
        /** The index the shader declares this binding at. Indices may skip values and arrive in any order. */
        u32 binding = 0;
        /**
         * What the shader calls this binding, filled in from reflection when the layout was given `shaders`
         * and empty otherwise. Not something to write by hand - a name typed here that the shader does not
         * use would be the very mistake keying by name is meant to remove. DescriptorSet::Update takes it.
         */
        Opal::StringUtf8 name;
        DescriptorType descriptor_type = DescriptorType::CombinedImageSampler;
        u32 descriptor_count = 1;
        ShaderTypeBits shader_types = ShaderTypeBits::AllGraphics;
        /** What this binding is allowed beyond writing every descriptor once before the set is bound. */
        DescriptorBindingFlagBits flags = DescriptorBindingFlagBits::None;
        /**
         * Samplers baked into the layout, one per descriptor, for DescriptorType::Sampler and
         * DescriptorType::CombinedImageSampler only. The sampler of such a binding cannot be written by
         * DescriptorSet::Update, and every Sampler listed here has to outlive the layout and every set allocated
         * from it.
         */
        Opal::DynamicArray<Opal::Ref<const Sampler>> immutable_samplers;
        OPAL_CLONE_FIELDS(binding, name, descriptor_type, descriptor_count, shader_types, flags, immutable_samplers);
    };
    Opal::DynamicArray<Binding> bindings;

    /**
     * The shaders this layout is meant to match. Optional: left empty, the layout is built exactly as it is
     * written, which is what it did before there was anything to check it against.
     *
     * Given them, every binding is checked against what those shaders declare in `set_index` - the kind, the
     * count, and the stages that read it - and each one is given the name the shader uses, which is what
     * lets DescriptorSet::Update take a name. A binding either side declares and the other does not throws.
     * Only the shaders themselves are read, so they need not outlive the layout.
     */
    Opal::DynamicArray<Opal::Ref<const Shader>> shaders;

    /** Which descriptor set of those shaders this layout is. Only read when `shaders` is not empty. */
    u32 set_index = 0;

    OPAL_CLONE_FIELDS(bindings, shaders, set_index);

    void AddBinding(u32 binding, DescriptorType descriptor_type, u32 descriptor_count, ShaderTypeBits shader_types,
                    Opal::ArrayView<const Opal::Ref<const Sampler>> immutable_samplers = {},
                    DescriptorBindingFlagBits flags = DescriptorBindingFlagBits::None);
};

struct DescriptorSetUpdateBinding
{
    struct BufferInfo : Opal::ClonableBase<BufferInfo>
    {
        Opal::Ref<const Buffer> buffer;
        u64 offset = 0;
        /** Bytes visible to the shader, starting at offset. k_whole_buffer covers the rest of the buffer. */
        u64 size = k_whole_buffer;
        OPAL_CLONE_FIELDS(buffer, offset, size);
    };
    struct ImageInfo : Opal::ClonableBase<ImageInfo>
    {
        Opal::Ref<const Sampler> sampler;
        Opal::Ref<const Texture> image;
        ImageLayout image_layout = ImageLayout::ShaderReadOnly;
        OPAL_CLONE_FIELDS(sampler, image, image_layout);
    };

    DescriptorType descriptor_type = DescriptorType::CombinedImageSampler;
    u32 binding = 0;
    /** Which descriptor of the binding to write, for a binding that is an array of them. */
    u32 array_element = 0;
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

    /**
     * Write several descriptors as one call, which is what a set with more than one binding to fill wants -
     * the single-resource overloads below are one vkUpdateDescriptorSets each.
     * @param updates What to write. Each names its own binding and descriptor type.
     */
    void Update(Opal::ArrayView<const DescriptorSetUpdateBinding> updates);

    /**
     * Write one image into a binding. The descriptor type comes from the layout the set was allocated from,
     * so it is not something a call site can disagree with the shader about.
     * @param binding Index the layout declares. A binding the layout does not have throws.
     * @param texture Image to write. Its view is what the shader reads.
     * @param sampler Sampler to write beside it. Ignored by Vulkan when the binding has immutable samplers.
     * @param image_layout Layout the image will be in when the shader reads it.
     * @param array_element Which descriptor of the binding to write, for a binding that is an array.
     */
    void Update(u32 binding, const Texture& texture, const Sampler& sampler,
                ImageLayout image_layout = ImageLayout::ShaderReadOnly, u32 array_element = 0);

    /**
     * Write one buffer into a binding. The descriptor type - constant or storage - comes from the layout, as
     * above.
     * @param binding Index the layout declares. A binding the layout does not have throws.
     * @param buffer Buffer to write.
     * @param offset Byte offset the shader sees as the start of the range.
     * @param size Bytes visible from offset on. k_whole_buffer is the rest of the buffer.
     * @param array_element Which descriptor of the binding to write, for a binding that is an array.
     */
    void Update(u32 binding, const Buffer& buffer, u64 offset = 0, u64 size = k_whole_buffer, u32 array_element = 0);

    /**
     * Write one image into the binding the shader calls `name`, which is the same call as above with the
     * index looked up rather than typed. Reading as Update("albedo_texture", ...) is the point: a binding
     * index is a number that can be wrong in a way nothing reads back, and a name cannot.
     * @param name What the shader calls the binding. A name it does not use throws, as does any name at all
     *             when the layout was built without `shaders` and so carries none.
     */
    void Update(const Opal::StringUtf8& name, const Texture& texture, const Sampler& sampler,
                ImageLayout image_layout = ImageLayout::ShaderReadOnly, u32 array_element = 0);

    /** Write one buffer into the binding the shader calls `name`, as above. */
    void Update(const Opal::StringUtf8& name, const Buffer& buffer, u64 offset = 0, u64 size = k_whole_buffer,
                u32 array_element = 0);

    /**
     * The index of the binding the shader calls `name`, which is what the two overloads above look up.
     * Throws on a name no binding carries, and says so plainly when the layout carries no names at all.
     */
    [[nodiscard]] u32 GetBindingIndex(const Opal::StringUtf8& name) const;

    /**
     * Descriptor type the layout declared for a binding. Throws when the layout has no such binding, which is
     * what the Update overloads above call to fill in the type they do not take.
     */
    [[nodiscard]] DescriptorType GetBindingDescriptorType(u32 binding) const;

private:
    /** What one binding of the layout holds, which is all the Update overloads need to know about it. */
    struct BindingInfo
    {
        u32 binding = 0;
        DescriptorType descriptor_type = DescriptorType::CombinedImageSampler;
        /** What the shader calls it, when the layout was built with the shaders that declare it. */
        Opal::StringUtf8 name;
    };

    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
    Opal::Ref<const DescriptorPool> m_pool;
    /**
     * What each binding of the layout holds. Copied rather than referenced: Vulkan lets a layout be destroyed
     * while sets allocated from it live on, and holding a reference would quietly take that away.
     */
    Opal::DynamicArray<BindingInfo> m_binding_types;
};

}  // namespace Rndr::Forge
