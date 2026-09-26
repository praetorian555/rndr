#pragma once

#include <mutex>

#include "volk.h"

#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"

#include "rndr/error-codes.hpp"
#include "rndr/types.hpp"

namespace Rndr::Forge
{

/**
 * One attachment of a render pass as the cache tells passes apart: what it is, and what the pass does with it. The
 * layout is the one the attachment is in before, during and after the pass, since Forge moves layouts with barriers
 * and never through a render pass.
 */
struct RenderPassAttachment
{
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkAttachmentLoadOp stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    bool operator==(const RenderPassAttachment& other) const = default;
};

/**
 * Everything that makes two render passes different objects: the attachments of the pass's one subpass, what it
 * resolves into, and its view mask. A resolve target is a single sampled attachment of the same format, so its
 * layout is all it adds; VK_IMAGE_LAYOUT_UNDEFINED is no resolve.
 */
struct RenderPassKey
{
    Opal::DynamicArray<RenderPassAttachment> color_attachments;
    Opal::DynamicArray<VkImageLayout> color_resolve_layouts;
    bool has_depth_stencil = false;
    RenderPassAttachment depth_stencil_attachment;
    VkImageLayout depth_stencil_resolve_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkResolveModeFlagBits depth_resolve_mode = VK_RESOLVE_MODE_NONE;
    VkResolveModeFlagBits stencil_resolve_mode = VK_RESOLVE_MODE_NONE;
    u32 view_mask = 0;

    bool operator==(const RenderPassKey& other) const;
};

/**
 * The render passes a device without dynamic rendering records its passes with, made the first time one is asked
 * for and kept until the device goes. A pass is one subpass, so any two with the same attachment formats, sample
 * counts and view mask are compatible whatever they resolve into (the single subpass case of render pass
 * compatibility), which is what lets a pipeline be built against one and drawn in another.
 *
 * Asked for while command buffers are recorded, which can happen on several threads at once, so it locks.
 */
class RenderPassCache
{
public:
    RenderPassCache() = default;
    RenderPassCache(const RenderPassCache&) = delete;
    RenderPassCache& operator=(const RenderPassCache&) = delete;

    /**
     * The render pass for this key, made now when it is the first time.
     * @return The render pass, or whatever the failing vkCreateRenderPass2 maps to.
     */
    [[nodiscard]] Opal::Expected<VkRenderPass, ErrorCode> Get(VkDevice device, const RenderPassKey& key);

    /** Destroy every render pass made. The device has to be idle, and still alive. */
    void Destroy(VkDevice device);

private:
    struct Entry
    {
        RenderPassKey key;
        VkRenderPass render_pass = VK_NULL_HANDLE;
    };

    std::mutex m_mutex;
    Opal::DynamicArray<Entry> m_entries;
};

}  // namespace Rndr::Forge
