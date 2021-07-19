#include "RenderPass.h"

#include "VKCtx.h"

namespace Ren {
const VkAttachmentLoadOp vk_load_ops[] = {
    VK_ATTACHMENT_LOAD_OP_LOAD,     // Load
    VK_ATTACHMENT_LOAD_OP_CLEAR,    // Clear
    VK_ATTACHMENT_LOAD_OP_DONT_CARE // DontCare
};
static_assert((sizeof(vk_load_ops) / sizeof(vk_load_ops[0])) == int(eLoadOp::_Count), "!");

const VkAttachmentStoreOp vk_store_ops[] = {
    VK_ATTACHMENT_STORE_OP_STORE,    // Store
    VK_ATTACHMENT_STORE_OP_DONT_CARE // DontCare
};
static_assert((sizeof(vk_store_ops) / sizeof(vk_store_ops[0])) == int(eStoreOp::_Count), "!");
} // namespace Ren

void Ren::RenderPass::Destroy() {
    if (render_pass_ != VK_NULL_HANDLE) {
        api_ctx_->render_passes_to_destroy->push_back(render_pass_);
        render_pass_ = VK_NULL_HANDLE;
    }
    color_attachments_.clear();
}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, const RenderTarget color_rts[], int color_rts_count,
                            RenderTarget depth_rt, ILog *log) {
    if (color_rts_count == color_attachments_.size() &&
        std::equal(color_rts, color_rts + color_rts_count, color_attachments_.data(),
                   [](const RenderTarget &rt, const TexHandle &h) {
                       return (!rt.ref && !h) || (rt.ref && rt.ref->handle() == h);
                   }) &&
        ((!depth_rt.ref && !depth_attachment_) || (depth_rt.ref && depth_rt.ref->handle() == depth_attachment_))) {
        return true;
    }

    Destroy();

    SmallVector<VkAttachmentDescription, MaxRTAttachments> pass_attachments;
    VkAttachmentReference color_attachment_refs[MaxRTAttachments];
    for (int i = 0; i < MaxRTAttachments; i++) {
        color_attachment_refs[i] = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};
    }
    VkAttachmentReference depth_attachment_ref = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};

    color_attachments_.resize(color_rts_count);
    depth_attachment_ = {};

    for (int i = 0; i < color_rts_count; ++i) {
        if (!color_rts[i].ref) {
            continue;
        }

        const Ren::Texture2D *tex = color_rts[i].ref.get();
        color_attachments_[i] = tex->handle();
        const uint32_t att_index = uint32_t(pass_attachments.size());

        auto &att_desc = pass_attachments.emplace_back();
        att_desc.format = Ren::VKFormatFromTexFormat(tex->params().format);
        att_desc.samples = VK_SAMPLE_COUNT_1_BIT;
        att_desc.loadOp = vk_load_ops[int(color_rts[i].load)];
        att_desc.storeOp = vk_store_ops[int(color_rts[i].store)];
        att_desc.stencilLoadOp = vk_load_ops[int(color_rts[i].load)];
        att_desc.stencilStoreOp = vk_store_ops[int(color_rts[i].store)];
        att_desc.initialLayout = tex->layout;
        att_desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        color_attachment_refs[i].attachment = att_index;
        color_attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if (depth_rt.ref) {
        const Ren::Texture2D *tex = depth_rt.ref.get();
        const uint32_t att_index = uint32_t(pass_attachments.size());

        auto &att_desc = pass_attachments.emplace_back();
        att_desc.format = Ren::VKFormatFromTexFormat(tex->params().format);
        att_desc.samples = VK_SAMPLE_COUNT_1_BIT;
        att_desc.loadOp = vk_load_ops[int(depth_rt.load)];
        att_desc.storeOp = vk_store_ops[int(depth_rt.store)];
        att_desc.stencilLoadOp = vk_load_ops[int(depth_rt.load)];
        att_desc.stencilStoreOp = vk_store_ops[int(depth_rt.store)];
        att_desc.initialLayout = tex->layout;
        att_desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depth_attachment_ref.attachment = att_index;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depth_attachment_ = depth_rt.ref->handle();
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = color_rts_count;
    subpass.pColorAttachments = color_attachment_refs;
    if (depth_attachment_ref.attachment != VK_ATTACHMENT_UNUSED) {
        subpass.pDepthStencilAttachment = &depth_attachment_ref;
    }

    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = uint32_t(pass_attachments.size());
    render_pass_create_info.pAttachments = pass_attachments.data();
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;

    const VkResult res = vkCreateRenderPass(api_ctx->device, &render_pass_create_info, nullptr, &render_pass_);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create render pass!");
        return false;
    }

    api_ctx_ = api_ctx;

    return true;
}