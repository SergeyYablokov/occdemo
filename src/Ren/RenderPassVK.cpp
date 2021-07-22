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

// make sure we can simply cast these
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT == 1, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT == 2, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT == 4, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT == 8, "!");
} // namespace Ren

Ren::RenderPass &Ren::RenderPass::operator=(RenderPass &&rhs) {
    if (this == &rhs) {
        return (*this);
    }

    Destroy();

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    handle_ = exchange(rhs.handle_, {});
    color_rts = std::move(rhs.color_rts);
    depth_rt = exchange(rhs.depth_rt, {});

    return (*this);
}

void Ren::RenderPass::Destroy() {
    if (handle_ != VK_NULL_HANDLE) {
        api_ctx_->render_passes_to_destroy->push_back(handle_);
        handle_ = VK_NULL_HANDLE;
    }
    color_rts.clear();
    depth_rt = {};
}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, const RenderTarget _color_rts[], const int _color_rts_count,
                            const RenderTarget _depth_rt, ILog *log) {
    if (_color_rts_count == color_rts.size() &&
        std::equal(_color_rts, _color_rts + _color_rts_count, color_rts.data(),
                   [](const RenderTarget &rt, const TexHandle &h) {
                       return (!rt.ref && !h) || (rt.ref && rt.ref->handle() == h);
                   }) &&
        ((!_depth_rt.ref && !depth_rt) || (_depth_rt.ref && _depth_rt.ref->handle() == depth_rt))) {
        return true;
    }

    Destroy();

    SmallVector<VkAttachmentDescription, MaxRTAttachments> pass_attachments;
    VkAttachmentReference color_attachment_refs[MaxRTAttachments];
    for (int i = 0; i < MaxRTAttachments; i++) {
        color_attachment_refs[i] = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};
    }
    VkAttachmentReference depth_attachment_ref = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};

    color_rts.resize(_color_rts_count);
    depth_rt = {};

    for (int i = 0; i < _color_rts_count; ++i) {
        if (!_color_rts[i].ref) {
            continue;
        }

        const Ren::Texture2D *tex = _color_rts[i].ref.get();
        color_rts[i] = tex->handle();
        const uint32_t att_index = uint32_t(pass_attachments.size());

        auto &att_desc = pass_attachments.emplace_back();
        att_desc.format = Ren::VKFormatFromTexFormat(tex->params().format);
        att_desc.samples = VkSampleCountFlagBits(tex->params().samples);
        att_desc.loadOp = vk_load_ops[int(_color_rts[i].load)];
        att_desc.storeOp = vk_store_ops[int(_color_rts[i].store)];
        att_desc.stencilLoadOp = vk_load_ops[int(_color_rts[i].load)];
        att_desc.stencilStoreOp = vk_store_ops[int(_color_rts[i].store)];
        att_desc.initialLayout = tex->layout;
        att_desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        color_attachment_refs[i].attachment = att_index;
        color_attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if (_depth_rt.ref) {
        const Ren::Texture2D *tex = _depth_rt.ref.get();
        const uint32_t att_index = uint32_t(pass_attachments.size());

        auto &att_desc = pass_attachments.emplace_back();
        att_desc.format = Ren::VKFormatFromTexFormat(tex->params().format);
        att_desc.samples = VkSampleCountFlagBits(tex->params().samples);
        att_desc.loadOp = vk_load_ops[int(_depth_rt.load)];
        att_desc.storeOp = vk_store_ops[int(_depth_rt.store)];
        att_desc.stencilLoadOp = vk_load_ops[int(_depth_rt.load)];
        att_desc.stencilStoreOp = vk_store_ops[int(_depth_rt.store)];
        att_desc.initialLayout = tex->layout;
        att_desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depth_attachment_ref.attachment = att_index;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depth_rt = _depth_rt.ref->handle();
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = _color_rts_count;
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

    const VkResult res = vkCreateRenderPass(api_ctx->device, &render_pass_create_info, nullptr, &handle_);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create render pass!");
        return false;
    }

    api_ctx_ = api_ctx;

    return true;
}