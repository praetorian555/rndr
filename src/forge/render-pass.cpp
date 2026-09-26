#include "render-pass.hpp"

#include "rndr/forge/vulkan-result.hpp"
#include "rndr/log.hpp"

bool Rndr::Forge::RenderPassKey::operator==(const RenderPassKey& other) const
{
    if (view_mask != other.view_mask || has_depth_stencil != other.has_depth_stencil ||
        color_attachments.GetSize() != other.color_attachments.GetSize())
    {
        return false;
    }
    for (i32 i = 0; i < color_attachments.GetSize(); ++i)
    {
        if (!(color_attachments[i] == other.color_attachments[i]) || color_resolve_layouts[i] != other.color_resolve_layouts[i])
        {
            return false;
        }
    }
    if (!has_depth_stencil)
    {
        return true;
    }
    return depth_stencil_attachment == other.depth_stencil_attachment &&
           depth_stencil_resolve_layout == other.depth_stencil_resolve_layout && depth_resolve_mode == other.depth_resolve_mode &&
           stencil_resolve_mode == other.stencil_resolve_mode;
}

namespace
{
VkAttachmentDescription2 ToAttachmentDescription(const Rndr::Forge::RenderPassAttachment& attachment)
{
    return VkAttachmentDescription2{
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        .format = attachment.format,
        .samples = attachment.samples,
        .loadOp = attachment.load_op,
        .storeOp = attachment.store_op,
        .stencilLoadOp = attachment.stencil_load_op,
        .stencilStoreOp = attachment.stencil_store_op,
        // The same layout throughout: the barriers before the pass put the texture in it, the ones after take it
        // out, and the pass itself moves nothing - which is what dynamic rendering does too.
        .initialLayout = attachment.layout,
        .finalLayout = attachment.layout,
    };
}

/** A resolve target: one sample, nothing loaded since every texel is written, and kept. */
VkAttachmentDescription2 ToResolveDescription(VkFormat format, VkImageLayout layout)
{
    return VkAttachmentDescription2{
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        .format = format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = layout,
        .finalLayout = layout,
    };
}

VkAttachmentReference2 ToReference(Rndr::u32 index, VkImageLayout layout)
{
    return VkAttachmentReference2{.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, .attachment = index, .layout = layout};
}
}  // namespace

Opal::Expected<VkRenderPass, Rndr::ErrorCode> Rndr::Forge::RenderPassCache::Get(VkDevice device, const RenderPassKey& key)
{
    using Result = Opal::Expected<VkRenderPass, ErrorCode>;

    const std::lock_guard<std::mutex> lock(m_mutex);
    for (const Entry& entry : m_entries)
    {
        if (entry.key == key)
        {
            return Result(entry.render_pass);
        }
    }

    // In the order the framebuffer names its views: the colour attachments, the colour resolve targets of those
    // that have one, the depth stencil attachment, and its resolve target.
    Opal::DynamicArray<VkAttachmentDescription2> attachments;
    Opal::DynamicArray<VkAttachmentReference2> color_references;
    Opal::DynamicArray<VkAttachmentReference2> resolve_references;
    bool has_color_resolve = false;
    for (i32 i = 0; i < key.color_attachments.GetSize(); ++i)
    {
        color_references.PushBack(ToReference(static_cast<u32>(attachments.GetSize()), key.color_attachments[i].layout));
        attachments.PushBack(ToAttachmentDescription(key.color_attachments[i]));
        has_color_resolve = has_color_resolve || key.color_resolve_layouts[i] != VK_IMAGE_LAYOUT_UNDEFINED;
    }
    for (i32 i = 0; i < key.color_attachments.GetSize(); ++i)
    {
        const VkImageLayout resolve_layout = key.color_resolve_layouts[i];
        if (resolve_layout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            resolve_references.PushBack(ToReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED));
            continue;
        }
        resolve_references.PushBack(ToReference(static_cast<u32>(attachments.GetSize()), resolve_layout));
        attachments.PushBack(ToResolveDescription(key.color_attachments[i].format, resolve_layout));
    }
    VkAttachmentReference2 depth_stencil_reference = ToReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED);
    VkAttachmentReference2 depth_stencil_resolve_reference = ToReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED);
    if (key.has_depth_stencil)
    {
        depth_stencil_reference = ToReference(static_cast<u32>(attachments.GetSize()), key.depth_stencil_attachment.layout);
        attachments.PushBack(ToAttachmentDescription(key.depth_stencil_attachment));
        if (key.depth_stencil_resolve_layout != VK_IMAGE_LAYOUT_UNDEFINED)
        {
            depth_stencil_resolve_reference = ToReference(static_cast<u32>(attachments.GetSize()), key.depth_stencil_resolve_layout);
            attachments.PushBack(ToResolveDescription(key.depth_stencil_attachment.format, key.depth_stencil_resolve_layout));
        }
    }

    const VkSubpassDescriptionDepthStencilResolve depth_stencil_resolve{
        .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE,
        .depthResolveMode = key.depth_resolve_mode,
        .stencilResolveMode = key.stencil_resolve_mode,
        .pDepthStencilResolveAttachment = &depth_stencil_resolve_reference,
    };
    const bool resolves_depth_stencil = depth_stencil_resolve_reference.attachment != VK_ATTACHMENT_UNUSED;
    const VkSubpassDescription2 subpass{
        .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
        .pNext = resolves_depth_stencil ? &depth_stencil_resolve : nullptr,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .viewMask = key.view_mask,
        .colorAttachmentCount = static_cast<u32>(color_references.GetSize()),
        .pColorAttachments = color_references.GetData(),
        .pResolveAttachments = has_color_resolve ? resolve_references.GetData() : nullptr,
        .pDepthStencilAttachment = key.has_depth_stencil ? &depth_stencil_reference : nullptr,
    };
    // No dependencies of its own: the implicit ones order the pass against nothing, since it changes no layout, and
    // the barriers the caller records around it are what synchronize it, as they do around a dynamic rendering pass.
    const VkRenderPassCreateInfo2 create_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
        .attachmentCount = static_cast<u32>(attachments.GetSize()),
        .pAttachments = attachments.GetData(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };
    Entry entry;
    entry.key.color_attachments = key.color_attachments.Clone();
    entry.key.color_resolve_layouts = key.color_resolve_layouts.Clone();
    entry.key.has_depth_stencil = key.has_depth_stencil;
    entry.key.depth_stencil_attachment = key.depth_stencil_attachment;
    entry.key.depth_stencil_resolve_layout = key.depth_stencil_resolve_layout;
    entry.key.depth_resolve_mode = key.depth_resolve_mode;
    entry.key.stencil_resolve_mode = key.stencil_resolve_mode;
    entry.key.view_mask = key.view_mask;
    RNDR_FORGE_VK_CHECK_EXPECTED(vkCreateRenderPass2(device, &create_info, nullptr, &entry.render_pass), "vkCreateRenderPass2", Result);
    const VkRenderPass render_pass = entry.render_pass;
    m_entries.PushBack(std::move(entry));
    return Result(render_pass);
}

void Rndr::Forge::RenderPassCache::Destroy(VkDevice device)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    for (const Entry& entry : m_entries)
    {
        vkDestroyRenderPass(device, entry.render_pass, nullptr);
    }
    m_entries.Clear();
}
